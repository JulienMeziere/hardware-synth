#pragma once

#include <string>
#include <memory>
#include <windows.h>
#include <mmsystem.h>

namespace Newkon
{
  class HardwareSynthesizer
  {
  public:
    HardwareSynthesizer(const std::string &deviceName,
                        const std::string &manufacturer,
                        UINT deviceId,
                        bool isInputDevice = false);

    ~HardwareSynthesizer();

    // Getters
    const std::string &getDeviceName() const { return deviceName; }
    const std::string &getManufacturer() const { return manufacturer; }
    UINT getDeviceId() const { return deviceId; }
    bool isInputDevice() const { return inputDevice; }
    bool isConnected() const { return connected; }

    // MIDI operations
    bool connect();
    void disconnect();
    bool sendMIDINote(UINT note, UINT velocity, UINT channel = 0);
    bool sendMIDINoteOff(UINT note, UINT channel = 0);
    bool sendMIDIControlChange(UINT controller, UINT value, UINT channel = 0);

  private:
    std::string deviceName;
    std::string manufacturer;
    UINT deviceId;
    bool inputDevice;
    bool connected;

    // MIDI handles
    HMIDIOUT midiOutHandle;
    HMIDIIN midiInHandle;

    bool initializeMIDI();
    void cleanupMIDI();
  };
}
