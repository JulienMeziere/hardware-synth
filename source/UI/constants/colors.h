#pragma once

#include "vstgui/vstgui.h"

#include "../../params.h"

namespace Newkon
{
  namespace UIColors
  {
    struct Color
    {
      uint8_t red;
      uint8_t green;
      uint8_t blue;
    };

    extern const Color background;
    extern const Color gray;

    // Declare the array, do not provide values here
    extern const Color knobColors[];

    VSTGUI::CColor toVstGuiCColor(const Color &color);
  } // namespace UIColors
} // namespace Newkon
