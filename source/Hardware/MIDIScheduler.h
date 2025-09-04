#pragma once

#include <windows.h>
#include <mmsystem.h>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>

namespace Newkon
{
  class MIDIScheduler
  {
  public:
    MIDIScheduler();
    ~MIDIScheduler();

    void start(HMIDIOUT handle);
    void stop();

    void scheduleShortMsg(DWORD msg, std::chrono::steady_clock::time_point when);

  private:
    struct Scheduled
    {
      std::chrono::steady_clock::time_point when;
      DWORD msg;
      bool operator>(const Scheduled &other) const { return when > other.when; }
    };

    std::atomic<bool> running{false};
    HMIDIOUT midiOut{nullptr};
    std::thread worker;
    std::mutex mutex;
    std::condition_variable cv;

    // min-heap by time
    std::priority_queue<Scheduled, std::vector<Scheduled>, std::greater<Scheduled>> queue;

    void run();
  };
}
