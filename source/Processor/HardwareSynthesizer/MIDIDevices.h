#pragma once

#include <vector>
#include <string>
#include <memory>
#include <windows.h>
#include <mmsystem.h>

namespace Newkon
{
  class HardwareSynthesizer;

  struct MIDIDeviceInfo
  {
    std::string deviceName;
    std::string manufacturer;
    UINT deviceId;

    MIDIDeviceInfo(const std::string &name, const std::string &manufacturerName, UINT id)
        : deviceName(name), manufacturer(manufacturerName), deviceId(id) {}
  };

  class MIDIDevices
  {
  public:
    static std::vector<std::string> listMIDIdevices();
    static std::unique_ptr<HardwareSynthesizer> connectToDevice(size_t deviceIndex);
    static const std::vector<MIDIDeviceInfo> &getOutputDevices();

  private:
    static std::vector<MIDIDeviceInfo> outputDevices;
  };
}
