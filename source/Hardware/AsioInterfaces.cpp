#include "AsioInterfaces.h"
#include "../Logger.h"
#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <cstdlib>

// ASIO SDK includes
#include "asiosys.h"
#include "asio.h"
#include "asiodrivers.h"
#include "asiolist.h"

extern AsioDrivers *asioDrivers;
extern bool loadAsioDriver(char *name);

namespace Newkon
{
  // Static member initialization
  std::vector<AsioInterfaceInfo> AsioInterfaces::asioDevices;
  int AsioInterfaces::currentInterfaceIndex = -1;
  int AsioInterfaces::currentInputIndex = -1;
  bool AsioInterfaces::isStreaming = false;

  // ASIO driver state
  static ASIODriverInfo g_driverInfo = {};
  static ASIOCallbacks g_callbacks = {};
  static ASIOBufferInfo *g_bufferInfos = nullptr;
  static ASIOChannelInfo *g_channelInfos = nullptr;
  static long g_inputChannels = 0;
  static long g_minSize = 0, g_maxSize = 0, g_preferredSize = 0, g_granularity = 0;
  static ASIOSampleRate g_sampleRate = 0.0;

  // ring buffer (stereo interleaved)
  alignas(32) static float *g_audioBuffer = nullptr; // aligned allocation below
  static int g_bufferSize = 0;
  static int g_bufferPos = 0;
  static int g_readPos = 0;

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
    if (!g_bufferInfos || !g_channelInfos || g_preferredSize <= 0)
      return;

    // Always treat as mono source and write mono frames
    const int channelsToUse = 1;
    const int writeBlock = static_cast<int>(g_preferredSize);

    {
      // Mono: convert first input channel only via precomputed converter
      const void *src0 = g_bufferInfos[0].buffers[index];

      long framesToWrite = g_preferredSize;
      // First contiguous run until end of ring (mono frames)
      long contFrames = g_bufferSize - g_bufferPos;
      long f1 = framesToWrite < contFrames ? framesToWrite : contFrames;

      if (g_kind[0] == kKindFloat32)
      {
        const float *pf = static_cast<const float *>(src0);
        for (long i = 0; i < f1; i++)
          g_audioBuffer[g_bufferPos++] = pf[i];
      }
      else if (g_kind[0] == kKindInt16)
      {
        const int16_t *ps = static_cast<const int16_t *>(src0);
        const float s = g_scale[0];
        for (long i = 0; i < f1; i++)
          g_audioBuffer[g_bufferPos++] = ps[i] * s;
      }
      else if (g_kind[0] == kKindInt32 || g_kind[0] == kKindInt32LSB24)
      {
        const int32_t *pi = static_cast<const int32_t *>(src0);
        const float s = g_scale[0];
        for (long i = 0; i < f1; i++)
          g_audioBuffer[g_bufferPos++] = pi[i] * s;
      }
      else
      {
        for (long i = 0; i < f1; i++)
          g_audioBuffer[g_bufferPos++] = g_convertSample[0] ? g_convertSample[0](src0, i) : 0.0f;
      }

      framesToWrite -= f1;
      if (framesToWrite > 0)
      {
        // Wrap
        g_bufferPos = 0;
        long base = f1;
        if (g_kind[0] == kKindFloat32)
        {
          const float *pf = static_cast<const float *>(src0);
          for (long i = 0; i < framesToWrite; i++)
            g_audioBuffer[g_bufferPos++] = pf[base + i];
        }
        else if (g_kind[0] == kKindInt16)
        {
          const int16_t *ps = static_cast<const int16_t *>(src0);
          const float s = g_scale[0];
          for (long i = 0; i < framesToWrite; i++)
            g_audioBuffer[g_bufferPos++] = ps[base + i] * s;
        }
        else if (g_kind[0] == kKindInt32 || g_kind[0] == kKindInt32LSB24)
        {
          const int32_t *pi = static_cast<const int32_t *>(src0);
          const float s = g_scale[0];
          for (long i = 0; i < framesToWrite; i++)
            g_audioBuffer[g_bufferPos++] = pi[base + i] * s;
        }
        else
        {
          for (long i = 0; i < framesToWrite; i++)
            g_audioBuffer[g_bufferPos++] = g_convertSample[0] ? g_convertSample[0](src0, base + i) : 0.0f;
        }
      }
    }
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
      return 1;
    default:
      return 0;
    }
  }

  std::vector<std::string> AsioInterfaces::listAsioInterfaces()
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

    return deviceNames;
  }

  bool AsioInterfaces::connectToInterface(int deviceIndex)
  {
    if (deviceIndex < 0 || deviceIndex >= static_cast<int>(asioDevices.size()))
    {
      return false;
    }

    stopAudioStream();
    ASIOExit();

    currentInterfaceIndex = deviceIndex;

    char name[256] = {0};
    std::snprintf(name, sizeof(name), "%s", asioDevices[deviceIndex].name.c_str());
    if (!loadAsioDriver(name))
    {
      currentInterfaceIndex = -1;
      return false;
    }

    std::memset(&g_driverInfo, 0, sizeof(g_driverInfo));
    g_driverInfo.sysRef = (void *)GetDesktopWindow();
    if (ASIOInit(&g_driverInfo) != ASE_OK)
    {
      ASIOExit();
      currentInterfaceIndex = -1;
      return false;
    }

    long dummyOutputs = 0;
    if (ASIOGetChannels(&g_inputChannels, &dummyOutputs) != ASE_OK)
      return false;
    if (ASIOGetBufferSize(&g_minSize, &g_maxSize, &g_preferredSize, &g_granularity) != ASE_OK)
      return false;
    if (ASIOGetSampleRate(&g_sampleRate) != ASE_OK)
      g_sampleRate = 44100.0;

    return true;
  }

  std::vector<std::string> AsioInterfaces::getAsioInputs(int deviceIndex)
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

  const std::vector<AsioInterfaceInfo> &AsioInterfaces::getAsioDevices()
  {
    return asioDevices;
  }

  bool AsioInterfaces::connectToInput(int inputIndex)
  {
    if (currentInterfaceIndex < 0 || currentInterfaceIndex >= static_cast<int>(asioDevices.size()))
    {
      return false;
    }

    if (inputIndex < 0)
    {
      return false;
    }

    currentInputIndex = inputIndex;
    s_selectedInputIndex = inputIndex;

    return true;
  }

  bool AsioInterfaces::startAudioStream()
  {
    if (currentInterfaceIndex < 0 || currentInputIndex < 0)
    {
      return false;
    }

    if (isStreaming)
    {
      stopAudioStream();
    }

    long available2 = g_inputChannels - AsioInterfaces::currentInputIndex;
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

    if (ASIOCreateBuffers(g_bufferInfos, channelsToUse, g_preferredSize, &g_callbacks) != ASE_OK)
      return false;

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

    // Precompute converters (mono path: index 0)
    for (int i = 0; i < channelsToUse && i < 2; i++)
    {
      switch (g_channelInfos[i].type)
      {
      case ASIOSTFloat32LSB:
        g_convertSample[i] = convFloat32;
        break;
      case ASIOSTInt32LSB:
        g_convertSample[i] = convInt32;
        break;
      case ASIOSTInt32LSB24:
        g_convertSample[i] = convInt32LSB24;
        break;
      case ASIOSTInt32LSB20:
        g_convertSample[i] = convInt32LSB20;
        break;
      case ASIOSTInt32LSB18:
        g_convertSample[i] = convInt32LSB18;
        break;
      case ASIOSTInt32LSB16:
        g_convertSample[i] = convInt32LSB16;
        break;
      case ASIOSTInt24LSB:
        g_convertSample[i] = convInt24;
        break;
      case ASIOSTInt16LSB:
        g_convertSample[i] = convInt16;
        break;
      default:
        g_convertSample[i] = nullptr;
        break;
      }
    }

    if (g_audioBuffer)
    {
      delete[] g_audioBuffer;
      g_audioBuffer = nullptr;
    }
    // ring buffer length: 100 ms mono or 8 blocks, whichever is larger
    const int minFrames100ms = (g_sampleRate > 0 ? static_cast<int>(g_sampleRate * 0.1) : g_preferredSize * 8);
    const int minFrames = (minFrames100ms > (g_preferredSize * 8) ? minFrames100ms : (g_preferredSize * 8));
    g_bufferSize = minFrames; // mono frames
    // 32-byte aligned alloc for better vectorization
    g_audioBuffer = static_cast<float *>(_aligned_malloc(sizeof(float) * g_bufferSize, 32));
    std::memset(g_audioBuffer, 0, sizeof(float) * g_bufferSize);
    g_bufferPos = 0;

    if (ASIOStart() != ASE_OK)
      return false;

    isStreaming = true;
    return true;
  }

  void AsioInterfaces::stopAudioStream()
  {
    if (!isStreaming)
      return;
    ASIOStop();
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
    if (g_audioBuffer)
    {
      _aligned_free(g_audioBuffer);
      g_audioBuffer = nullptr;
    }
    g_bufferSize = 0;
    g_bufferPos = 0;
    isStreaming = false;
  }

  bool AsioInterfaces::getAudioData(float *outputBuffer, int numSamples, int numChannels)
  {
    if (!isStreaming || currentInterfaceIndex < 0 || currentInputIndex < 0 || !g_audioBuffer)
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

    // Initialize read cursor safely behind writer (~4 ASIO blocks)
    if (g_readPos <= 0 || g_readPos >= g_bufferSize)
    {
      int safety = g_preferredSize * 2 * 4; // frames * stereo * blocks
      if (safety > g_bufferSize / 2)
        safety = g_bufferSize / 2;
      g_readPos = (g_bufferPos - safety + g_bufferSize) % g_bufferSize;
    }

    for (int i = 0; i < total; i++)
    {
      outputBuffer[i] = g_audioBuffer[g_readPos];
      g_readPos++;
      if (g_readPos >= g_bufferSize)
        g_readPos = 0;
    }

    return true;
  }

  bool AsioInterfaces::getAudioDataStereo(float *outL, float *outR, int numSamples)
  {
    if (!isStreaming || currentInterfaceIndex < 0 || currentInputIndex < 0 || !g_audioBuffer || !outL || !outR)
      return false;

    if (numSamples <= 0)
      return false;

    // Initialize read cursor if needed (mono ring)
    if (g_readPos <= 0 || g_readPos >= g_bufferSize)
    {
      int safety = g_preferredSize * 4; // frames*blocks
      if (safety > g_bufferSize / 2)
        safety = g_bufferSize / 2;
      g_readPos = (g_bufferPos - safety + g_bufferSize) % g_bufferSize;
    }

    // Ring is mono frames; duplicate to L/R with contiguous fast path
    int framesLeft = numSamples;
    int contFrames = g_bufferSize - g_readPos;
    int f1 = framesLeft < contFrames ? framesLeft : contFrames;
    for (int i = 0; i < f1; i++)
    {
      float v = g_audioBuffer[g_readPos++];
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
        float v = g_audioBuffer[g_readPos++];
        outL[base + i] = v;
        outR[base + i] = v;
      }
    }

    return true;
  }
}
