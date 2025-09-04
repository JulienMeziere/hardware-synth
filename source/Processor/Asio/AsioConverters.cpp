#include "AsioConverters.h"

namespace Newkon
{
  namespace AsioConverters
  {
    float convFloat32(const void *src, long index)
    {
      const float *pf = static_cast<const float *>(src);
      return pf[index];
    }

    float convInt32(const void *src, long index)
    {
      const int32_t *pi = static_cast<const int32_t *>(src);
      return static_cast<float>(pi[index]) * (1.0f / 2147483648.0f);
    }

    float convInt32LSB24(const void *src, long index)
    {
      const int32_t *pi = static_cast<const int32_t *>(src);
      return static_cast<float>(pi[index]) * (1.0f / 8388608.0f);
    }

    float convInt32LSB20(const void *src, long index)
    {
      const int32_t *pi = static_cast<const int32_t *>(src);
      return static_cast<float>(pi[index]) * (1.0f / 524288.0f);
    }

    float convInt32LSB18(const void *src, long index)
    {
      const int32_t *pi = static_cast<const int32_t *>(src);
      return static_cast<float>(pi[index]) * (1.0f / 262144.0f);
    }

    float convInt32LSB16(const void *src, long index)
    {
      const int32_t *pi = static_cast<const int32_t *>(src);
      return static_cast<float>(pi[index]) * (1.0f / 32768.0f);
    }

    float convInt24(const void *src, long index)
    {
      const unsigned char *p = static_cast<const unsigned char *>(src);
      const int32_t o = index * 3;
      int32_t s = (int32_t)((int32_t)p[o + 0] | ((int32_t)p[o + 1] << 8) | ((int32_t)p[o + 2] << 16));
      if (s & 0x00800000)
        s |= 0xFF000000;
      return (float)s * (1.0f / 8388608.0f);
    }

    float convInt16(const void *src, long index)
    {
      const int16_t *ps = static_cast<const int16_t *>(src);
      return ps[index] * (1.0f / 32768.0f);
    }
  }
}
