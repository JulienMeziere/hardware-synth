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

  class AsioInterface
  {
  public:
    AsioInterface();
    ~AsioInterface();

    // Get list of available ASIO interfaces
    std::vector<std::string> listAsioInterfaces();

    // Connect to a specific ASIO interface by index
    bool connectToInterface(int deviceIndex);

    // Get available inputs for a specific ASIO interface
    std::vector<std::string> getAsioInputs(int deviceIndex);

    // Connect to a specific input on the current ASIO interface
    bool connectToInput(int inputIndex);

    // Start audio streaming from the connected input
    bool startAudioStream();

    // Stop audio streaming
    void stopAudioStream();

    // Properly shutdown ASIO driver and clear state
    void shutdown();

    // Get audio data from the connected input (called by VST process)
    bool getAudioData(float *__restrict outputBuffer, int numSamples, int numChannels);

    // Get audio data directly into separate L/R buffers (stereo)
    bool getAudioDataStereo(float *__restrict outL, float *__restrict outR, int numSamples);

    // Query how many mono frames are currently available to read
    int availableFrames();

    // True if an ASIO interface and input are connected and streaming
    bool isConnectedAndStreaming();

    // Handle driver reset requests (buffer size/sample rate changes)
    void handlePendingReset();

    // Get the stored ASIO interfaces
    const std::vector<AsioInterfaceInfo> &getAsioDevices();

  private:
    // Store the list of ASIO interfaces
    std::vector<AsioInterfaceInfo> asioDevices;

    // Current connection state
    int currentInterfaceIndex;
    int currentInputIndex;
    bool isStreaming;

    // Internal ring buffer managed in implementation file
  };
}
