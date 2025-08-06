//------------------------------------------------------------------------
// Copyright(c) 2023 Newkon.
//------------------------------------------------------------------------

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "public.sdk/source/vst/vstparameters.h"

#include "../Logger.h"

#include "Processor.h"
#include "../cids.h"

using namespace Steinberg;

namespace Newkon
{
	//------------------------------------------------------------------------
	// HardwareSynthProcessor
	//------------------------------------------------------------------------
	HardwareSynthProcessor::HardwareSynthProcessor()
	{
		//--- set the wanted controller for our processor
		setControllerClass(kHardwareSynthControllerUID);
	}

	//------------------------------------------------------------------------
	HardwareSynthProcessor::~HardwareSynthProcessor()
	{
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthProcessor::initialize(FUnknown *context)
	{
		// Here the Plug-in will be instantiated

		//---always initialize the parent-------
		tresult result = AudioEffect::initialize(context);
		// if everything Ok, continue
		if (result != kResultOk)
		{
			return result;
		}

		//--- create Audio IO ------
		addAudioInput(STR16("Stereo In"), Steinberg::Vst::SpeakerArr::kStereo);
		addAudioOutput(STR16("Stereo Out"), Steinberg::Vst::SpeakerArr::kStereo);

		/* If you don't need an event bus, you can remove the next line */
		addEventInput(STR16("Event In"), 1);
		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthProcessor::terminate()
	{
		// Here the Plug-in will be de-instantiated, last possibility to remove some memory!

		//---do not forget to call parent ------
		return AudioEffect::terminate();
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthProcessor::setActive(TBool state)
	{
		//--- called when the Plug-in is enable/disable (On/Off) -----
		return AudioEffect::setActive(state);
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthProcessor::process(Vst::ProcessData &data)
	{
		//--- Read inputs parameter changes-----------
		if (data.inputParameterChanges)
		{
			int32 numParamsChanged = data.inputParameterChanges->getParameterCount();
			for (int32 index = 0; index < numParamsChanged; index++)
			{
				if (auto *paramQueue = data.inputParameterChanges->getParameterData(index))
				{
					Vst::ParamValue value;
					int32 sampleOffset;
					int32 numPoints = paramQueue->getPointCount();
					paramQueue->getPoint(numPoints - 1, sampleOffset, value);
					switch (paramQueue->getParameterId())
					{
					case kKnobToCV1:
						this->knob1Val = (float)value;
						// controller.setParamValue(0, this->knob1Val);
						break;
					case kKnobToCV2:
						this->knob2Val = (float)value;
						// controller.setParamValue(1, this->knob2Val);
						break;
					case kKnobToCV3:
						this->knob3Val = (float)value;
						// controller.setParamValue(2, this->knob3Val);
						break;
					case kKnobToCV4:
						this->knob4Val = (float)value;
						// controller.setParamValue(3, this->knob4Val);
						break;
					case kKnobToCV5:
						this->knob5Val = (float)value;
						// controller.setParamValue(4, this->knob5Val);
						break;
					case kKnobToCV6:
						this->knob6Val = (float)value;
						// controller.setParamValue(5, this->knob6Val);
						break;
					case kKnobToGate1:
						this->knob7Val = (float)value;
						// controller.setParamValue(6, this->knob7Val);
						break;
					case kKnobToGate2:
						this->knob8Val = (float)value;
						// controller.setParamValue(7, this->knob8Val);
						break;
					}
				}
			}
		}

		//--- Here you have to implement your processing
		// Vst::Sample32 *outL = data.outputs[0].channelBuffers32[0];
		// Vst::Sample32 *outR = data.outputs[0].channelBuffers32[1];
		// for (int32 i = 0; i < data.numSamples; i++)
		// {
		// 	outL[i] = 0;
		// 	outR[i] = 0;
		// }

		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthProcessor::setupProcessing(Vst::ProcessSetup &newSetup)
	{
		//--- called before any processing ----
		sampleRate = newSetup.sampleRate;
		return AudioEffect::setupProcessing(newSetup);
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthProcessor::canProcessSampleSize(int32 symbolicSampleSize)
	{
		// by default kSample32 is supported
		if (symbolicSampleSize == Vst::kSample32)
			return kResultTrue;

		// disable the following comment if your processing support kSample64
		/* if (symbolicSampleSize == Vst::kSample64)
			return kResultTrue; */

		return kResultFalse;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthProcessor::setState(IBStream *state)
	{
		// called when we load a preset, the model has to be reloaded
		IBStreamer streamer(state, kLittleEndian);

		float val;
		if (streamer.readFloat(val) == false)
			return kResultFalse;
		this->knob1Val = val;
		if (streamer.readFloat(val) == false)
			return kResultFalse;
		this->knob2Val = val;
		if (streamer.readFloat(val) == false)
			return kResultFalse;
		this->knob3Val = val;
		if (streamer.readFloat(val) == false)
			return kResultFalse;
		this->knob4Val = val;
		if (streamer.readFloat(val) == false)
			return kResultFalse;
		this->knob5Val = val;
		if (streamer.readFloat(val) == false)
			return kResultFalse;
		this->knob6Val = val;
		if (streamer.readFloat(val) == false)
			return kResultFalse;
		this->knob7Val = val;
		if (streamer.readFloat(val) == false)
			return kResultFalse;
		this->knob8Val = val;

		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthProcessor::getState(IBStream *state)
	{
		// here we need to save the model
		IBStreamer streamer(state, kLittleEndian);

		streamer.writeFloat(this->knob1Val);
		streamer.writeFloat(this->knob2Val);
		streamer.writeFloat(this->knob3Val);
		streamer.writeFloat(this->knob4Val);
		streamer.writeFloat(this->knob5Val);
		streamer.writeFloat(this->knob6Val);
		streamer.writeFloat(this->knob7Val);
		streamer.writeFloat(this->knob8Val);

		return kResultOk;
	}

	//------------------------------------------------------------------------
	uint32 PLUGIN_API HardwareSynthProcessor::getLatencySamples()
	{
		return static_cast<uint32>(sampleRate * 1.0);
	}

	//------------------------------------------------------------------------
} // namespace Newkon
