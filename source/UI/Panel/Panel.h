#pragma once

#include "vstgui/vstgui.h"

namespace Newkon
{

  class Panel
  {
  public:
    Panel(VSTGUI::CViewContainer *view, VSTGUI::CKnob *knob, VSTGUI::CColor color, bool visible = false);

    void focus();
    void blur();

  private:
    VSTGUI::CViewContainer *view = nullptr;
    VSTGUI::CColor mainColor;
    VSTGUI::CKnob *mainKnob = nullptr;
    bool visible = false;
  };

} // namespace Newkon
