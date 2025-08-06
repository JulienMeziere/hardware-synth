#pragma once

#include <memory>
#include <vector>
#include "vstgui/plugin-bindings/vst3editor.h"
#include "Panel.h"

namespace Newkon
{

  class PanelManager
  {
  public:
    PanelManager();
    ~PanelManager();

    void initialize(VSTGUI::VST3Editor *editor);

    // Method to show or hide a specific panel by index
    void focusPannel(size_t index);

  private:
    std::vector<std::unique_ptr<Panel>> panels;
    bool initialized = false;
  };

} // namespace Newkon
