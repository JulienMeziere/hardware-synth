#pragma once

#include <string>
#include <fstream>
#include <sstream>

namespace Newkon
{
#ifdef HARDWARE_SYNTH_RELEASE
  // Release mode: No-op logger (zero overhead)
  class Logger
  {
  public:
    static Logger &getInstance(bool debug = true)
    {
      static Logger instance;
      return instance;
    }

    static void killInstance() {}

    // No-op operators for release mode
    template <typename T>
    Logger &operator<<(const T &) { return *this; }

    Logger &operator<<(std::ostream &(*)(std::ostream &)) { return *this; }

  private:
    Logger() = default;
  };
#else
  // Debug mode: Full logging functionality
  class Logger : public std::ofstream
  {
  public:
    static Logger &getInstance(bool debug = true)
    {
      static Logger instance(debug);
      return instance;
    }

    static void killInstance()
    {
      Logger &instance = getInstance();
      instance.close();
    }

  private:
    Logger(bool debug = true) : std::ofstream(debug ? "C:\\Users\\jusde\\dev\\hardware-synth\\plugin\\serialLogs.txt" : "", debug ? std::ios::app : std::ios::out) {}
  };
#endif
} // namespace Newkon
