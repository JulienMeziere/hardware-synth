//------------------------------------------------------------------------
// Copyright(c) 2023 Newkon.
//------------------------------------------------------------------------

#include "pluginterfaces/base/ibstream.h"
#include "base/source/fstreamer.h"

#include "controller.h"
#include "../params.h"
#include "../cids.h"

#include "../Logger.h"
#include "../utils/includes.h"

using namespace Steinberg;
namespace Newkon
{

	//------------------------------------------------------------------------
	// HardwareSynthController Implementation
	//------------------------------------------------------------------------
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

		// Here you could register some parameters
		parameters.addParameter(STR16("Knob to CV 1"), STR16("%"), 0, DEFAULT_KNOB_VALUE, Vst::ParameterInfo::kCanAutomate, kKnobToCV1);
		// parameters.addParameter(STR16("Knob 1 Min"), STR16("%"), 0, DEFAULT_KNOB_VALUE, Vst::ParameterInfo::kCanAutomate, kKnob1Min);
		// parameters.addParameter(STR16("Knob 1 Max"), STR16("%"), 0, DEFAULT_KNOB_VALUE, Vst::ParameterInfo::kCanAutomate, kKnob1Max);

		parameters.addParameter(STR16("Knob to CV 2"), STR16("%"), 0, DEFAULT_KNOB_VALUE, Vst::ParameterInfo::kCanAutomate, kKnobToCV2);
		// parameters.addParameter(STR16("Knob 2 Min"), STR16("%"), 0, DEFAULT_KNOB_VALUE, Vst::ParameterInfo::kCanAutomate, kKnob2Min);
		// parameters.addParameter(STR16("Knob 2 Max"), STR16("%"), 0, DEFAULT_KNOB_VALUE, Vst::ParameterInfo::kCanAutomate, kKnob2Max);

		parameters.addParameter(STR16("Knob to CV 3"), STR16("%"), 0, DEFAULT_KNOB_VALUE, Vst::ParameterInfo::kCanAutomate, kKnobToCV3);
		parameters.addParameter(STR16("Knob to CV 4"), STR16("%"), 0, DEFAULT_KNOB_VALUE, Vst::ParameterInfo::kCanAutomate, kKnobToCV4);
		parameters.addParameter(STR16("Knob to CV 5"), STR16("%"), 0, DEFAULT_KNOB_VALUE, Vst::ParameterInfo::kCanAutomate, kKnobToCV5);
		parameters.addParameter(STR16("Knob to CV 6"), STR16("%"), 0, DEFAULT_KNOB_VALUE, Vst::ParameterInfo::kCanAutomate, kKnobToCV6);
		parameters.addParameter(STR16("Knob to Gate 1"), STR16("%"), 0, DEFAULT_KNOB_VALUE, Vst::ParameterInfo::kCanAutomate, kKnobToGate1);
		parameters.addParameter(STR16("Knob to Gate 2"), STR16("%"), 0, DEFAULT_KNOB_VALUE, Vst::ParameterInfo::kCanAutomate, kKnobToGate2);

		Logger::getInstance();

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

		float val;
		if (streamer.readFloat(val) == false)
			return kResultFalse;
		setParamNormalized(kKnobToCV1, val);
		// setParamNormalized(kKnob1Min, val);
		// setParamNormalized(kKnob1Max, val);

		setParamNormalized(kKnobToCV2, val);
		// setParamNormalized(kKnob2Min, val);
		// setParamNormalized(kKnob2Max, val);

		setParamNormalized(kKnobToCV3, val);
		setParamNormalized(kKnobToCV4, val);
		setParamNormalized(kKnobToCV5, val);
		setParamNormalized(kKnobToCV6, val);
		setParamNormalized(kKnobToGate1, val);
		setParamNormalized(kKnobToGate2, val);

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
			setTimeout([this]()
								 { this->panelManager->initialize(this->editor); },
								 500);
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

		// Check which parameter changed
		if (result == kResultOk)
		{
			switch (tag)
			{
			case kKnobToCV1:
				this->panelManager->focusPannel(0);
				break;
			case kKnobToCV2:
				this->panelManager->focusPannel(1);
				break;
			case kKnobToCV3:
				this->panelManager->focusPannel(2);
				break;
			case kKnobToCV4:
				this->panelManager->focusPannel(3);
				break;
			case kKnobToCV5:
				this->panelManager->focusPannel(4);
				break;
			case kKnobToCV6:
				this->panelManager->focusPannel(5);
				break;
			case kKnobToGate1:
				this->panelManager->focusPannel(6);
				break;
			case kKnobToGate2:
				this->panelManager->focusPannel(7);
				break;
			}
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
} // namespace Newkon
