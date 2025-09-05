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
#include "AsioConverters.h"

// ASIO SDK includes
#include "asiosys.h"
#include "asio.h"
#include "asiodrivers.h"
#include "asiolist.h"

extern AsioDrivers *asioDrivers;
extern bool loadAsioDriver(char *name);

namespace Newkon
{
  struct AsioState
  {
    ASIODriverInfo driverInfo{};
    ASIOCallbacks callbacks{};
    ASIOBufferInfo *bufferInfos = nullptr;
    ASIOChannelInfo *channelInfos = nullptr;
    long inputChannels = 0;
    long minSize = 0;
    long maxSize = 0;
    long preferredSize = 0;
    long granularity = 0;
    ASIOSampleRate sampleRate = 0.0;
    volatile bool callbacksEnabled = false;
    volatile int activeCallbackCount = 0;
    int selectedInputIndex = -1;
    // converters (mono path uses index 0)
    typedef float (*SampleConverter)(const void *src, long index);
    SampleConverter convertSample[2] = {nullptr, nullptr};
    enum SampleKind
    {
      kKindUnknown = 0,
      kKindFloat32,
      kKindInt16,
      kKindInt32,
      kKindInt32LSB24
    };
    SampleKind kind[2] = {kKindUnknown, kKindUnknown};
    float scale[2] = {1.0f, 1.0f};
  };

  AsioInterface *AsioInterface::s_current = nullptr;

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

  void AsioInterface::bufferSwitchThunk(long index, ASIOBool /*processNow*/)
  {
    AsioInterface *self = AsioInterface::s_current;
    if (!self || !self->state)
      return;
    AsioState *st = self->state;
    if (!st->callbacksEnabled)
      return;
    ++st->activeCallbackCount;
    if (!st->bufferInfos || !st->channelInfos || st->preferredSize <= 0)
    {
      --st->activeCallbackCount;
      return;
    }
    if (index != 0 && index != 1)
    {
      --st->activeCallbackCount;
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
    const int writeBlock = static_cast<int>(st->preferredSize);

    {
      // Mono: convert first input channel only via precomputed converter
      const void *src0 = st->bufferInfos[0].buffers[index];
      if (!src0)
      {
        --st->activeCallbackCount;
        return;
      }

      uint32_t wpos = self->ringBuffer.getWritePos();
      int framesToWrite = static_cast<int>(st->preferredSize);
      const uint32_t cap = self->ringBuffer.capacity();
      if (wpos < 0 || wpos >= cap)
        wpos = 0;
      if (framesToWrite > (int)cap)
        framesToWrite = (int)cap;
      int contFrames = (int)cap - (int)wpos;
      if (contFrames < 0)
        contFrames = 0;
      int f1 = framesToWrite < contFrames ? framesToWrite : contFrames;
      const int totalToWrite = framesToWrite;

      if (st->kind[0] == AsioState::kKindFloat32)
      {
        const float *pf = static_cast<const float *>(src0);
        std::memcpy(self->ringBuffer.data() + wpos, pf, sizeof(float) * f1);
        wpos += f1;
        if (wpos == self->ringBuffer.capacity())
          wpos = 0;
        framesToWrite -= f1;
        if (framesToWrite > 0)
        {
          std::memcpy(self->ringBuffer.data(), pf + f1, sizeof(float) * framesToWrite);
          wpos = framesToWrite;
        }
      }
      else if (st->kind[0] == AsioState::kKindInt16)
      {
        const int16_t *ps = static_cast<const int16_t *>(src0);
        const float s = st->scale[0];
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
          _mm256_storeu_ps(self->ringBuffer.data() + wpos + i, fa);
          _mm256_storeu_ps(self->ringBuffer.data() + wpos + i + 8, fb);
        }
        for (; i < f1; i++)
          self->ringBuffer.data()[wpos + i] = ps[i] * s;
        wpos += f1;
        if (wpos == self->ringBuffer.capacity())
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
            _mm256_storeu_ps(self->ringBuffer.data() + j, fa);
            _mm256_storeu_ps(self->ringBuffer.data() + j + 8, fb);
          }
          for (; j < framesToWrite; j++)
            self->ringBuffer.data()[j] = ps[f1 + j] * s;
          wpos = framesToWrite;
        }
      }
      else if (st->kind[0] == AsioState::kKindInt32 || st->kind[0] == AsioState::kKindInt32LSB24)
      {
        const int32_t *pi = static_cast<const int32_t *>(src0);
        const float s = st->scale[0];
        int i = 0;
        int n = f1 & ~7; // 8 samples per iter
        const __m256 scale = _mm256_set1_ps(s);
        for (; i < n; i += 8)
        {
          __m256i v = _mm256_loadu_si256((const __m256i *)(pi + i));
          __m256 f = _mm256_mul_ps(_mm256_cvtepi32_ps(v), scale);
          _mm256_storeu_ps(self->ringBuffer.data() + wpos + i, f);
        }
        for (; i < f1; i++)
          self->ringBuffer.data()[wpos + i] = pi[i] * s;
        wpos += f1;
        if (wpos == self->ringBuffer.capacity())
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
            _mm256_storeu_ps(self->ringBuffer.data() + j, f);
          }
          for (; j < framesToWrite; j++)
            self->ringBuffer.data()[j] = pi[f1 + j] * s;
          wpos = framesToWrite;
        }
      }
      else
      {
        for (int i = 0; i < f1; i++)
          self->ringBuffer.data()[wpos + i] = st->convertSample[0] ? st->convertSample[0](src0, i) : 0.0f;
        wpos += f1;
        if (wpos == self->ringBuffer.capacity())
          wpos = 0;
        framesToWrite -= f1;
        if (framesToWrite > 0)
        {
          for (int i = 0; i < framesToWrite; i++)
            self->ringBuffer.data()[i] = st->convertSample[0] ? st->convertSample[0](src0, f1 + i) : 0.0f;
          wpos = framesToWrite;
        }
      }
      // Advance writer head by total written frames
      self->ringBuffer.advanceWrite((uint32_t)totalToWrite);
    }
    --st->activeCallbackCount;
  }

  ASIOTime *AsioInterface::bufferSwitchTimeInfoThunk(ASIOTime *timeInfo, long index, ASIOBool processNow)
  {
    AsioInterface::bufferSwitchThunk(index, processNow);
    return timeInfo;
  }

  void AsioInterface::sampleRateDidChangeThunk(ASIOSampleRate sRate)
  {
    AsioInterface *self = AsioInterface::s_current;
    if (!self || !self->state)
      return;
    self->state->sampleRate = sRate;
  }

  long AsioInterface::asioMessageThunk(long selector, long value, void *message, double *opt)
  {
    AsioInterface *self = AsioInterface::s_current;
    if (!self || !self->state)
      return 0;
    AsioState *st = self->state;
    switch (selector)
    {
    case kAsioSelectorSupported:
      return 1;
    case kAsioEngineVersion:
      return 2;
    case kAsioResetRequest:
      self->handlePendingReset();
      return 1;
    case kAsioResyncRequest:
      self->handlePendingReset();
      return 1;
    case kAsioLatenciesChanged:
      self->handlePendingReset();
      return 1;
    default:
      return 0;
    }
  }

  void AsioInterface::handlePendingReset()
  {
    if (!state)
      return;

    // Pause producer work and wait for any in-flight callback to finish
    state->callbacksEnabled = false;
    for (int spins = 0; spins < 10000; ++spins)
    {
      if (state->activeCallbackCount == 0)
        break;
      _mm_pause();
    }

    // Re-query buffer sizes and sample rate from driver
    long minS = state->minSize, maxS = state->maxSize, prefS = state->preferredSize, gran = state->granularity;
    ASIOError bsRc = ASIOGetBufferSize(&minS, &maxS, &prefS, &gran);
    if (bsRc == ASE_OK)
    {
      // state->minSize = minS;
      // state->maxSize = maxS;
      // state->preferredSize = prefS;
      // state->granularity = gran;
    }

    ASIOSampleRate sr = 0.0;
    if (ASIOGetSampleRate(&sr) == ASE_OK)
      state->sampleRate = sr;

    // Recompute ring capacity based on new timing
    const int minFrames100ms = (state->sampleRate > 0 ? static_cast<int>(state->sampleRate * 0.1) : state->preferredSize * 8);
    const int minFrames = (minFrames100ms > (state->preferredSize * 8) ? minFrames100ms : (state->preferredSize * 8));
    int pow2 = 1;
    while (pow2 < minFrames)
      pow2 <<= 1;
    pow2 <<= 1; // extra headroom

    ringBuffer.resize(static_cast<uint32_t>(pow2));
    Logger::getInstance() << "ASIO reset applied: preferred=" << state->preferredSize
                          << ", sr=" << state->sampleRate
                          << ", ringCapacity=" << ringBuffer.capacity() << std::endl;

    // Resume producer
    state->callbacksEnabled = true;
  }

  AsioInterface::AsioInterface() : currentInterfaceIndex(-1), currentInputIndex(-1), isStreaming(false), ringBuffer(1)
  {
    state = new AsioState();
  }

  AsioInterface::~AsioInterface()
  {
    shutdown();
    delete state;
    state = nullptr;
    if (AsioInterface::s_current == this)
      AsioInterface::s_current = nullptr;
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

    Logger::getInstance() << "Stopping audio stream called from connectToInterface" << std::endl;
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

    std::memset(&state->driverInfo, 0, sizeof(state->driverInfo));
    state->driverInfo.sysRef = (void *)GetDesktopWindow();
    ASIOError initRc = ASIOInit(&state->driverInfo);
    if (initRc != ASE_OK)
    {
      Logger::getInstance() << "ASIOInit failed: " << asioErrStr(initRc)
                            << ", msg='" << (state->driverInfo.errorMessage ? state->driverInfo.errorMessage : "") << "'" << std::endl;
      ASIOExit();
      currentInterfaceIndex = -1;
      return false;
    }

    long dummyOutputs = 0;
    ASIOError chRc = ASIOGetChannels(&state->inputChannels, &dummyOutputs);
    if (chRc != ASE_OK)
    {
      Logger::getInstance() << "ASIOGetChannels failed: " << asioErrStr(chRc) << std::endl;
      ASIOExit();
      currentInterfaceIndex = -1;
      return false;
    }
    ASIOError bsRc = ASIOGetBufferSize(&state->minSize, &state->maxSize, &state->preferredSize, &state->granularity);
    if (bsRc != ASE_OK)
    {
      Logger::getInstance() << "ASIOGetBufferSize failed: " << asioErrStr(bsRc) << std::endl;
      ASIOExit();
      currentInterfaceIndex = -1;
      return false;
    }
    ASIOError srRc = ASIOGetSampleRate(&state->sampleRate);
    if (srRc != ASE_OK)
    {
      Logger::getInstance() << "ASIOGetSampleRate failed: " << asioErrStr(srRc) << ", defaulting to 44100" << std::endl;
      state->sampleRate = 44100.0;
    }

    Logger::getInstance() << "ASIO interface connected: " << asioDevices[deviceIndex].name
                          << ", inputs=" << state->inputChannels
                          << ", preferredBuffer=" << state->preferredSize
                          << ", sampleRate=" << state->sampleRate << std::endl;

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

    inputs.reserve(static_cast<size_t>(state->inputChannels));
    for (long i = 0; i < state->inputChannels; i++)
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
    state->selectedInputIndex = inputIndex;
    Logger::getInstance() << "ASIO input connect: index " << inputIndex << std::endl;
    return true;
  }

  bool AsioInterface::startAudioStream()
  {
    if (currentInterfaceIndex < 0 || currentInputIndex < 0)
    {
      return false;
    }

    if (isStreaming)
    {
      Logger::getInstance() << "Stopping audio stream called from startAudioStream" << std::endl;
      stopAudioStream();
    }

    long available2 = state->inputChannels - AsioInterface::currentInputIndex;
    if (available2 < 0)
      available2 = 0;
    const int channelsToUse = static_cast<int>(available2 < 2 ? available2 : 2);
    if (channelsToUse <= 0)
      return false;

    state->callbacks.bufferSwitch = &AsioInterface::bufferSwitchThunk;
    state->callbacks.bufferSwitchTimeInfo = &AsioInterface::bufferSwitchTimeInfoThunk;
    state->callbacks.asioMessage = &AsioInterface::asioMessageThunk;
    state->callbacks.sampleRateDidChange = &AsioInterface::sampleRateDidChangeThunk;

    if (state->bufferInfos)
    {
      ASIODisposeBuffers();
      delete[] state->bufferInfos;
      state->bufferInfos = nullptr;
    }

    state->bufferInfos = new ASIOBufferInfo[channelsToUse];
    for (int i = 0; i < channelsToUse; i++)
    {
      state->bufferInfos[i].isInput = ASIOTrue;
      state->bufferInfos[i].channelNum = state->selectedInputIndex + i;
      state->bufferInfos[i].buffers[0] = state->bufferInfos[i].buffers[1] = nullptr;
    }

    {
      // set current instance for callbacks before creating buffers
      AsioInterface::s_current = this;
      ASIOError cr = ASIOCreateBuffers(state->bufferInfos, channelsToUse, state->preferredSize, &state->callbacks);
      if (cr != ASE_OK)
      {
        Logger::getInstance() << "ASIOCreateBuffers failed: " << asioErrStr(cr) << std::endl;
        // cleanup allocated arrays and instance pointer
        AsioInterface::s_current = nullptr;
        delete[] state->bufferInfos;
        state->bufferInfos = nullptr;
        return false;
      }
    }

    if (state->channelInfos)
    {
      delete[] state->channelInfos;
      state->channelInfos = nullptr;
    }
    state->channelInfos = new ASIOChannelInfo[channelsToUse];
    for (int i = 0; i < channelsToUse; i++)
    {
      state->channelInfos[i].channel = state->selectedInputIndex + i;
      state->channelInfos[i].isInput = ASIOTrue;
      ASIOGetChannelInfo(&state->channelInfos[i]);
    }

    // Sanity check: preferred size should be > 0 and <= max
    if (state->preferredSize <= 0 || (state->maxSize > 0 && state->preferredSize > state->maxSize))
    {
      Logger::getInstance() << "Invalid preferred buffer size: " << state->preferredSize << " (max=" << state->maxSize << ")" << std::endl;
      return false;
    }

    // Precompute converters (mono path: index 0)
    for (int i = 0; i < channelsToUse && i < 2; i++)
    {
      switch (state->channelInfos[i].type)
      {
      case ASIOSTFloat32LSB:
        state->convertSample[i] = AsioConverters::convFloat32;
        state->kind[i] = AsioState::kKindFloat32;
        state->scale[i] = 1.0f;
        break;
      case ASIOSTInt32LSB:
        state->convertSample[i] = AsioConverters::convInt32;
        state->kind[i] = AsioState::kKindInt32;
        state->scale[i] = (1.0f / 2147483648.0f);
        break;
      case ASIOSTInt32LSB24:
        state->convertSample[i] = AsioConverters::convInt32LSB24;
        state->kind[i] = AsioState::kKindInt32LSB24;
        state->scale[i] = (1.0f / 8388608.0f);
        break;
      case ASIOSTInt32LSB20:
        state->convertSample[i] = AsioConverters::convInt32LSB20;
        state->kind[i] = AsioState::kKindUnknown;
        state->scale[i] = 1.0f;
        break;
      case ASIOSTInt32LSB18:
        state->convertSample[i] = AsioConverters::convInt32LSB18;
        state->kind[i] = AsioState::kKindUnknown;
        state->scale[i] = 1.0f;
        break;
      case ASIOSTInt32LSB16:
        state->convertSample[i] = AsioConverters::convInt32LSB16;
        state->kind[i] = AsioState::kKindUnknown;
        state->scale[i] = 1.0f;
        break;
      case ASIOSTInt24LSB:
        state->convertSample[i] = AsioConverters::convInt24;
        state->kind[i] = AsioState::kKindUnknown;
        state->scale[i] = 1.0f;
        break;
      case ASIOSTInt16LSB:
        state->convertSample[i] = AsioConverters::convInt16;
        state->kind[i] = AsioState::kKindInt16;
        state->scale[i] = (1.0f / 32768.0f);
        break;
      default:
        state->convertSample[i] = nullptr;
        state->kind[i] = AsioState::kKindUnknown;
        state->scale[i] = 1.0f;
        break;
      }
    }

    // ring buffer length: 100 ms mono or 8 blocks, whichever is larger, rounded to power of two
    const int minFrames100ms = (state->sampleRate > 0 ? static_cast<int>(state->sampleRate * 0.1) : state->preferredSize * 8);
    const int minFrames = (minFrames100ms > (state->preferredSize * 8) ? minFrames100ms : (state->preferredSize * 8));
    int pow2 = 1;
    while (pow2 < minFrames)
      pow2 <<= 1;
    // Double capacity for extra headroom against jitter/underruns
    pow2 <<= 1;
    ringBuffer.resize(static_cast<uint32_t>(pow2));

    state->callbacksEnabled = false;
    {
      ASIOError sr = ASIOStart();
      if (sr != ASE_OK)
      {
        Logger::getInstance() << "ASIOStart failed: " << asioErrStr(sr) << std::endl;
        // cleanup if start fails to avoid leaks and dangling callbacks
        ASIODisposeBuffers();
        if (state->bufferInfos)
        {
          delete[] state->bufferInfos;
          state->bufferInfos = nullptr;
        }
        if (state->channelInfos)
        {
          delete[] state->channelInfos;
          state->channelInfos = nullptr;
        }
        ringBuffer.sync();
        AsioInterface::s_current = nullptr;
        return false;
      }
    }

    isStreaming = true;
    Logger::getInstance() << "ASIO stream started for interface: " << asioDevices[currentInterfaceIndex].name
                          << ", input index: " << currentInputIndex
                          << ", preferred buffer: " << state->preferredSize << std::endl;
    state->callbacksEnabled = true;
    return true;
  }

  void AsioInterface::stopAudioStream()
  {
    if (!isStreaming)
      return;
    state->callbacksEnabled = false;
    Logger::getInstance() << "ASIO stream stopping" << std::endl;
    ASIOStop();
    // Wait for any in-flight callback to finish before freeing buffers
    for (int spins = 0; spins < 10000; ++spins)
    {
      if (state->activeCallbackCount == 0)
        break;
      _mm_pause();
    }
    ASIODisposeBuffers();
    if (state->bufferInfos)
    {
      delete[] state->bufferInfos;
      state->bufferInfos = nullptr;
    }
    if (state->channelInfos)
    {
      delete[] state->channelInfos;
      state->channelInfos = nullptr;
    }

    isStreaming = false;
    Logger::getInstance() << "ASIO stream stopped" << std::endl;
    if (AsioInterface::s_current == this)
      AsioInterface::s_current = nullptr;
  }

  void AsioInterface::shutdown()
  {
    // Ensure streaming is stopped and buffers are freed
    if (isStreaming)
    {
      Logger::getInstance() << "ASIO stream stopping called from shutdown" << std::endl;
      stopAudioStream();
    }

    if (state->bufferInfos)
    {
      delete[] state->bufferInfos;
      state->bufferInfos = nullptr;
    }
    if (state->channelInfos)
    {
      delete[] state->channelInfos;
      state->channelInfos = nullptr;
    }

    // Tell driver we are done
    ASIODisposeBuffers();
    ASIOExit();
    currentInterfaceIndex = -1;
    currentInputIndex = -1;
  }

  bool AsioInterface::getAudioData(float *__restrict outputBuffer, int numSamples, int numChannels)
  {
    if (!isStreaming || currentInterfaceIndex < 0 || currentInputIndex < 0)
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

    // read head is maintained inside ringBuffer

    // Compute available frames (mono) and clamp reads to avoid underrun
    const uint32_t wposNow = ringBuffer.getWritePos();
    const uint32_t rposNow = ringBuffer.getReadPos();
    const uint32_t mask = ringBuffer.mask();
    uint32_t available = (wposNow - rposNow) & mask;

    int toRead = total < available ? total : available;
    for (int i = 0; i < toRead; i++)
    {
      outputBuffer[i] = ringBuffer.data()[rposNow + i & mask];
    }
    ringBuffer.advanceRead((uint32_t)toRead);
    if (toRead < total)
    {
      Logger::getInstance() << "Underrun: requested " << total << ", available " << available << std::endl;
      std::memset(outputBuffer + toRead, 0, sizeof(float) * (total - toRead));
    }

    return true;
  }

  bool AsioInterface::getAudioDataStereo(float *__restrict outL, float *__restrict outR, int numSamples)
  {
    if (!isStreaming || currentInterfaceIndex < 0 || currentInputIndex < 0 || !outL || !outR)
      return false;

    if (numSamples <= 0)
      return false;

    // read head is maintained inside ringBuffer

    // Compute available frames (mono) and clamp reads to avoid underrun
    const uint32_t wposNow = ringBuffer.getWritePos();
    const uint32_t rposNow = ringBuffer.getReadPos();
    const uint32_t mask = ringBuffer.mask();
    uint32_t available = (wposNow - rposNow) & mask;
    int framesToRead = numSamples < available ? numSamples : available;

    // Ring is mono frames; duplicate to L/R with contiguous fast path
    int framesLeft = framesToRead;
    uint32_t rpos2 = ringBuffer.getReadPos();
    int contFrames = static_cast<int>(ringBuffer.capacity() - rpos2);
    int f1 = framesLeft < contFrames ? framesLeft : contFrames;
    for (int i = 0; i < f1; i++)
    {
      float v = ringBuffer.data()[rpos2++];
      outL[i] = v;
      outR[i] = v;
    }
    framesLeft -= f1;
    if (framesLeft > 0)
    {
      rpos2 = 0;
      int base = f1;
      for (int i = 0; i < framesLeft; i++)
      {
        float v = ringBuffer.data()[rpos2++];
        outL[base + i] = v;
        outR[base + i] = v;
      }
    }
    ringBuffer.advanceRead((uint32_t)framesToRead);
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
    if (!state || !state->callbacksEnabled)
      return 0;
    const uint32_t mask = ringBuffer.mask();
    const uint32_t wposNow = ringBuffer.getWritePos();
    const uint32_t rposNow = ringBuffer.getReadPos();
    return (wposNow - rposNow) & mask;
  }

  bool AsioInterface::isConnectedAndStreaming()
  {
    return isStreaming && currentInterfaceIndex >= 0 && currentInputIndex >= 0 && ringBuffer.capacity() > 0;
  }
}
