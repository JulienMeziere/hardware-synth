//------------------------------------------------------------------------
// Copyright(c) 2023 Newkon.
//------------------------------------------------------------------------

#include "pluginterfaces/base/ibstream.h"
#include "base/source/fstreamer.h"

#include "controller.h"
#include "../params.h"
#include "../cids.h"

#include "../Logger.h"
#include "../Processor/HardwareSynthesizer/MIDIDevices.h"

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
		static bool firstTime = true;
		Logger::getInstance() << "Creating view: " << name << std::endl;
		// Here the Host wants to open your editor (if you have one)
		if (FIDStringsEqual(name, Vst::ViewType::kEditor))
		{
			// create your editor here and return a IPlugView ptr of it
			this->editor = new VSTGUI::VST3Editor(this, "view", "editor.uidesc");
			if (!firstTime)
			{
				return this->editor;
			}
			firstTime = false;

			// Create a custom delegate to handle UI events
			class ButtonCreationDelegate : public VSTGUI::VST3EditorDelegate
			{
			public:
				ButtonCreationDelegate(HardwareSynthController *controller) : controller(controller) {}

				void didOpen(VSTGUI::VST3Editor *editor) override
				{
					if (editor && editor->getFrame())
					{
						auto *frame = editor->getFrame();
						auto *firstView = frame->getView(0);
						if (firstView)
						{
							auto *container = dynamic_cast<VSTGUI::CViewContainer *>(firstView);
							if (container && container->getNbViews() >= 3)
							{
								auto *secondScrollView = dynamic_cast<VSTGUI::CScrollView *>(container->getView(1));
								if (secondScrollView)
									secondScrollView->setVisible(false);
								auto *thirdScrollView = dynamic_cast<VSTGUI::CScrollView *>(container->getView(2));
								if (thirdScrollView)
									thirdScrollView->setVisible(false);
							}
						}
					}

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

		// If no MIDI devices, try to get them again
		if (midiDevices.empty())
		{
			midiDevices = MIDIDevices::listMIDIdevices();
		}

		if (!editor || midiDevices.empty())
		{

			return;
		}

		// Get the main frame
		auto *frame = editor->getFrame();
		if (!frame)
		{
			Logger::getInstance() << "Cannot create buttons: frame is null" << std::endl;
			return;
		}

		// Find the first scroll view (MIDI devices) in the container
		VSTGUI::CScrollView *scrollView = nullptr;

		// Get the first view (container) and look for the first scrollview inside it
		if (frame->getNbViews() >= 1)
		{
			auto *firstView = frame->getView(0);
			if (firstView)
			{
				auto *container = dynamic_cast<VSTGUI::CViewContainer *>(firstView);
				if (container && container->getNbViews() >= 1)
				{
					scrollView = dynamic_cast<VSTGUI::CScrollView *>(container->getView(0));
				}
			}
		}
		// Store reference to scroll view for later hiding
		this->scrollView = scrollView;

		if (!scrollView)
		{
			Logger::getInstance() << "No scroll view found!" << std::endl;
			return;
		}

		// Use scroll view as target container
		VSTGUI::CViewContainer *targetContainer = scrollView;

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
		if (scrollView)
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

		// Handle MIDI device buttons (1000-1007)
		if (tag >= 1000 && tag < 2000)
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
		// Handle ASIO interface buttons (2000-2007)
		else if (tag >= 2000 && tag < 3000)
		{
			size_t interfaceIndex = tag - 2000;
			if (interfaceIndex < asioInterfaces.size())
			{
				selectedAsioInterface = static_cast<int>(interfaceIndex);
				auto *processor = HardwareSynthProcessor::getCurrentInstance();
				if (processor && processor->getAsioInterface().connectToInterface(static_cast<int>(interfaceIndex)))
				{
					showAsioInputs();
				}
			}
		}
		// Handle ASIO input buttons (3000-3007)
		else if (tag >= 3000 && tag < 4000)
		{
			size_t inputIndex = tag - 3000;
			if (inputIndex < asioInputs.size())
			{
				auto *processor2 = HardwareSynthProcessor::getCurrentInstance();
				if (processor2 && processor2->getAsioInterface().connectToInput(static_cast<int>(inputIndex)))
				{
					Logger::getInstance() << "Starting audio streaming from the controller" << std::endl;
					if (!processor2->getAsioInterface().startAudioStream())
					{
						Logger::getInstance() << "Failed to start audio streaming" << std::endl;
					}
				}
				else
				{
					Logger::getInstance() << "Failed to connect to ASIO input" << std::endl;
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
				// Hide MIDI devices scroll view when synthesizer is connected
				if (scrollView)
				{
					scrollView->setVisible(false);
				}

				// Show audio inputs scroll view
				showAudioInputs();
			}
			else
			{
				// Show MIDI devices scroll view when no synthesizer is connected
				if (scrollView)
				{
					scrollView->setVisible(true);
				}

				// Hide audio inputs scroll view
				if (audioInputsScrollView)
				{
					audioInputsScrollView->setVisible(false);
				}
				else
				{
					// If audioInputsScrollView is not yet initialized, get it and hide it
					if (editor && editor->getFrame())
					{
						audioInputsScrollView = dynamic_cast<VSTGUI::CScrollView *>(editor->getFrame()->getView(1));
						if (audioInputsScrollView)
						{
							audioInputsScrollView->setVisible(false);
						}
					}
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
	void HardwareSynthController::showAudioInputs()
	{
		if (!audioInputsScrollView)
		{
			// Get audio inputs scroll view from UI (second scrollview in container)
			if (editor && editor->getFrame())
			{
				auto *frame = editor->getFrame();
				if (frame->getNbViews() >= 1)
				{
					auto *firstView = frame->getView(0);
					if (firstView)
					{
						auto *container = dynamic_cast<VSTGUI::CViewContainer *>(firstView);
						if (container && container->getNbViews() >= 2)
						{
							audioInputsScrollView = dynamic_cast<VSTGUI::CScrollView *>(container->getView(1));
							if (audioInputsScrollView)
							{
								// Get list of ASIO interfaces
								auto *processor = HardwareSynthProcessor::getCurrentInstance();
								asioInterfaces = processor ? processor->getAsioInterface().listAsioInterfaces() : std::vector<std::string>{};

								// Create buttons for ASIO interfaces
								createAsioInterfaceButtons();

								// Show the scroll view
								audioInputsScrollView->setVisible(true);
							}
						}
					}
				}
			}
		}
		else
		{
			audioInputsScrollView->setVisible(true);
		}
	}

	//------------------------------------------------------------------------
	void HardwareSynthController::showAsioInputs()
	{
		if (!asioInputsScrollView)
		{
			// Find ASIO inputs scroll view by label (third scrollview)
			if (editor && editor->getFrame())
			{
				auto *frame = editor->getFrame();
				if (frame->getNbViews() >= 1)
				{
					auto *firstView = frame->getView(0);
					if (firstView)
					{
						auto *container = dynamic_cast<VSTGUI::CViewContainer *>(firstView);
						if (container && container->getNbViews() >= 3)
						{
							asioInputsScrollView = dynamic_cast<VSTGUI::CScrollView *>(container->getView(2));
							if (asioInputsScrollView)
							{
								// Get list of ASIO inputs for the selected interface
								auto *processor2 = HardwareSynthProcessor::getCurrentInstance();
								asioInputs = processor2 ? processor2->getAsioInterface().getAsioInputs(selectedAsioInterface) : std::vector<std::string>{};

								// Create buttons for ASIO inputs
								createAsioInputButtons();

								// Show the scroll view
								asioInputsScrollView->setVisible(true);
							}
						}
					}
				}
			}
		}
		else
		{
			asioInputsScrollView->setVisible(true);
		}
	}

	//------------------------------------------------------------------------
	void HardwareSynthController::createAsioInterfaceButtons()
	{
		if (!audioInputsScrollView || asioInterfaces.empty())
			return;

		// Use the scroll view directly as container (same pattern as MIDI devices)
		VSTGUI::CViewContainer *container = audioInputsScrollView;

		// Clear existing buttons
		container->removeAll();

		// Get scroll view dimensions
		VSTGUI::CRect scrollRect = audioInputsScrollView->getViewSize();
		const int buttonHeight = 30;
		const int buttonSpacing = 5;
		int currentY = 10;
		int availableWidth = scrollRect.getWidth() - 20; // Leave 10px margin on each side

		// Create buttons for each ASIO interface
		for (size_t i = 0; i < asioInterfaces.size(); i++)
		{
			VSTGUI::CTextButton *button = new VSTGUI::CTextButton(
					VSTGUI::CRect(10, currentY, 10 + availableWidth, currentY + buttonHeight),
					this, static_cast<int32_t>(kAudioInputButton0 + static_cast<int32_t>(i)), asioInterfaces[i].c_str());

			// Style the button (same as MIDI device buttons)
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

			container->addView(button);
			currentY += buttonHeight + buttonSpacing;
		}

		// Update container size
		int containerHeight = currentY + 10;
		audioInputsScrollView->setContainerSize(VSTGUI::CRect(0, 0, 220, containerHeight));

		// Refresh the view
		container->setDirty(true);
	}

	//------------------------------------------------------------------------
	void HardwareSynthController::createAsioInputButtons()
	{
		if (!asioInputsScrollView || asioInputs.empty())
			return;

		// Use the scroll view directly as container (same pattern as MIDI devices)
		VSTGUI::CViewContainer *container = asioInputsScrollView;

		// Clear existing buttons
		container->removeAll();

		// Get scroll view dimensions
		VSTGUI::CRect scrollRect = asioInputsScrollView->getViewSize();
		const int buttonHeight = 30;
		const int buttonSpacing = 5;
		int currentY = 10;
		int availableWidth = scrollRect.getWidth() - 20; // Leave 10px margin on each side

		// Create buttons for each ASIO input
		for (size_t i = 0; i < asioInputs.size(); i++)
		{
			VSTGUI::CTextButton *button = new VSTGUI::CTextButton(
					VSTGUI::CRect(10, currentY, 10 + availableWidth, currentY + buttonHeight),
					this, static_cast<int32_t>(kAsioInputButton0 + static_cast<int32_t>(i)), asioInputs[i].c_str());

			// Style the button (same as MIDI device buttons)
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

			container->addView(button);
			currentY += buttonHeight + buttonSpacing;
		}

		// Update container size
		int containerHeight = currentY + 10;
		asioInputsScrollView->setContainerSize(VSTGUI::CRect(0, 0, 220, containerHeight));

		// Refresh the view
		container->setDirty(true);
	}

	//------------------------------------------------------------------------
} // namespace Newkon
