//------------------------------------------------------------------------
// Copyright(c) 2023 Newkon.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace Newkon
{
  //------------------------------------------------------------------------
  static const Steinberg::FUID kHardwareSynthProcessorUID(0x12345678, 0x9ABCDEF0, 0x11223344, 0x55667788);
  static const Steinberg::FUID kHardwareSynthControllerUID(0x87654321, 0x0FEDCBA9, 0x44332211, 0x88776655);

#define HardwareSynthVST3Category "Instrument"

  //------------------------------------------------------------------------
} // namespace Newkon
