#include "MIDIDevices.h"
#include "HardwareSynthesizer.h"
#include "../Logger.h"
#include <windows.h>
#include <mmsystem.h>
#include <sstream>

namespace Newkon
{
  std::vector<MIDIDeviceInfo> MIDIDevices::outputDevices;

  // Helper function to convert WCHAR to std::string using Windows API
  std::string wcharToString(const WCHAR *wstr)
  {
    if (!wstr)
      return "";

    int size = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0)
      return "";

    std::string result(size - 1, 0); // -1 to exclude null terminator
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &result[0], size, nullptr, nullptr);
    return result;
  }

  std::vector<std::string> MIDIDevices::listMIDIdevices()
  {
    std::vector<std::string> devices;
    outputDevices.clear();

    // Get number of MIDI output devices only
    UINT numDevices = midiOutGetNumDevs();

    Logger::getInstance() << "Found " << numDevices << " MIDI output devices" << std::endl;

    for (UINT i = 0; i < numDevices; i++)
    {
      MIDIOUTCAPS caps;
      MMRESULT result = midiOutGetDevCaps(i, &caps, sizeof(caps));

      if (result == MMSYSERR_NOERROR)
      {
        // Convert WCHAR to std::string using Windows API
        std::string deviceName = wcharToString(caps.szPname);
        std::string manufacturer = "Unknown";

        // Try to get manufacturer info
        if (caps.wMid != 0)
        {
          std::ostringstream oss;
          oss << "Manufacturer ID: 0x" << std::hex << caps.wMid;
          manufacturer = oss.str();
        }

        // Store device info for later connection
        outputDevices.emplace_back(deviceName, manufacturer, i);

        std::string deviceInfo = "Device " + std::to_string(i) + ": " + deviceName + " (" + manufacturer + ")";
        devices.push_back(deviceInfo);

        Logger::getInstance() << deviceInfo << std::endl;
      }
      else
      {
        std::string errorMsg = "Failed to get device info for device " + std::to_string(i) + " (Error: " + std::to_string(result) + ")";
        devices.push_back(errorMsg);
        Logger::getInstance() << errorMsg << std::endl;
      }
    }

    return devices;
  }

  std::unique_ptr<HardwareSynthesizer> MIDIDevices::connectToDevice(size_t deviceIndex)
  {
    if (deviceIndex >= outputDevices.size())
    {
      Logger::getInstance() << "Invalid device index: " << deviceIndex
                            << " (available devices: " << outputDevices.size() << ")" << std::endl;
      return nullptr;
    }

    const MIDIDeviceInfo &deviceInfo = outputDevices[deviceIndex];
    Logger::getInstance() << "Attempting to connect to device index " << deviceIndex
                          << ": " << deviceInfo.deviceName << " (ID: " << deviceInfo.deviceId << ")" << std::endl;

    auto synthesizer = std::make_unique<HardwareSynthesizer>(
        deviceInfo.deviceName,
        deviceInfo.manufacturer,
        deviceInfo.deviceId,
        false // Always false since we only store output devices
    );

    if (synthesizer->connect())
    {
      return synthesizer;
    }

    Logger::getInstance() << "Failed to connect to device index: " << deviceIndex << std::endl;
    return nullptr;
  }

  const std::vector<MIDIDeviceInfo> &MIDIDevices::getOutputDevices()
  {
    return outputDevices;
  }
}
