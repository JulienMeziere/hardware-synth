#pragma once

#include <thread>
#include <functional>
#include <chrono>
#include <atomic>
#include <memory>

namespace Newkon
{
  // Class to represent a timeout handle
  class TimeoutHandle
  {
  public:
    TimeoutHandle(std::function<void()> callback);

    // Execute the callback if not canceled
    void execute();

    // Cancel the timeout
    void cancel();

  private:
    std::function<void()> callback;
    std::atomic<bool> canceled;
  };

  // Wrapper for lambda type to achieve type erasure
  using FunctionWrapper = std::function<void()>;

  // Overloaded setTimeout function declarations
  std::shared_ptr<TimeoutHandle> setTimeout(FunctionWrapper function, std::chrono::milliseconds delay);
  std::shared_ptr<TimeoutHandle> setTimeout(FunctionWrapper function, int delay);

  // clearTimeout function declaration
  void clearTimeout(std::shared_ptr<TimeoutHandle> handle);

} // namespace Newkon
