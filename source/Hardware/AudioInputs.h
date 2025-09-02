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

    // Get the stored ASIO interfaces
    static const std::vector<AsioInterfaceInfo> &getAsioDevices();

  private:
    // Store the list of ASIO interfaces
    static std::vector<AsioInterfaceInfo> asioDevices;
  };
}
