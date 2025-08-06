#include "Panel.h"

#include "../constants/includes.h"

namespace Newkon
{

  Panel::Panel(VSTGUI::CViewContainer *view, VSTGUI::CKnob *knob, VSTGUI::CColor color, bool initVisible) : view(view), mainKnob(knob), mainColor(color), visible(!initVisible) {}

  void Panel::focus()
  {
    if (!view || !mainKnob || visible)
      return;

    view->setVisible(true);
    mainKnob->setColorShadowHandle(UIColors::toVstGuiCColor(UIColors::background));
    visible = true;
  }

  void Panel::blur()
  {
    if (!view || !mainKnob || !visible)
      return;

    view->setVisible(false);
    mainKnob->setColorShadowHandle(UIColors::toVstGuiCColor(UIColors::gray));
    visible = false;
  }

} // namespace Newkon
