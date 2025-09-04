#include "RingBufferFloat.h"
#include <cstdlib>
#include <cstring>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace Newkon
{
  static inline uint32_t roundUpPow2(uint32_t v)
  {
    if (v == 0)
      return 1;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
  }

  RingBufferFloat::RingBufferFloat(uint32_t capacityPow2)
  {
    cap_ = roundUpPow2(capacityPow2);
    mask_ = cap_ - 1;
#if defined(_MSC_VER)
    data_ = static_cast<float *>(_aligned_malloc(sizeof(float) * cap_, 32));
#else
    posix_memalign(reinterpret_cast<void **>(&data_), 32, sizeof(float) * cap_);
#endif
    std::memset(data_, 0, sizeof(float) * cap_);
    writePos_.store(0, std::memory_order_relaxed);
    readPos_.store(0, std::memory_order_relaxed);
  }

  RingBufferFloat::~RingBufferFloat()
  {
    if (data_)
    {
#if defined(_MSC_VER)
      _aligned_free(data_);
#else
      free(data_);
#endif
      data_ = nullptr;
    }
  }

  void RingBufferFloat::clear()
  {
    if (data_)
      std::memset(data_, 0, sizeof(float) * cap_);
    writePos_.store(0, std::memory_order_relaxed);
    readPos_.store(0, std::memory_order_relaxed);
  }

  void RingBufferFloat::alignReadBehindWrite(uint32_t distance)
  {
    const uint32_t w = writePos_.load(std::memory_order_acquire);
    const uint32_t d = (distance > cap_ / 2) ? (cap_ / 2) : distance;
    const uint32_t r = (w - d) & mask_;
    readPos_.store(r, std::memory_order_relaxed);
  }

  void RingBufferFloat::advanceWrite(uint32_t count)
  {
    const uint32_t w = writePos_.load(std::memory_order_relaxed);
    writePos_.store((w + count) & mask_, std::memory_order_release);
  }

  void RingBufferFloat::advanceRead(uint32_t count)
  {
    const uint32_t r = readPos_.load(std::memory_order_relaxed);
    readPos_.store((r + count) & mask_, std::memory_order_relaxed);
  }

  uint32_t RingBufferFloat::read(float *dst, uint32_t count)
  {
    if (count == 0)
      return 0;
    uint32_t r = readPos_.load(std::memory_order_relaxed);
    const uint32_t c1 = cap_ - r;
    uint32_t n1 = (count < c1) ? count : c1;
    std::memcpy(dst, data_ + r, sizeof(float) * n1);
    r = (r + n1) & mask_;
    uint32_t done = n1;
    if (done < count)
    {
      uint32_t n2 = count - done;
      std::memcpy(dst + done, data_, sizeof(float) * n2);
      r = n2;
      done += n2;
    }
    readPos_.store(r, std::memory_order_relaxed);
    return done;
  }
}
