#include "setTimeout.h"

namespace Newkon
{
  // Implementation of TimeoutHandle class
  TimeoutHandle::TimeoutHandle(std::function<void()> callback)
      : callback(callback), canceled(false)
  {
  }

  void TimeoutHandle::execute()
  {
    if (!canceled.load())
      callback();
  }

  void TimeoutHandle::cancel()
  {
    canceled.store(true);
  }

  // Overloaded setTimeout function implementations
  std::shared_ptr<TimeoutHandle> setTimeoutImpl(FunctionWrapper function, std::chrono::milliseconds delay)
  {
    auto handle = std::make_shared<TimeoutHandle>([function = std::move(function)]()
                                                  { function(); });

    std::thread([handle, delay]()
                {
                  std::this_thread::sleep_for(delay);
                  handle->execute(); })
        .detach();

    return handle;
  }

  std::shared_ptr<TimeoutHandle> setTimeout(FunctionWrapper function, std::chrono::milliseconds delay)
  {
    return setTimeoutImpl(std::move(function), delay);
  }

  std::shared_ptr<TimeoutHandle> setTimeout(FunctionWrapper function, int delay)
  {
    return setTimeoutImpl(std::move(function), std::chrono::milliseconds(delay));
  }

  void clearTimeout(std::shared_ptr<TimeoutHandle> handle)
  {
    if (handle)
      handle->cancel();
  }

} // namespace Newkon
