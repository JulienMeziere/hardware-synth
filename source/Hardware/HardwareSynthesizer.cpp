#include "HardwareSynthesizer.h"
#include "../Logger.h"
#include <windows.h>
#include <mmsystem.h>

namespace Newkon
{
  HardwareSynthesizer::HardwareSynthesizer(const std::string &deviceName,
                                           const std::string &manufacturer,
                                           UINT deviceId,
                                           bool isInputDevice)
      : deviceName(deviceName), manufacturer(manufacturer), deviceId(deviceId), inputDevice(isInputDevice), connected(false), midiOutHandle(nullptr), midiInHandle(nullptr)
  {
    Logger::getInstance() << "HardwareSynthesizer created: " << deviceName
                          << " (ID: " << deviceId << ")" << std::endl;
  }

  HardwareSynthesizer::~HardwareSynthesizer()
  {
    disconnect();
    Logger::getInstance() << "HardwareSynthesizer destroyed: " << deviceName << std::endl;
  }

  bool HardwareSynthesizer::connect()
  {
    if (connected)
    {
      Logger::getInstance() << "Device already connected: " << deviceName << std::endl;
      return true;
    }

    if (initializeMIDI())
    {
      connected = true;
      Logger::getInstance() << "Successfully connected to: " << deviceName << std::endl;
      return true;
    }
    else
    {
      Logger::getInstance() << "Failed to connect to: " << deviceName << std::endl;
      return false;
    }
  }

  void HardwareSynthesizer::disconnect()
  {
    if (!connected)
      return;

    cleanupMIDI();
    connected = false;
    Logger::getInstance() << "Disconnected from: " << deviceName << std::endl;
  }

  bool HardwareSynthesizer::sendMIDINote(UINT note, UINT velocity, UINT channel)
  {
    if (!connected || inputDevice)
    {
      return false;
    }

    // MIDI Note On message: 0x90 + channel, note, velocity
    DWORD midiMessage = 0x90 | (channel & 0x0F);
    midiMessage |= (note & 0x7F) << 8;
    midiMessage |= (velocity & 0x7F) << 16;

    MMRESULT result = midiOutShortMsg(midiOutHandle, midiMessage);
    if (result == MMSYSERR_NOERROR)
    {
      return true;
    }
    else
    {
      return false;
    }
  }

  bool HardwareSynthesizer::sendMIDINoteOff(UINT note, UINT channel)
  {
    if (!connected || inputDevice)
    {

      return false;
    }

    // MIDI Note Off message: 0x80 + channel, note, velocity (usually 64 for note off)
    DWORD midiMessage = 0x80 | (channel & 0x0F);
    midiMessage |= (note & 0x7F) << 8;
    midiMessage |= 64 << 16; // Standard note off velocity

    MMRESULT result = midiOutShortMsg(midiOutHandle, midiMessage);
    if (result == MMSYSERR_NOERROR)
    {

      return true;
    }
    else
    {

      return false;
    }
  }

  bool HardwareSynthesizer::sendMIDIControlChange(UINT controller, UINT value, UINT channel)
  {
    if (!connected || inputDevice)
    {

      return false;
    }

    // MIDI Control Change message: 0xB0 + channel, controller, value
    DWORD midiMessage = 0xB0 | (channel & 0x0F);
    midiMessage |= (controller & 0x7F) << 8;
    midiMessage |= (value & 0x7F) << 16;

    MMRESULT result = midiOutShortMsg(midiOutHandle, midiMessage);
    if (result == MMSYSERR_NOERROR)
    {

      return true;
    }
    else
    {

      return false;
    }
  }

  bool HardwareSynthesizer::initializeMIDI()
  {
    if (inputDevice)
    {
      // For input devices, we would open midiIn
      MMRESULT result = midiInOpen(&midiInHandle, deviceId, 0, 0, CALLBACK_NULL);
      if (result != MMSYSERR_NOERROR)
      {
        Logger::getInstance() << "Failed to open MIDI input device " << deviceName
                              << " (Error: " << result << ")" << std::endl;
        return false;
      }
    }
    else
    {
      // For output devices, open midiOut
      MMRESULT result = midiOutOpen(&midiOutHandle, deviceId, 0, 0, CALLBACK_NULL);
      if (result != MMSYSERR_NOERROR)
      {
        Logger::getInstance() << "Failed to open MIDI output device " << deviceName
                              << " (Error: " << result << ")" << std::endl;
        return false;
      }
    }

    return true;
  }

  void HardwareSynthesizer::cleanupMIDI()
  {
    if (midiOutHandle)
    {
      midiOutClose(midiOutHandle);
      midiOutHandle = nullptr;
    }

    if (midiInHandle)
    {
      midiInClose(midiInHandle);
      midiInHandle = nullptr;
    }
  }
}
