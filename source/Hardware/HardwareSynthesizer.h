#pragma once

#include <string>
#include <memory>
#include <windows.h>
#include <mmsystem.h>
#include "MIDIScheduler.h"
#include <chrono>

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

    // Scheduled (time-aware) sending API
    void scheduleMIDINote(UINT note, UINT velocity, UINT channel, double offsetSeconds);
    void scheduleMIDINoteOff(UINT note, UINT channel, double offsetSeconds);
    void scheduleMIDIControlChange(UINT controller, UINT value, UINT channel, double offsetSeconds);

    // Absolute-time variants (use one baseNow per process block in caller)
    void scheduleMIDINoteAt(UINT note, UINT velocity, UINT channel, std::chrono::steady_clock::time_point when);
    void scheduleMIDINoteOffAt(UINT note, UINT channel, std::chrono::steady_clock::time_point when);
    void scheduleMIDIControlChangeAt(UINT controller, UINT value, UINT channel, std::chrono::steady_clock::time_point when);

  private:
    std::string deviceName;
    std::string manufacturer;
    UINT deviceId;
    bool inputDevice;
    bool connected;

    // MIDI handles
    HMIDIOUT midiOutHandle;
    HMIDIIN midiInHandle;
    MIDIScheduler scheduler;

    bool initializeMIDI();
    void cleanupMIDI();
  };
}
