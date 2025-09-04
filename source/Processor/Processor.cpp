//------------------------------------------------------------------------
// Copyright(c) 2023 Newkon.
//------------------------------------------------------------------------

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "public.sdk/source/vst/vstparameters.h"

#include "../Logger.h"
#include "./HardwareSynthesizer/MIDIDevices.h"

#include "Processor.h"
#include "../cids.h"
#include <immintrin.h>

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
		// Add audio input bus for ASIO interface audio
		addAudioInput(STR16("ASIO Input"), Steinberg::Vst::SpeakerArr::kStereo);
		addAudioOutput(STR16("Stereo Out"), Steinberg::Vst::SpeakerArr::kStereo);

		/* If you don't need an event bus, you can remove the next line */
		addEventInput(STR16("Event In"), 1);
		return kResultOk;
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthProcessor::terminate()
	{
		//---do not forget to call parent ------
		return AudioEffect::terminate();
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthProcessor::setActive(TBool state)
	{
		//--- called when the Plug-in is enable/disable (On/Off) -----
		if (!state)
		{
			asioInterface.stopAudioStream();
		}
		return AudioEffect::setActive(state);
	}

	//------------------------------------------------------------------------
	tresult PLUGIN_API HardwareSynthProcessor::process(Vst::ProcessData &data)
	{
		// React to ASIO driver reset requests (buffer size/sample rate changes)
		asioInterface.handlePendingReset();

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

		// Process MIDI events and forward to connected synthesizer (time-aware via scheduler)
		if (data.inputEvents && connectedSynthesizer)
		{
			auto baseNow = std::chrono::steady_clock::now();
			int32 numEvents = data.inputEvents->getEventCount();
			for (int32 i = 0; i < numEvents; i++)
			{
				Vst::Event event;
				if (data.inputEvents->getEvent(i, event) == kResultOk)
				{
					double offsetSeconds = 0.0;
					if (event.sampleOffset > 0 && sampleRate > 0.0)
						offsetSeconds = static_cast<double>(event.sampleOffset) / sampleRate;
					auto when = baseNow + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(offsetSeconds));

					if (event.type == Vst::Event::kNoteOnEvent)
					{
						UINT note = static_cast<UINT>(event.noteOn.pitch);
						UINT velocity = static_cast<UINT>(event.noteOn.velocity * 127.0f);
						UINT channel = static_cast<UINT>(event.noteOn.channel);
						connectedSynthesizer->scheduleMIDINoteAt(note, velocity, channel, when);
					}
					else if (event.type == Vst::Event::kNoteOffEvent)
					{
						UINT note = static_cast<UINT>(event.noteOff.pitch);
						UINT channel = static_cast<UINT>(event.noteOff.channel);
						connectedSynthesizer->scheduleMIDINoteOffAt(note, channel, when);
					}
					else if (event.type == Vst::Event::kDataEvent)
					{
						if (event.data.size >= 3)
						{
							UINT status = event.data.bytes[0];
							UINT data1 = event.data.bytes[1];
							UINT data2 = event.data.bytes[2];
							if ((status & 0xF0) == 0xB0)
							{
								UINT channel = status & 0x0F;
								connectedSynthesizer->scheduleMIDIControlChangeAt(data1, data2, channel, when);
							}
						}
					}
				}
			}
		}

		//--- Audio processing: Forward ASIO input to DAW output
		if (data.numSamples > 0 && data.outputs && data.outputs[0].numChannels >= 2)
		{
			// Simple policy: if enough samples are available now, copy; otherwise leave buffers as-is
			if (asioInterface.availableFrames() >= data.numSamples)
			{
				asioInterface.getAudioDataStereo(data.outputs[0].channelBuffers32[0], data.outputs[0].channelBuffers32[1], data.numSamples);
			}
			else
			{
				// leave host buffers untouched when not enough data
			}
		}

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
			return true;
		}
		return false;
	}

	//------------------------------------------------------------------------
	void HardwareSynthProcessor::disconnectSynthesizer()
	{
		if (connectedSynthesizer)
		{
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
