#include "colors.h"

namespace Newkon
{
  namespace UIColors
  {
    const Color background = {24, 24, 24};
    const Color gray = {47, 47, 47};

    const Color knobColors[] = {
        {255, 87, 51},
        {255, 171, 51},
        {255, 235, 59},
        {100, 221, 23},
        {0, 176, 255},
        {170, 0, 255},
        {255, 64, 129},
        {0, 229, 255},
    };

    VSTGUI::CColor toVstGuiCColor(const Color &color)
    {
      return VSTGUI::CColor(color.red, color.green, color.blue);
    }
  }
}
