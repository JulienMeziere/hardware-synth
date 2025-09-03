#pragma once

#include <vector>
#include <string>

namespace Newkon
{
  // Structure to hold ASIO interface information
  struct AsioInterfaceInfo
  {
    std::string name;
    int deviceIndex;
    bool isDefault;
  };

  class AsioInterfaces
  {
  public:
    // Get list of available ASIO interfaces
    static std::vector<std::string> listAsioInterfaces();

    // Connect to a specific ASIO interface by index
    static bool connectToInterface(int deviceIndex);

    // Get available inputs for a specific ASIO interface
    static std::vector<std::string> getAsioInputs(int deviceIndex);

    // Connect to a specific input on the current ASIO interface
    static bool connectToInput(int inputIndex);

    // Start audio streaming from the connected input
    static bool startAudioStream();

    // Stop audio streaming
    static void stopAudioStream();

    // Properly shutdown ASIO driver and clear state
    static void shutdown();

    // Get audio data from the connected input (called by VST process)
    static bool getAudioData(float *__restrict outputBuffer, int numSamples, int numChannels);

    // Get audio data directly into separate L/R buffers (stereo)
    static bool getAudioDataStereo(float *__restrict outL, float *__restrict outR, int numSamples);

    // Get the stored ASIO interfaces
    static const std::vector<AsioInterfaceInfo> &getAsioDevices();

  private:
    // Store the list of ASIO interfaces
    static std::vector<AsioInterfaceInfo> asioDevices;

    // Current connection state
    static int currentInterfaceIndex;
    static int currentInputIndex;
    static bool isStreaming;
  };
}
