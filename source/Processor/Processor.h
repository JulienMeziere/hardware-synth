//------------------------------------------------------------------------
// Copyright(c) 2023 Newkon.
//------------------------------------------------------------------------

#pragma once

#include <memory>

#include "public.sdk/source/vst/vstaudioeffect.h"

#include "../params.h"

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

		//------------------------------------------------------------------------
	protected:
		float knob1Val = DEFAULT_KNOB_VALUE;
		float knob2Val = DEFAULT_KNOB_VALUE;
		float knob3Val = DEFAULT_KNOB_VALUE;
		float knob4Val = DEFAULT_KNOB_VALUE;
		float knob5Val = DEFAULT_KNOB_VALUE;
		float knob6Val = DEFAULT_KNOB_VALUE;
		float knob7Val = DEFAULT_KNOB_VALUE;
		float knob8Val = DEFAULT_KNOB_VALUE;
	};

	//------------------------------------------------------------------------
} // namespace Newkon
