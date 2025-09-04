#pragma once

#include <cstdint>
#include <atomic>

namespace Newkon
{
  class RingBufferFloat
  {
  public:
    explicit RingBufferFloat(uint32_t capacityPow2);
    ~RingBufferFloat();

    void resize(uint32_t capacityPow2);
    uint32_t capacity() const { return cap_; }
    uint32_t mask() const { return mask_; }

    void clear();
    void alignReadBehindWrite(uint32_t distance);

    // Low-level accessors used for zero-copy conversions
    float *data() { return data_; }
    uint32_t getWritePos() const { return writePos_.load(std::memory_order_relaxed); }
    uint32_t getReadPos() const { return readPos_.load(std::memory_order_relaxed); }
    void setReadPos(uint32_t pos) { readPos_.store(pos & mask_, std::memory_order_relaxed); }
    void advanceWrite(uint32_t count);
    void advanceRead(uint32_t count);

    // Convenience copying APIs
    uint32_t read(float *dst, uint32_t count);

  private:
    float *data_ = nullptr;
    uint32_t cap_ = 0;
    uint32_t mask_ = 0;
    std::atomic<uint32_t> writePos_{0};
    std::atomic<uint32_t> readPos_{0};
  };
}
