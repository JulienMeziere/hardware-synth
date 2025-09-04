#pragma once

#include <vector>
#include <string>
#include "RingBufferFloat.h"

// Forward declare minimal ASIO types to avoid including ASIO headers here
struct ASIOTime;
typedef long ASIOBool;
typedef double ASIOSampleRate;

namespace Newkon
{
  // Structure to hold ASIO interface information
  struct AsioInterfaceInfo
  {
    std::string name;
    int deviceIndex;
    bool isDefault;
  };

  struct AsioState; // forward declaration to keep ASIO SDK types out of header

  class AsioInterface
  {
  public:
    // Owns a single ASIO driver connection and a mono ring buffer bridging the ASIO thread to the host thread.
    AsioInterface();
    ~AsioInterface();

    // Enumerate installed ASIO drivers; populates internal device list and returns their names.
    std::vector<std::string> listAsioInterfaces();

    // Load and initialize the selected ASIO driver by index; cleans up any prior connection.
    bool connectToInterface(int deviceIndex);

    // Return available input channel names for a given interface (connects to it if necessary).
    std::vector<std::string> getAsioInputs(int deviceIndex);

    // Select the input channel index to use (first of a possible stereo pair). Does not start streaming.
    bool connectToInput(int inputIndex);

    // Create ASIO buffers, bind callbacks, size the ring buffer, and start streaming.
    bool startAudioStream();

    // Stop streaming, wait for in-flight callbacks, and dispose ASIO buffers.
    void stopAudioStream();

    // Full teardown of the driver and buffers; safe to call multiple times.
    void shutdown();

    // Read mono samples into an interleaved buffer (numChannels channels); zero-fills on underrun.
    bool getAudioData(float *__restrict outputBuffer, int numSamples, int numChannels);

    // Read mono samples and duplicate into L/R buffers; zero-fills on underrun.
    bool getAudioDataStereo(float *__restrict outL, float *__restrict outR, int numSamples);

    // Number of readable mono frames currently buffered.
    int availableFrames();

    // True if a driver is initialized, an input is selected, and the stream is running.
    bool isConnectedAndStreaming();

    // Access the last enumerated list of ASIO devices.
    const std::vector<AsioInterfaceInfo> &getAsioDevices();

  private:
    // Handle pending ASIO reset notifications by rebuilding buffers and restarting.
    void handlePendingReset();

    // Called by the ASIO driver on its audio thread when the driver flips the double-buffer.
    // index is 0/1 selecting which buffer half is ready. Forwards to the active instance.
    static void bufferSwitchThunk(long index, ASIOBool processNow);

    // Variant of bufferSwitch that includes timing info from the driver. Forwards to the active instance.
    // Must return the same ASIOTime* it received after handling the buffer flip.
    static ASIOTime *bufferSwitchTimeInfoThunk(ASIOTime *timeInfo, long index, ASIOBool processNow);

    // Notification that the driver's sample rate changed while running; updates instance state.
    static void sampleRateDidChangeThunk(ASIOSampleRate sRate);

    // Generic driver message callback (reset/resync/latency changed, etc.). Sets reset flags on the instance.
    // Returns 1 when the message is handled, 0 to indicate unsupported selector.
    static long asioMessageThunk(long selector, long value, void *message, double *opt);

    // Active instance for callback forwarding; set when buffers are created and cleared on stop/failure.
    static AsioInterface *s_current;

    // Store the list of ASIO interfaces
    std::vector<AsioInterfaceInfo> asioDevices;

    // Current connection state
    int currentInterfaceIndex;
    int currentInputIndex;
    bool isStreaming;

    RingBufferFloat ringBuffer;

    // Internal ASIO driver state
    AsioState *state;
  };
}
