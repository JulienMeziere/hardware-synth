//------------------------------------------------------------------------
// Copyright(c) 2023 Newkon.
//------------------------------------------------------------------------

#include "pluginterfaces/base/ibstream.h"
#include "base/source/fstreamer.h"

#include "controller.h"
#include "../params.h"
#include "../cids.h"

#include "../Logger.h"
#include "../Hardware/MIDIDevices.h"

// VSTGUI includes for dynamic UI creation
#include "vstgui4/vstgui/lib/vstguibase.h"

// VSTGUI UI classes
#include "vstgui4/vstgui/lib/cscrollview.h"
#include "vstgui4/vstgui/lib/controls/cbuttons.h"
#include "vstgui4/vstgui/lib/crect.h"
#include "vstgui4/vstgui/lib/ccolor.h"
#include "vstgui4/vstgui/lib/cpoint.h"
#include "vstgui4/vstgui/lib/cgradient.h"

// VSTGUI VST3 editor classes
#include "vstgui4/vstgui/plugin-bindings/vst3editor.h"

// UI colors
#include "constants/colors.h"

#include <functional>

using namespace Steinberg;
namespace Newkon
{

	//------------------------------------------------------------------------
	// HardwareSynthController Implementation
	//------------------------------------------------------------------------
	HardwareSynthController::HardwareSynthController()
	{
	}

	tresult PLUGIN_API HardwareSynthController::initialize(FUnknown *context)
	{
		// Here the Plug-in will be instantiated

		//---do not forget to call parent ------
		tresult result = EditControllerEx1::initialize(context);
		if (result != kResultOk)
		{
			return result;
		}

		setKnobMode(Vst::kLinearMode);

		// Detect MIDI devices
		midiDevices = MIDIDevices::listMIDIdevices();

		// Register your parameters here

		return result;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthController::terminate()
	{
		// Here the Plug-in will be de-instantiated, last possibility to remove some memory!

		Logger::killInstance();
		//---do not forget to call parent ------
		return EditControllerEx1::terminate();
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthController::setComponentState(IBStream *state)
	{
		// Here you get the state of the component (Processor part)
		if (!state)
			return kResultFalse;

		IBStreamer streamer(state, kLittleEndian);

		// Restore your parameter states here

		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthController::setState(IBStream *state)
	{
		// Here you get the state of the controller

		return kResultTrue;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthController::getState(IBStream *state)
	{
		// Here you are asked to deliver the state of the controller (if needed)
		// Note: the real state of your plug-in is saved in the processor

		return kResultTrue;
	}

	//------------------------------------------------------------------------
	IPlugView *PLUGIN_API HardwareSynthController::createView(FIDString name)
	{
		// Here the Host wants to open your editor (if you have one)
		if (FIDStringsEqual(name, Vst::ViewType::kEditor))
		{
			// create your editor here and return a IPlugView ptr of it
			this->editor = new VSTGUI::VST3Editor(this, "view", "editor.uidesc");

			// Create a custom delegate to handle UI events
			class ButtonCreationDelegate : public VSTGUI::VST3EditorDelegate
			{
			public:
				ButtonCreationDelegate(HardwareSynthController *controller) : controller(controller) {}

				void didOpen(VSTGUI::VST3Editor *editor) override
				{
					controller->createDeviceButtons();
					controller->updateUIState();
				}

			private:
				HardwareSynthController *controller;
			};

			// Set the delegate on the editor
			auto delegate = new ButtonCreationDelegate(this);
			dynamic_cast<VSTGUI::VST3Editor *>(this->editor)->setDelegate(delegate);

			return this->editor;
		}
		this->editor = nullptr;
		return nullptr;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthController::setParamNormalized(Vst::ParamID tag, Vst::ParamValue value)
	{
		// called by host to update your parameters
		tresult result = EditControllerEx1::setParamNormalized(tag, value);

		// Handle your parameter changes here
		if (result == kResultOk)
		{
			// Add your parameter cases here
		}

		return result;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthController::getParamStringByValue(Vst::ParamID tag, Vst::ParamValue valueNormalized, Vst::String128 string)
	{
		// called by host to get a string for given normalized value of a specific parameter
		// (without having to set the value!)
		return EditControllerEx1::getParamStringByValue(tag, valueNormalized, string);
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthController::getParamValueByString(Vst::ParamID tag, Vst::TChar *string, Vst::ParamValue &valueNormalized)
	{
		// called by host to get a normalized value from a string representation of a specific parameter
		// (without having to set the value!)
		return EditControllerEx1::getParamValueByString(tag, string, valueNormalized);
	}

	//------------------------------------------------------------------------
	void HardwareSynthController::createDeviceButtons()
	{
		if (!editor || midiDevices.empty())
		{
			Logger::getInstance() << "Cannot create buttons: editor=" << (editor ? "OK" : "null") << ", devices=" << midiDevices.size() << std::endl;
			return;
		}

		// Get the main frame
		auto *frame = editor->getFrame();
		if (!frame)
		{
			Logger::getInstance() << "Cannot create buttons: frame is null" << std::endl;
			return;
		}

		// Find the scroll view by searching through all views (including nested)
		VSTGUI::CScrollView *scrollView = nullptr;

		// Function to recursively search for scroll view
		std::function<void(VSTGUI::CViewContainer *)> findScrollView = [&](VSTGUI::CViewContainer *container)
		{
			if (!container)
				return;

			for (int32_t i = 0; i < container->getNbViews(); i++)
			{
				auto *view = container->getView(i);
				if (view)
				{
					auto *sv = dynamic_cast<VSTGUI::CScrollView *>(view);
					if (sv)
					{
						scrollView = sv;
						return;
					}

					// Recursively search in nested containers
					auto *nestedContainer = dynamic_cast<VSTGUI::CViewContainer *>(view);
					if (nestedContainer)
					{
						findScrollView(nestedContainer);
						if (scrollView)
							return; // Found it, stop searching
					}
				}
			}
		};

		// Search in the frame first
		findScrollView(frame);

		// Store reference to scroll view for later hiding
		this->scrollView = scrollView;

		// Use fallback container if no scroll view found
		if (!scrollView)
		{
			auto *container = dynamic_cast<VSTGUI::CViewContainer *>(frame->getView(0));
			if (!container)
			{
				Logger::getInstance() << "Cannot create buttons: no suitable container found" << std::endl;
				return;
			}
		}

		// Handle both scroll view and container fallback
		VSTGUI::CViewContainer *targetContainer = nullptr;
		bool isScrollView = (scrollView != nullptr);

		if (isScrollView)
		{
			targetContainer = scrollView;
		}
		else
		{
			// Use the fallback container
			targetContainer = dynamic_cast<VSTGUI::CViewContainer *>(frame->getView(0));
		}

		if (!targetContainer)
		{
			Logger::getInstance() << "Cannot create buttons: no suitable container found" << std::endl;
			return;
		}

		// Check if buttons are already created to avoid duplicates
		if (targetContainer->getNbViews() > 1)
		{
			return; // Buttons already exist
		}

		const int buttonHeight = 30;
		const int buttonSpacing = 5;
		int currentY = 10;

		// Get the available width from the target container
		VSTGUI::CRect containerSize = targetContainer->getViewSize();
		int availableWidth = containerSize.getWidth() - 20; // Leave 10px margin on each side

		for (size_t i = 0; i < midiDevices.size(); i++)
		{
			// Create button with full available width
			VSTGUI::CRect buttonRect(10, currentY, 10 + availableWidth, currentY + buttonHeight);
			auto *button = new VSTGUI::CTextButton(buttonRect, this, static_cast<int32_t>(1000 + i), midiDevices[i].c_str());

			// Style the button with gray background using gradient
			auto grayGradient = VSTGUI::owned(VSTGUI::CGradient::create(0.0, 1.0,
																																	UIColors::toVstGuiCColor(UIColors::gray),
																																	UIColors::toVstGuiCColor(UIColors::gray)));
			button->setGradient(grayGradient);

			// Set highlighted gradient to background color when clicked
			auto backgroundGradient = VSTGUI::owned(VSTGUI::CGradient::create(0.0, 1.0,
																																				UIColors::toVstGuiCColor(UIColors::background),
																																				UIColors::toVstGuiCColor(UIColors::background)));
			button->setGradientHighlighted(backgroundGradient);

			button->setFrameColor(VSTGUI::CColor(128, 128, 128));
			button->setFrameColorHighlighted(VSTGUI::CColor(128, 128, 128));
			button->setTextColor(VSTGUI::CColor(255, 255, 255));

			// Add to target container
			targetContainer->addView(button);

			currentY += buttonHeight + buttonSpacing;
		}

		// Update container size if it's a scroll view
		if (isScrollView)
		{
			int containerHeight = currentY + 10;
			scrollView->setContainerSize(VSTGUI::CRect(0, 0, 220, containerHeight));
		}

		// Refresh the view
		targetContainer->setDirty(true);
	}

	//------------------------------------------------------------------------
	void HardwareSynthController::valueChanged(VSTGUI::CControl *pControl)
	{
		if (!pControl)
			return;

		int32_t tag = static_cast<int32_t>(pControl->getTag());

		// Only handle button press (value = 1.0), not release (value = 0.0)
		if (pControl->getValue() != 1.0f)
			return;

		if (tag >= 1000)
		{
			size_t deviceIndex = tag - 1000;
			if (deviceIndex < midiDevices.size())
			{
				// Connect to the selected device via processor
				if (auto *processor = getProcessor())
				{
					if (processor->connectToSynthesizer(deviceIndex))
					{
						// Update UI state based on processor
						updateUIState();
					}
				}
			}
		}
	}

	//------------------------------------------------------------------------
	void HardwareSynthController::updateUIState()
	{
		if (auto *processor = getProcessor())
		{
			if (processor->isSynthesizerConnected())
			{
				// Hide scroll view when synthesizer is connected
				if (scrollView)
				{
					scrollView->setVisible(false);
				}
			}
			else
			{
				// Show scroll view when no synthesizer is connected
				if (scrollView)
				{
					scrollView->setVisible(true);
				}
			}
		}
	}

	//------------------------------------------------------------------------
	HardwareSynthProcessor *HardwareSynthController::getProcessor()
	{
		// Get the processor using static method
		return HardwareSynthProcessor::getCurrentInstance();
	}

	//------------------------------------------------------------------------
} // namespace Newkon
