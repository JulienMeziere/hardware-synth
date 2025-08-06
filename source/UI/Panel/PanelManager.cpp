#include <memory>

#include "PanelManager.h"
#include "../constants/includes.h"
#include "../../Logger.h"
#include "../../params.h"
#include "../../utils/includes.h"

namespace Newkon
{

  PanelManager::PanelManager() {}

  PanelManager::~PanelManager()
  {
    panels.clear(); // smart pointers will automatically clean up memory
  }

  void PanelManager::initialize(VSTGUI::VST3Editor *editor)
  {
    if (this->initialized)
      return;
    this->initialized = true;
    if (!editor)
      return;
    auto *frame = editor->getFrame();
    if (!frame)
      return;
    auto *view = frame->getView(0);
    if (!view)
      return;
    auto viewContainer = view->asViewContainer();
    if (!viewContainer || !viewContainer->hasChildren())
      return;

    const int numPanels = 8;
    for (int i = 0; i < numPanels; i++)
    {
      auto *pannelView = viewContainer->getView(i);
      if (!pannelView)
        return;
      auto *pannelContainerView = pannelView->asViewContainer();
      if (!pannelContainerView)
        return;
      auto *knobView = viewContainer->getView(i + numPanels);
      if (!knobView)
        return;
      VSTGUI::CKnob *knob = dynamic_cast<VSTGUI::CKnob *>(knobView);
      if (!knob)
        return;
      panels.push_back(std::make_unique<Panel>(pannelContainerView, knob, UIColors::toVstGuiCColor(UIColors::knobColors[i]), i == 0));
    }
    this->focusPannel(0);
  }

  void PanelManager::focusPannel(size_t index)
  {
    for (size_t i = 0; i < panels.size(); i++)
      i == index ? panels[i]->focus() : panels[i]->blur();
  }

} // namespace Newkon
