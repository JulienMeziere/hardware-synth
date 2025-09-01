//------------------------------------------------------------------------
// Copyright(c) 2023 Newkon.
//------------------------------------------------------------------------

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstcomponent.h"
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
		// Read inputs parameter changes
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
						// Handle your parameter changes here
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
		bufferSize = newSetup.maxSamplesPerBlock;
		Logger::getInstance() << "Buffer size: " << bufferSize << " samples" << std::endl;
		Logger::getInstance() << "Sample rate: " << sampleRate << " Hz" << std::endl;

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

		// Restore your parameter states here

		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthProcessor::getState(IBStream *state)
	{
		// here we need to save the model
		IBStreamer streamer(state, kLittleEndian);

		// Save your parameter states here

		return kResultOk;
	}

	//------------------------------------------------------------------------
	uint32 PLUGIN_API HardwareSynthProcessor::getLatencySamples()
	{
		return static_cast<uint32>(sampleRate * currentLatencySeconds);
	}

	//------------------------------------------------------------------------
	void HardwareSynthProcessor::setLatency(double latencySeconds)
	{
		if (currentLatencySeconds != latencySeconds)
		{
			currentLatencySeconds = latencySeconds;
			Logger::getInstance() << "Latency changed to: " << latencySeconds << " seconds" << std::endl;

			// Notify host that latency has changed
			// This forces FL Studio to restart audio processing
			if (auto *host = getHostContext())
			{
				Steinberg::FUnknownPtr<Steinberg::Vst::IComponentHandler> componentHandler(host);
				if (componentHandler)
				{
					componentHandler->restartComponent(Steinberg::Vst::kLatencyChanged);
				}
			}
		}
	}

	//------------------------------------------------------------------------
} // namespace Newkon
