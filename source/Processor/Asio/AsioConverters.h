#pragma once

#include <cstdint>

namespace Newkon
{
  namespace AsioConverters
  {
    float convFloat32(const void *src, long index);
    float convInt32(const void *src, long index);
    float convInt32LSB24(const void *src, long index);
    float convInt32LSB20(const void *src, long index);
    float convInt32LSB18(const void *src, long index);
    float convInt32LSB16(const void *src, long index);
    float convInt24(const void *src, long index);
    float convInt16(const void *src, long index);
  }
}
