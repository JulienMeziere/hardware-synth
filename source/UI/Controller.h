//------------------------------------------------------------------------
// Copyright(c) 2023 Newkon.
//------------------------------------------------------------------------

#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui4/vstgui/lib/vstguibase.h"

// VSTGUI UI classes
#include "vstgui4/vstgui/lib/cscrollview.h"
#include "vstgui4/vstgui/lib/controls/cbuttons.h"
#include "vstgui4/vstgui/lib/controls/icontrollistener.h"
#include "vstgui4/vstgui/lib/crect.h"
#include "vstgui4/vstgui/lib/ccolor.h"
#include "vstgui4/vstgui/lib/cpoint.h"

#include <vector>
#include <string>
#include <memory>

#include "../Processor/HardwareSynthesizer/HardwareSynthesizer.h"
#include "../Processor/Processor.h"

namespace Newkon
{

	//------------------------------------------------------------------------
	//  HardwareSynthController
	//------------------------------------------------------------------------
	class HardwareSynthController : public Steinberg::Vst::EditControllerEx1, public VSTGUI::IControlListener
	{
	public:
		//------------------------------------------------------------------------
		HardwareSynthController();
		~HardwareSynthController() SMTG_OVERRIDE = default;

		// Create function
		static Steinberg::FUnknown *createInstance(void * /*context*/)
		{
			return (Steinberg::Vst::IEditController *)new HardwareSynthController;
		}

		// IPluginBase
		Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown *context) SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream *state) SMTG_OVERRIDE;

		// EditController
		Steinberg::IPlugView *PLUGIN_API createView(Steinberg::FIDString name) SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream *state) SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream *state) SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API setParamNormalized(Steinberg::Vst::ParamID tag,
																										 Steinberg::Vst::ParamValue value) SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API getParamStringByValue(Steinberg::Vst::ParamID tag,
																												Steinberg::Vst::ParamValue valueNormalized,
																												Steinberg::Vst::String128 string) SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API getParamValueByString(Steinberg::Vst::ParamID tag,
																												Steinberg::Vst::TChar *string,
																												Steinberg::Vst::ParamValue &valueNormalized) SMTG_OVERRIDE;

		// IControlListener
		void valueChanged(VSTGUI::CControl *pControl) SMTG_OVERRIDE;

		/** Update UI based on processor state */
		void updateUIState();

		/** Get the processor instance */
		HardwareSynthProcessor *getProcessor();

		/** Show audio inputs scrollview */
		void showAudioInputs();

		//---Interface---------
		DEFINE_INTERFACES
		// Here you can add more supported VST3 interfaces
		// DEF_INTERFACE (Vst::IXXX)
		END_DEFINE_INTERFACES(EditController)
		DELEGATE_REFCOUNT(EditController)

		//------------------------------------------------------------------------
	private:
		void createDeviceButtons();
		void createAsioInterfaceButtons();
		void showAsioInputs();
		void createAsioInputButtons();

		//------------------------------------------------------------------------
	private:
		VSTGUI::VST3Editor *editor = nullptr;
		std::vector<std::string> midiDevices;
		std::vector<std::string> asioInterfaces;
		std::vector<std::string> asioInputs;
		int selectedAsioInterface = -1; // Store selected ASIO interface index
		VSTGUI::CScrollView *scrollView = nullptr;
		VSTGUI::CScrollView *audioInputsScrollView = nullptr;
		VSTGUI::CScrollView *asioInputsScrollView = nullptr;
	};

	//------------------------------------------------------------------------
} // namespace Newkon
