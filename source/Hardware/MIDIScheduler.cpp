#include "MIDIScheduler.h"
#include "../Logger.h"

namespace Newkon
{
  MIDIScheduler::MIDIScheduler() {}
  MIDIScheduler::~MIDIScheduler() { stop(); }

  void MIDIScheduler::start(HMIDIOUT handle)
  {
    stop();
    midiOut = handle;
    running.store(true, std::memory_order_relaxed);
    worker = std::thread(&MIDIScheduler::run, this);
  }

  void MIDIScheduler::stop()
  {
    if (running.exchange(false))
    {
      cv.notify_all();
      if (worker.joinable())
        worker.join();
    }
    // drain
    {
      std::lock_guard<std::mutex> lock(mutex);
      while (!queue.empty())
        queue.pop();
    }
    midiOut = nullptr;
  }

  void MIDIScheduler::scheduleShortMsg(DWORD msg, std::chrono::steady_clock::time_point when)
  {
    {
      std::lock_guard<std::mutex> lock(mutex);
      queue.push(Scheduled{when, msg});
    }
    cv.notify_all();
  }

  void MIDIScheduler::run()
  {
    using namespace std::chrono;
    while (running.load(std::memory_order_relaxed))
    {
      std::unique_lock<std::mutex> lock(mutex);
      if (queue.empty())
      {
        cv.wait(lock, [&]
                { return !running.load(std::memory_order_relaxed) || !queue.empty(); });
        if (!running.load(std::memory_order_relaxed))
          break;
      }
      auto next = queue.top();
      auto now = steady_clock::now();
      if (next.when > now)
      {
        cv.wait_until(lock, next.when, [&]
                      { return !running.load(std::memory_order_relaxed); });
        if (!running.load(std::memory_order_relaxed))
          break;
        now = steady_clock::now();
        if (next.when > now)
          continue; // re-check
      }
      queue.pop();
      DWORD msg = next.msg;
      HMIDIOUT out = midiOut;
      lock.unlock();
      if (out)
      {
        midiOutShortMsg(out, msg);
        const UINT status = msg & 0xF0;
        if (status == 0x90)
        {
          Logger::getInstance() << "MIDI Note On sent: msg=0x" << std::hex << msg << std::dec << std::endl;
        }
        else if (status == 0x80)
        {
          Logger::getInstance() << "MIDI Note Off sent: msg=0x" << std::hex << msg << std::dec << std::endl;
        }
        else if (status == 0xB0)
        {
          Logger::getInstance() << "MIDI CC sent: msg=0x" << std::hex << msg << std::dec << std::endl;
        }
      }
    }
  }
}
