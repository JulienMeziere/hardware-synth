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
#include "../Hardware/HardwareSynthesizer.h"

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

		// Register your parameters here

		Logger::getInstance();

		// List MIDI devices
		Logger::getInstance() << "=== MIDI Device Detection (Controller) ===" << std::endl;
		std::vector<std::string> midiDevices = MIDIDevices::listMIDIdevices();
		Logger::getInstance() << "Total MIDI devices found: " << midiDevices.size() << std::endl;
		Logger::getInstance() << "=== End MIDI Device Detection ===" << std::endl;

		// Connect to device index 2 (Neutron)
		Logger::getInstance() << "=== Connecting to Device Index 2 ===" << std::endl;
		auto synthesizer = MIDIDevices::connectToDevice(2);
		if (synthesizer)
		{
			Logger::getInstance() << "Successfully connected to: " << synthesizer->getDeviceName() << std::endl;
			Logger::getInstance() << "Starting A4 note loop (500ms on/off)..." << std::endl;

			// Send A4 note (note 69) in a loop
			for (int i = 0; i < 10; i++) // Send 10 cycles for testing
			{
				// Note on
				synthesizer->sendMIDINote(69, 100, 0);
				Logger::getInstance() << "A4 ON" << std::endl;

				// Wait 500ms (simplified - in real implementation you'd use proper timing)
				Sleep(500);

				// Note off (proper MIDI Note Off message)
				synthesizer->sendMIDINoteOff(69, 0);
				Logger::getInstance() << "A4 OFF" << std::endl;

				// Wait 500ms
				Sleep(500);
			}

			Logger::getInstance() << "A4 note loop completed" << std::endl;
		}
		else
		{
			Logger::getInstance() << "Failed to connect to device index 2" << std::endl;
		}
		Logger::getInstance() << "=== End Connection ===" << std::endl;

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
			switch (tag)
			{
				// Add your parameter cases here
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
