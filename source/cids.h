//------------------------------------------------------------------------
// Copyright(c) 2023 Newkon.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace Newkon
{
  //------------------------------------------------------------------------
  static const Steinberg::FUID kHardwareSynthProcessorUID(0x0A423737, 0x6BBC558E, 0x8736B74E, 0xE4D15F73);
  static const Steinberg::FUID kHardwareSynthControllerUID(0x61CE2421, 0x9F1858C3, 0x99F5797F, 0x18A862C5);

#define HardwareSynthVST3Category "Instrument"

  //------------------------------------------------------------------------
} // namespace Newkon
