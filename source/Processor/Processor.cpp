//------------------------------------------------------------------------
// Copyright(c) 2023 Newkon.
//------------------------------------------------------------------------

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "public.sdk/source/vst/vstparameters.h"

#include "../Logger.h"
#include "../Hardware/MIDIDevices.h"

#include "Processor.h"
#include "../cids.h"

using namespace Steinberg;

namespace Newkon
{
	// Static member initialization
	HardwareSynthProcessor *HardwareSynthProcessor::currentInstance = nullptr;
	//------------------------------------------------------------------------
	// HardwareSynthProcessor
	//------------------------------------------------------------------------
	HardwareSynthProcessor::HardwareSynthProcessor()
	{
		//--- set the wanted controller for our processor
		setControllerClass(kHardwareSynthControllerUID);

		// Set static instance reference
		currentInstance = this;
	}

	//------------------------------------------------------------------------
	HardwareSynthProcessor::~HardwareSynthProcessor()
	{
		// Clear static instance reference
		if (currentInstance == this)
		{
			currentInstance = nullptr;
		}
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

		// Process MIDI events and forward to connected synthesizer
		if (data.inputEvents && connectedSynthesizer)
		{
			int32 numEvents = data.inputEvents->getEventCount();
			for (int32 i = 0; i < numEvents; i++)
			{
				Vst::Event event;
				if (data.inputEvents->getEvent(i, event) == kResultOk)
				{
					// Forward MIDI events to connected synthesizer
					if (event.type == Vst::Event::kNoteOnEvent)
					{
						// Convert VST3 note on to MIDI note on
						UINT note = static_cast<UINT>(event.noteOn.pitch);
						UINT velocity = static_cast<UINT>(event.noteOn.velocity * 127.0f);
						UINT channel = static_cast<UINT>(event.noteOn.channel);

						connectedSynthesizer->sendMIDINote(note, velocity, channel);
					}
					else if (event.type == Vst::Event::kNoteOffEvent)
					{
						// Convert VST3 note off to MIDI note off
						UINT note = static_cast<UINT>(event.noteOff.pitch);
						UINT channel = static_cast<UINT>(event.noteOff.channel);

						connectedSynthesizer->sendMIDINoteOff(note, channel);
					}
					else if (event.type == Vst::Event::kDataEvent)
					{
						// Handle MIDI CC and other data events
						if (event.data.size >= 3)
						{
							UINT status = event.data.bytes[0];
							UINT data1 = event.data.bytes[1];
							UINT data2 = event.data.bytes[2];

							// Check if it's a Control Change message (0xB0-0xBF)
							if ((status & 0xF0) == 0xB0)
							{
								UINT channel = status & 0x0F;
								connectedSynthesizer->sendMIDIControlChange(data1, data2, channel);
							}
						}
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

		// Restore hardware synthesizer connection state
		int32 connected = 0;
		if (streamer.readInt32(connected) == kResultOk)
		{
			if (connected == 1)
			{
				// Restore device index
				int32 deviceIndex = 0;
				if (streamer.readInt32(deviceIndex) == kResultOk)
				{
					// Reconnect to the saved device
					connectToSynthesizer(static_cast<size_t>(deviceIndex));
				}
			}
		}

		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthProcessor::getState(IBStream *state)
	{
		// here we need to save the model
		IBStreamer streamer(state, kLittleEndian);

		// Save hardware synthesizer connection state
		if (connectedSynthesizer)
		{
			// Save that we have a connection
			streamer.writeInt32(1);

			// Find the device index by comparing device names
			auto devices = MIDIDevices::listMIDIdevices();
			int32 deviceIndex = -1;
			for (size_t i = 0; i < devices.size(); i++)
			{
				if (devices[i] == connectedSynthesizer->getDeviceName())
				{
					deviceIndex = static_cast<int32>(i);
					break;
				}
			}
			streamer.writeInt32(deviceIndex);
		}
		else
		{
			// Save that we have no connection
			streamer.writeInt32(0);
		}

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
	bool HardwareSynthProcessor::connectToSynthesizer(size_t deviceIndex)
	{
		// Disconnect current synthesizer if any
		disconnectSynthesizer();

		// Connect to new synthesizer
		connectedSynthesizer = std::move(MIDIDevices::connectToDevice(deviceIndex));
		if (connectedSynthesizer)
		{
			Logger::getInstance() << "Connected to: " + connectedSynthesizer->getDeviceName() << std::endl;
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------
	void HardwareSynthProcessor::disconnectSynthesizer()
	{
		if (connectedSynthesizer)
		{
			Logger::getInstance() << "Disconnected from: " + connectedSynthesizer->getDeviceName() << std::endl;
			connectedSynthesizer.reset();
		}
	}

	//------------------------------------------------------------------------
	bool HardwareSynthProcessor::isSynthesizerConnected() const
	{
		return connectedSynthesizer != nullptr;
	}

	//------------------------------------------------------------------------
	std::string HardwareSynthProcessor::getConnectedSynthesizerName() const
	{
		if (connectedSynthesizer)
		{
			return connectedSynthesizer->getDeviceName();
		}
		return "";
	}

	//------------------------------------------------------------------------
	HardwareSynthProcessor *HardwareSynthProcessor::getCurrentInstance()
	{
		return currentInstance;
	}

	//------------------------------------------------------------------------
} // namespace Newkon
