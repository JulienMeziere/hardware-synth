//------------------------------------------------------------------------
// Copyright(c) 2023 Newkon.
//------------------------------------------------------------------------

#pragma once

#include <memory>
#include <chrono>

#include "public.sdk/source/vst/vstaudioeffect.h"

#include "../params.h"
#include "../Hardware/HardwareSynthesizer.h"

namespace Newkon
{

	//------------------------------------------------------------------------
	//  HardwareSynthProcessor
	//------------------------------------------------------------------------
	class HardwareSynthProcessor : public Steinberg::Vst::AudioEffect
	{
	public:
		HardwareSynthProcessor();
		~HardwareSynthProcessor() SMTG_OVERRIDE;

		// Create function
		static Steinberg::FUnknown *createInstance(void * /*context*/)
		{
			return (Steinberg::Vst::IAudioProcessor *)new HardwareSynthProcessor;
		}

		//--- ---------------------------------------------------------------------
		// AudioEffect overrides:
		//--- ---------------------------------------------------------------------
		/** Called at first after constructor */
		Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown *context) SMTG_OVERRIDE;

		/** Called at the end before destructor */
		Steinberg::tresult PLUGIN_API terminate() SMTG_OVERRIDE;

		/** Switch the Plug-in on/off */
		Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) SMTG_OVERRIDE;

		/** Will be called before any process call */
		Steinberg::tresult PLUGIN_API setupProcessing(Steinberg::Vst::ProcessSetup &newSetup) SMTG_OVERRIDE;

		/** Asks if a given sample size is supported see SymbolicSampleSizes. */
		Steinberg::tresult PLUGIN_API canProcessSampleSize(Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;

		/** Here we go...the process call */
		Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData &data) SMTG_OVERRIDE;

		/** For persistence */
		Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream *state) SMTG_OVERRIDE;
		Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream *state) SMTG_OVERRIDE;

		/** Returns latency in samples */
		Steinberg::uint32 PLUGIN_API getLatencySamples() SMTG_OVERRIDE;

		/** Change latency dynamically */
		void setLatency(double latencySeconds);

		/** Connect to a hardware synthesizer by device index */
		bool connectToSynthesizer(size_t deviceIndex);

		/** Disconnect from current synthesizer */
		void disconnectSynthesizer();

		/** Check if a synthesizer is connected */
		bool isSynthesizerConnected() const;

		/** Get connected synthesizer device name */
		std::string getConnectedSynthesizerName() const;

		/** Get the current processor instance (for UI access) */
		static HardwareSynthProcessor *getCurrentInstance();

		//------------------------------------------------------------------------
	protected:
		double sampleRate = 44100.0;
		Steinberg::int32 bufferSize = 512;
		double currentLatencySeconds = 0.0;

		// Latency debounce state
		bool latencyChangePending = false;
		double pendingLatencySeconds = 0.0;
		std::chrono::steady_clock::time_point lastLatencyChangeRequest;

		// Hardware synthesizer management
		std::unique_ptr<HardwareSynthesizer> connectedSynthesizer;

		// Static reference for UI access
		static HardwareSynthProcessor *currentInstance;
	};

	//------------------------------------------------------------------------
} // namespace Newkon
