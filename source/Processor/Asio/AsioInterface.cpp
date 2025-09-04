#include "AsioInterface.h"
#include "RingBufferFloat.h"
#include "../../Logger.h"
#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <atomic>
#include <xmmintrin.h>
#include <immintrin.h>

// ASIO SDK includes
#include "asiosys.h"
#include "asio.h"
#include "asiodrivers.h"
#include "asiolist.h"

extern AsioDrivers *asioDrivers;
extern bool loadAsioDriver(char *name);

namespace Newkon
{
  // ASIO driver state
  static ASIODriverInfo g_driverInfo = {};
  static ASIOCallbacks g_callbacks = {};
  static ASIOBufferInfo *g_bufferInfos = nullptr;
  static ASIOChannelInfo *g_channelInfos = nullptr;
  static long g_inputChannels = 0;
  static long g_minSize = 0, g_maxSize = 0, g_preferredSize = 0, g_granularity = 0;
  static ASIOSampleRate g_sampleRate = 0.0;

  // ring buffer (mono frames)
  static RingBufferFloat *s_ring = nullptr;
  static int g_bufferPos = 0; // writer index (frames)
  static int g_readPos = 0;   // reader index (frames)

  static volatile bool s_resetRequested = false;
  static volatile bool s_callbacksEnabled = false;
  static volatile int s_inCallback = 0;
  static volatile bool s_resetInProgress = false;

  static const char *asioErrStr(ASIOError e)
  {
    switch (e)
    {
    case ASE_OK:
      return "ASE_OK";
    case ASE_SUCCESS:
      return "ASE_SUCCESS";
    case ASE_NotPresent:
      return "ASE_NotPresent";
    case ASE_HWMalfunction:
      return "ASE_HWMalfunction";
    case ASE_InvalidParameter:
      return "ASE_InvalidParameter";
    case ASE_InvalidMode:
      return "ASE_InvalidMode";
    case ASE_SPNotAdvancing:
      return "ASE_SPNotAdvancing";
    case ASE_NoClock:
      return "ASE_NoClock";
    case ASE_NoMemory:
      return "ASE_NoMemory";
    default:
      return "ASE_Unknown";
    }
  }

  // Precomputed converters per input channel (we use mono = first channel)
  typedef float (*SampleConverter)(const void *src, long index);
  static SampleConverter g_convertSample[2] = {nullptr, nullptr};
  enum SampleKind
  {
    kKindUnknown = 0,
    kKindFloat32,
    kKindInt16,
    kKindInt32,
    kKindInt32LSB24
  };
  static SampleKind g_kind[2] = {kKindUnknown, kKindUnknown};
  static float g_scale[2] = {1.0f, 1.0f};

  static float convFloat32(const void *src, long index)
  {
    const float *pf = static_cast<const float *>(src);
    return pf[index];
  }
  static float convInt32(const void *src, long index)
  {
    const int32_t *pi = static_cast<const int32_t *>(src);
    return static_cast<float>(pi[index]) * (1.0f / 2147483648.0f);
  }
  static float convInt32LSB24(const void *src, long index)
  {
    const int32_t *pi = static_cast<const int32_t *>(src);
    return static_cast<float>(pi[index]) * (1.0f / 8388608.0f);
  }
  static float convInt32LSB20(const void *src, long index)
  {
    const int32_t *pi = static_cast<const int32_t *>(src);
    return static_cast<float>(pi[index]) * (1.0f / 524288.0f);
  }
  static float convInt32LSB18(const void *src, long index)
  {
    const int32_t *pi = static_cast<const int32_t *>(src);
    return static_cast<float>(pi[index]) * (1.0f / 262144.0f);
  }
  static float convInt32LSB16(const void *src, long index)
  {
    const int32_t *pi = static_cast<const int32_t *>(src);
    return static_cast<float>(pi[index]) * (1.0f / 32768.0f);
  }
  static float convInt24(const void *src, long index)
  {
    const unsigned char *p = static_cast<const unsigned char *>(src);
    const int32_t o = index * 3;
    int32_t s = (int32_t)((int32_t)p[o + 0] | ((int32_t)p[o + 1] << 8) | ((int32_t)p[o + 2] << 16));
    if (s & 0x00800000)
      s |= 0xFF000000;
    return (float)s * (1.0f / 8388608.0f);
  }
  static float convInt16(const void *src, long index)
  {
    const int16_t *ps = static_cast<const int16_t *>(src);
    return ps[index] * (1.0f / 32768.0f);
  }
  static int s_selectedInputIndex = -1;

  static void bufferSwitch(long index, ASIOBool /*processNow*/)
  {
    if (!s_callbacksEnabled)
      return;
    if (s_resetInProgress)
      return;
    ++s_inCallback;
    if (!g_bufferInfos || !g_channelInfos || g_preferredSize <= 0 || !s_ring)
    {
      --s_inCallback;
      return;
    }
    if (index != 0 && index != 1)
    {
      --s_inCallback;
      return;
    }

    // Set FTZ/DAZ once on this thread to avoid denormal stalls
    static thread_local bool s_mxcsrInitialized = false;
    if (!s_mxcsrInitialized)
    {
      unsigned int csr = _mm_getcsr();
      csr |= 0x8040; // FTZ (bit 15) | DAZ (bit 6)
      _mm_setcsr(csr);
      s_mxcsrInitialized = true;
    }

    // Always treat as mono source and write mono frames
    const int channelsToUse = 1;
    const int writeBlock = static_cast<int>(g_preferredSize);

    {
      // Mono: convert first input channel only via precomputed converter
      const void *src0 = g_bufferInfos[0].buffers[index];
      if (!src0)
      {
        --s_inCallback;
        return;
      }

      int wpos = g_bufferPos;
      int framesToWrite = static_cast<int>(g_preferredSize);
      const int cap = static_cast<int>(s_ring ? s_ring->capacity() : 0);
      if (wpos < 0 || wpos >= cap)
        wpos = 0;
      if (framesToWrite > cap)
        framesToWrite = cap;
      int contFrames = cap - wpos;
      if (contFrames < 0)
        contFrames = 0;
      int f1 = framesToWrite < contFrames ? framesToWrite : contFrames;

      if (g_kind[0] == kKindFloat32)
      {
        const float *pf = static_cast<const float *>(src0);
        std::memcpy(s_ring->data() + wpos, pf, sizeof(float) * f1);
        wpos += f1;
        if (wpos == (int)s_ring->capacity())
          wpos = 0;
        framesToWrite -= f1;
        if (framesToWrite > 0)
        {
          std::memcpy(s_ring->data(), pf + f1, sizeof(float) * framesToWrite);
          wpos = framesToWrite;
        }
      }
      else if (g_kind[0] == kKindInt16)
      {
        const int16_t *ps = static_cast<const int16_t *>(src0);
        const float s = g_scale[0];
        int i = 0;
        int n = f1 & ~15; // 16 samples per iter
        const __m256 scale = _mm256_set1_ps(s);
        for (; i < n; i += 16)
        {
          __m128i v16a = _mm_loadu_si128((const __m128i *)(ps + i));
          __m128i v16b = _mm_loadu_si128((const __m128i *)(ps + i + 8));
          __m256i v32a = _mm256_cvtepi16_epi32(v16a);
          __m256i v32b = _mm256_cvtepi16_epi32(v16b);
          __m256 fa = _mm256_mul_ps(_mm256_cvtepi32_ps(v32a), scale);
          __m256 fb = _mm256_mul_ps(_mm256_cvtepi32_ps(v32b), scale);
          _mm256_storeu_ps(s_ring->data() + wpos + i, fa);
          _mm256_storeu_ps(s_ring->data() + wpos + i + 8, fb);
        }
        for (; i < f1; i++)
          s_ring->data()[wpos + i] = ps[i] * s;
        wpos += f1;
        if (wpos == (int)s_ring->capacity())
          wpos = 0;
        framesToWrite -= f1;
        if (framesToWrite > 0)
        {
          int j = 0;
          int m = framesToWrite & ~15;
          for (; j < m; j += 16)
          {
            __m128i v16a = _mm_loadu_si128((const __m128i *)(ps + f1 + j));
            __m128i v16b = _mm_loadu_si128((const __m128i *)(ps + f1 + j + 8));
            __m256i v32a = _mm256_cvtepi16_epi32(v16a);
            __m256i v32b = _mm256_cvtepi16_epi32(v16b);
            __m256 fa = _mm256_mul_ps(_mm256_cvtepi32_ps(v32a), scale);
            __m256 fb = _mm256_mul_ps(_mm256_cvtepi32_ps(v32b), scale);
            _mm256_storeu_ps(s_ring->data() + j, fa);
            _mm256_storeu_ps(s_ring->data() + j + 8, fb);
          }
          for (; j < framesToWrite; j++)
            s_ring->data()[j] = ps[f1 + j] * s;
          wpos = framesToWrite;
        }
      }
      else if (g_kind[0] == kKindInt32 || g_kind[0] == kKindInt32LSB24)
      {
        const int32_t *pi = static_cast<const int32_t *>(src0);
        const float s = g_scale[0];
        int i = 0;
        int n = f1 & ~7; // 8 samples per iter
        const __m256 scale = _mm256_set1_ps(s);
        for (; i < n; i += 8)
        {
          __m256i v = _mm256_loadu_si256((const __m256i *)(pi + i));
          __m256 f = _mm256_mul_ps(_mm256_cvtepi32_ps(v), scale);
          _mm256_storeu_ps(s_ring->data() + wpos + i, f);
        }
        for (; i < f1; i++)
          s_ring->data()[wpos + i] = pi[i] * s;
        wpos += f1;
        if (wpos == (int)s_ring->capacity())
          wpos = 0;
        framesToWrite -= f1;
        if (framesToWrite > 0)
        {
          int j = 0;
          int m = framesToWrite & ~7;
          for (; j < m; j += 8)
          {
            __m256i v = _mm256_loadu_si256((const __m256i *)(pi + f1 + j));
            __m256 f = _mm256_mul_ps(_mm256_cvtepi32_ps(v), scale);
            _mm256_storeu_ps(s_ring->data() + j, f);
          }
          for (; j < framesToWrite; j++)
            s_ring->data()[j] = pi[f1 + j] * s;
          wpos = framesToWrite;
        }
      }
      else
      {
        for (int i = 0; i < f1; i++)
          s_ring->data()[wpos + i] = g_convertSample[0] ? g_convertSample[0](src0, i) : 0.0f;
        wpos += f1;
        if (wpos == (int)s_ring->capacity())
          wpos = 0;
        framesToWrite -= f1;
        if (framesToWrite > 0)
        {
          for (int i = 0; i < framesToWrite; i++)
            s_ring->data()[i] = g_convertSample[0] ? g_convertSample[0](src0, f1 + i) : 0.0f;
          wpos = framesToWrite;
        }
      }

      g_bufferPos = wpos;
    }
    --s_inCallback;
  }

  static ASIOTime *bufferSwitchTimeInfo(ASIOTime *timeInfo, long index, ASIOBool processNow)
  {
    bufferSwitch(index, processNow);
    return timeInfo;
  }

  static void sampleRateDidChange(ASIOSampleRate sRate)
  {
    g_sampleRate = sRate;
  }

  static long asioMessage(long selector, long value, void *message, double *opt)
  {
    switch (selector)
    {
    case kAsioSelectorSupported:
      return 1;
    case kAsioEngineVersion:
      return 2;
    case kAsioResetRequest:
      s_resetRequested = true;
      s_resetInProgress = true;
      Logger::getInstance() << "ASIO reset requested by driver" << std::endl;
      return 1;
    case kAsioResyncRequest:
      // Latency or buffer size may have changed; handle like reset
      s_resetRequested = true;
      s_resetInProgress = true;
      Logger::getInstance() << "ASIO resync requested by driver" << std::endl;
      return 1;
    case kAsioLatenciesChanged:
      // Not all drivers use ResetRequest; be safe and rebuild buffers
      s_resetRequested = true;
      s_resetInProgress = true;
      Logger::getInstance() << "ASIO latencies changed" << std::endl;
      return 1;
    default:
      return 0;
    }
  }

  AsioInterface::AsioInterface() : currentInterfaceIndex(-1), currentInputIndex(-1), isStreaming(false) {}

  AsioInterface::~AsioInterface()
  {
    shutdown();
  }

  void AsioInterface::handlePendingReset()
  {
    if (!s_resetRequested)
      return;
    s_resetRequested = false;

    if (currentInterfaceIndex < 0 || currentInputIndex < 0)
      return;

    // Mark reset in progress to make all readers/writers no-op
    s_resetInProgress = true;

    // ASIO-recommended reset: do not unload/reload driver; just rebuild buffers
    const long oldPreferred = g_preferredSize;
    stopAudioStream();
    Sleep(10);

    long minS = 0, maxS = 0, prefS = 0, gran = 0;
    ASIOError bsRc = ASIOGetBufferSize(&minS, &maxS, &prefS, &gran);
    if (bsRc != ASE_OK)
    {
      Logger::getInstance() << "ASIOGetBufferSize after reset failed: " << asioErrStr(bsRc) << std::endl;
      return;
    }
    g_minSize = minS;
    g_maxSize = maxS;
    g_preferredSize = prefS;
    g_granularity = gran;

    ASIOSampleRate sr = 0.0;
    ASIOError srRc = ASIOGetSampleRate(&sr);
    if (srRc == ASE_OK)
      g_sampleRate = sr;

    // Recreate buffers and restart. Retry a few times if start fails.
    bool started = false;
    for (int attempt = 0; attempt < 3 && !started; ++attempt)
    {
      if (startAudioStream())
      {
        started = true;
        break;
      }
      Sleep(10);
    }
    if (!started)
    {
      Logger::getInstance() << "ASIO reset error: start stream failed" << std::endl;
      // keep s_resetInProgress true so readers stay silent
      return;
    }
    if (g_preferredSize != oldPreferred)
    {
      Logger::getInstance() << "ASIO buffer size changed: " << oldPreferred << " -> " << g_preferredSize << std::endl;
    }

    // Reset completed; allow readers/writers to resume
    s_resetInProgress = false;
  }

  std::vector<std::string> AsioInterface::listAsioInterfaces()
  {
    asioDevices.clear();
    std::vector<std::string> deviceNames;

    AsioDriverList list;
    LPASIODRVSTRUCT p = list.lpdrvlist;
    int idx = 0;
    while (p)
    {
      AsioInterfaceInfo info;
      info.name = p->drvname;
      info.deviceIndex = idx;
      info.isDefault = (idx == 0);
      asioDevices.push_back(info);
      deviceNames.push_back(info.name);
      p = p->next;
      idx++;
    }

    Logger::getInstance() << "ASIO interfaces scan success: " << deviceNames.size() << " found" << std::endl;
    return deviceNames;
  }

  bool AsioInterface::connectToInterface(int deviceIndex)
  {
    if (deviceIndex < 0 || deviceIndex >= static_cast<int>(asioDevices.size()))
    {
      return false;
    }

    // If already connected to an interface, log disconnect
    if (currentInterfaceIndex >= 0 && currentInterfaceIndex < static_cast<int>(asioDevices.size()) && currentInterfaceIndex != deviceIndex)
    {
      Logger::getInstance() << "ASIO interface disconnect: " << asioDevices[currentInterfaceIndex].name << std::endl;
    }

    stopAudioStream();
    ASIOExit();

    Logger::getInstance() << "ASIO interface connect: " << asioDevices[deviceIndex].name << std::endl;

    currentInterfaceIndex = deviceIndex;

    char name[256] = {0};
    std::snprintf(name, sizeof(name), "%s", asioDevices[deviceIndex].name.c_str());
    bool loaded = false;
    for (int attempt = 0; attempt < 5 && !loaded; ++attempt)
    {
      if (loadAsioDriver(name))
      {
        loaded = true;
        break;
      }
      Sleep(10);
    }
    if (!loaded)
    {
      Logger::getInstance() << "loadAsioDriver failed for '" << name << "'" << std::endl;
      currentInterfaceIndex = -1;
      return false;
    }

    std::memset(&g_driverInfo, 0, sizeof(g_driverInfo));
    g_driverInfo.sysRef = (void *)GetDesktopWindow();
    ASIOError initRc = ASIOInit(&g_driverInfo);
    if (initRc != ASE_OK)
    {
      Logger::getInstance() << "ASIOInit failed: " << asioErrStr(initRc)
                            << ", msg='" << (g_driverInfo.errorMessage ? g_driverInfo.errorMessage : "") << "'" << std::endl;
      ASIOExit();
      currentInterfaceIndex = -1;
      return false;
    }

    long dummyOutputs = 0;
    ASIOError chRc = ASIOGetChannels(&g_inputChannels, &dummyOutputs);
    if (chRc != ASE_OK)
    {
      Logger::getInstance() << "ASIOGetChannels failed: " << asioErrStr(chRc) << std::endl;
      return false;
    }
    ASIOError bsRc = ASIOGetBufferSize(&g_minSize, &g_maxSize, &g_preferredSize, &g_granularity);
    if (bsRc != ASE_OK)
    {
      Logger::getInstance() << "ASIOGetBufferSize failed: " << asioErrStr(bsRc) << std::endl;
      return false;
    }
    ASIOError srRc = ASIOGetSampleRate(&g_sampleRate);
    if (srRc != ASE_OK)
    {
      Logger::getInstance() << "ASIOGetSampleRate failed: " << asioErrStr(srRc) << ", defaulting to 44100" << std::endl;
      g_sampleRate = 44100.0;
    }

    Logger::getInstance() << "ASIO interface connected: " << asioDevices[deviceIndex].name
                          << ", inputs=" << g_inputChannels
                          << ", preferredBuffer=" << g_preferredSize
                          << ", sampleRate=" << g_sampleRate << std::endl;

    return true;
  }

  std::vector<std::string> AsioInterface::getAsioInputs(int deviceIndex)
  {
    std::vector<std::string> inputs;

    if (deviceIndex < 0 || deviceIndex >= static_cast<int>(asioDevices.size()))
    {
      return inputs;
    }

    if (currentInterfaceIndex != deviceIndex)
    {
      if (!connectToInterface(deviceIndex))
        return inputs;
    }

    inputs.reserve(static_cast<size_t>(g_inputChannels));
    for (long i = 0; i < g_inputChannels; i++)
    {
      ASIOChannelInfo ci = {};
      ci.channel = i;
      ci.isInput = ASIOTrue;
      if (ASIOGetChannelInfo(&ci) == ASE_OK && ci.name[0] != '\0')
        inputs.emplace_back(ci.name);
      else
        inputs.emplace_back("Input " + std::to_string(i + 1));
    }

    return inputs;
  }

  const std::vector<AsioInterfaceInfo> &AsioInterface::getAsioDevices()
  {
    return asioDevices;
  }

  bool AsioInterface::connectToInput(int inputIndex)
  {
    if (currentInterfaceIndex < 0 || currentInterfaceIndex >= static_cast<int>(asioDevices.size()))
    {
      return false;
    }

    if (inputIndex < 0)
    {
      return false;
    }

    // If switching inputs, log disconnect
    if (currentInputIndex != -1 && currentInputIndex != inputIndex)
    {
      Logger::getInstance() << "ASIO input disconnect: index " << currentInputIndex << std::endl;
    }
    currentInputIndex = inputIndex;
    s_selectedInputIndex = inputIndex;
    Logger::getInstance() << "ASIO input connect: index " << inputIndex << std::endl;
    return true;
  }

  bool AsioInterface::startAudioStream()
  {
    if (currentInterfaceIndex < 0 || currentInputIndex < 0)
    {
      return false;
    }

    // Prevent readers while (re)creating buffers
    s_resetInProgress = true;

    if (isStreaming)
    {
      stopAudioStream();
    }

    long available2 = g_inputChannels - AsioInterface::currentInputIndex;
    if (available2 < 0)
      available2 = 0;
    const int channelsToUse = static_cast<int>(available2 < 2 ? available2 : 2);
    if (channelsToUse <= 0)
      return false;

    g_callbacks.bufferSwitch = bufferSwitch;
    g_callbacks.bufferSwitchTimeInfo = bufferSwitchTimeInfo;
    g_callbacks.asioMessage = asioMessage;
    g_callbacks.sampleRateDidChange = sampleRateDidChange;

    if (g_bufferInfos)
    {
      ASIODisposeBuffers();
      delete[] g_bufferInfos;
      g_bufferInfos = nullptr;
    }

    g_bufferInfos = new ASIOBufferInfo[channelsToUse];
    for (int i = 0; i < channelsToUse; i++)
    {
      g_bufferInfos[i].isInput = ASIOTrue;
      g_bufferInfos[i].channelNum = s_selectedInputIndex + i;
      g_bufferInfos[i].buffers[0] = g_bufferInfos[i].buffers[1] = nullptr;
    }

    {
      ASIOError cr = ASIOCreateBuffers(g_bufferInfos, channelsToUse, g_preferredSize, &g_callbacks);
      if (cr != ASE_OK)
      {
        Logger::getInstance() << "ASIOCreateBuffers failed: " << asioErrStr(cr) << std::endl;
        return false;
      }
    }

    if (g_channelInfos)
    {
      delete[] g_channelInfos;
      g_channelInfos = nullptr;
    }
    g_channelInfos = new ASIOChannelInfo[channelsToUse];
    for (int i = 0; i < channelsToUse; i++)
    {
      g_channelInfos[i].channel = s_selectedInputIndex + i;
      g_channelInfos[i].isInput = ASIOTrue;
      ASIOGetChannelInfo(&g_channelInfos[i]);
    }

    // Sanity check: preferred size should be > 0 and <= max
    if (g_preferredSize <= 0 || (g_maxSize > 0 && g_preferredSize > g_maxSize))
    {
      Logger::getInstance() << "Invalid preferred buffer size: " << g_preferredSize << " (max=" << g_maxSize << ")" << std::endl;
      return false;
    }

    // Precompute converters (mono path: index 0)
    for (int i = 0; i < channelsToUse && i < 2; i++)
    {
      switch (g_channelInfos[i].type)
      {
      case ASIOSTFloat32LSB:
        g_convertSample[i] = convFloat32;
        g_kind[i] = kKindFloat32;
        g_scale[i] = 1.0f;
        break;
      case ASIOSTInt32LSB:
        g_convertSample[i] = convInt32;
        g_kind[i] = kKindInt32;
        g_scale[i] = (1.0f / 2147483648.0f);
        break;
      case ASIOSTInt32LSB24:
        g_convertSample[i] = convInt32LSB24;
        g_kind[i] = kKindInt32LSB24;
        g_scale[i] = (1.0f / 8388608.0f);
        break;
      case ASIOSTInt32LSB20:
        g_convertSample[i] = convInt32LSB20;
        g_kind[i] = kKindUnknown;
        g_scale[i] = 1.0f;
        break;
      case ASIOSTInt32LSB18:
        g_convertSample[i] = convInt32LSB18;
        g_kind[i] = kKindUnknown;
        g_scale[i] = 1.0f;
        break;
      case ASIOSTInt32LSB16:
        g_convertSample[i] = convInt32LSB16;
        g_kind[i] = kKindUnknown;
        g_scale[i] = 1.0f;
        break;
      case ASIOSTInt24LSB:
        g_convertSample[i] = convInt24;
        g_kind[i] = kKindUnknown;
        g_scale[i] = 1.0f;
        break;
      case ASIOSTInt16LSB:
        g_convertSample[i] = convInt16;
        g_kind[i] = kKindInt16;
        g_scale[i] = (1.0f / 32768.0f);
        break;
      default:
        g_convertSample[i] = nullptr;
        g_kind[i] = kKindUnknown;
        g_scale[i] = 1.0f;
        break;
      }
    }

    if (s_ring)
    {
      delete s_ring;
      s_ring = nullptr;
    }
    // ring buffer length: 100 ms mono or 8 blocks, whichever is larger, rounded to power of two
    const int minFrames100ms = (g_sampleRate > 0 ? static_cast<int>(g_sampleRate * 0.1) : g_preferredSize * 8);
    const int minFrames = (minFrames100ms > (g_preferredSize * 8) ? minFrames100ms : (g_preferredSize * 8));
    int pow2 = 1;
    while (pow2 < minFrames)
      pow2 <<= 1;
    // Double capacity for extra headroom against jitter/underruns
    pow2 <<= 1;
    s_ring = new RingBufferFloat(static_cast<uint32_t>(pow2));
    g_bufferPos = 0;
    g_readPos = 0;

    s_callbacksEnabled = false;
    {
      ASIOError sr = ASIOStart();
      if (sr != ASE_OK)
      {
        Logger::getInstance() << "ASIOStart failed: " << asioErrStr(sr) << std::endl;
        return false;
      }
    }

    isStreaming = true;
    Logger::getInstance() << "ASIO stream started for interface: " << asioDevices[currentInterfaceIndex].name
                          << ", input index: " << currentInputIndex
                          << ", preferred buffer: " << g_preferredSize << std::endl;
    s_callbacksEnabled = true;
    s_resetInProgress = false;
    return true;
  }

  void AsioInterface::stopAudioStream()
  {
    if (!isStreaming)
      return;
    s_resetInProgress = true;
    s_callbacksEnabled = false;
    Logger::getInstance() << "ASIO stream stopping" << std::endl;
    ASIOStop();
    // Wait for any in-flight callback to finish before freeing buffers
    for (int spins = 0; spins < 10000; ++spins)
    {
      if (s_inCallback == 0)
        break;
      _mm_pause();
    }
    ASIODisposeBuffers();
    if (g_bufferInfos)
    {
      delete[] g_bufferInfos;
      g_bufferInfos = nullptr;
    }
    if (g_channelInfos)
    {
      delete[] g_channelInfos;
      g_channelInfos = nullptr;
    }
    if (s_ring)
    {
      delete s_ring;
      s_ring = nullptr;
    }
    g_bufferPos = 0;
    g_readPos = 0;
    isStreaming = false;
    Logger::getInstance() << "ASIO stream stopped" << std::endl;
  }

  void AsioInterface::shutdown()
  {
    // Ensure streaming is stopped and buffers are freed
    if (isStreaming)
      stopAudioStream();

    if (g_bufferInfos)
    {
      delete[] g_bufferInfos;
      g_bufferInfos = nullptr;
    }
    if (g_channelInfos)
    {
      delete[] g_channelInfos;
      g_channelInfos = nullptr;
    }
    if (s_ring)
    {
      delete s_ring;
      s_ring = nullptr;
    }
    g_bufferPos = 0;
    g_readPos = 0;

    // Tell driver we are done
    ASIODisposeBuffers();
    ASIOExit();
    currentInterfaceIndex = -1;
    currentInputIndex = -1;
  }

  bool AsioInterface::getAudioData(float *__restrict outputBuffer, int numSamples, int numChannels)
  {
    if (s_resetInProgress)
    {
      if (outputBuffer)
        std::memset(outputBuffer, 0, numSamples * numChannels * sizeof(float));
      return false;
    }
    if (!isStreaming || currentInterfaceIndex < 0 || currentInputIndex < 0 || !s_ring)
    {
      if (outputBuffer)
      {
        memset(outputBuffer, 0, numSamples * numChannels * sizeof(float));
      }
      return false;
    }
    const int total = numSamples * numChannels;
    if (total <= 0)
      return false;

    // Ensure read cursor is within bounds
    if (g_readPos < 0 || g_readPos >= (int)s_ring->capacity())
      g_readPos = 0;

    // Compute available frames (mono) and clamp reads to avoid underrun
    const int wposNow = g_bufferPos;
    const int mask = static_cast<int>(s_ring->mask());
    int available = (wposNow - g_readPos) & mask;

    int toRead = total < available ? total : available;
    for (int i = 0; i < toRead; i++)
    {
      outputBuffer[i] = s_ring->data()[g_readPos];
      g_readPos = (g_readPos + 1) & mask;
    }
    if (toRead < total)
    {
      Logger::getInstance() << "Underrun: requested " << total << ", available " << available << std::endl;
      std::memset(outputBuffer + toRead, 0, sizeof(float) * (total - toRead));
    }

    return true;
  }

  bool AsioInterface::getAudioDataStereo(float *__restrict outL, float *__restrict outR, int numSamples)
  {
    if (s_resetInProgress)
      return false;
    if (!isStreaming || currentInterfaceIndex < 0 || currentInputIndex < 0 || !s_ring || !outL || !outR)
      return false;

    if (numSamples <= 0)
      return false;

    // Ensure read cursor is within bounds
    if (g_readPos < 0 || g_readPos >= (int)s_ring->capacity())
      g_readPos = 0;

    // Compute available frames (mono) and clamp reads to avoid underrun
    const int wposNow = g_bufferPos;
    const int mask = static_cast<int>(s_ring->mask());
    int available = (wposNow - g_readPos) & mask;
    int framesToRead = numSamples < available ? numSamples : available;

    // Ring is mono frames; duplicate to L/R with contiguous fast path
    int framesLeft = framesToRead;
    int contFrames = static_cast<int>(s_ring->capacity()) - g_readPos;
    int f1 = framesLeft < contFrames ? framesLeft : contFrames;
    for (int i = 0; i < f1; i++)
    {
      float v = s_ring->data()[g_readPos++];
      outL[i] = v;
      outR[i] = v;
    }
    framesLeft -= f1;
    if (framesLeft > 0)
    {
      g_readPos = 0;
      int base = f1;
      for (int i = 0; i < framesLeft; i++)
      {
        float v = s_ring->data()[g_readPos++];
        outL[base + i] = v;
        outR[base + i] = v;
      }
    }
    if (framesToRead < numSamples)
    {
      Logger::getInstance() << "Underrun: requested " << numSamples << ", available " << available << std::endl;
      std::memset(outL + framesToRead, 0, sizeof(float) * (numSamples - framesToRead));
      std::memset(outR + framesToRead, 0, sizeof(float) * (numSamples - framesToRead));
    }

    return true;
  }

  int AsioInterface::availableFrames()
  {
    if (s_resetInProgress)
      return 0;
    if (!s_ring)
      return 0;
    const int mask = static_cast<int>(s_ring->mask());
    const int wposNow = g_bufferPos;
    return (wposNow - g_readPos) & mask;
  }

  bool AsioInterface::isConnectedAndStreaming()
  {
    return isStreaming && currentInterfaceIndex >= 0 && currentInputIndex >= 0 && s_ring != nullptr;
  }
}
