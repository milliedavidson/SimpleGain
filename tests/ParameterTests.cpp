// =============================================================================
//  ParameterTests.cpp — the parameter's identity, range and display, and the
//  channel layouts we accept
//
//  Several of these guard things that are PERMANENT once released (see CLAUDE.md):
//  the parameter ID, its version hint and its range. A failing test here means a
//  change that would break saved projects or recorded automation.
// =============================================================================
#include "TestSupport.h"

namespace
{
    using namespace support;

    struct ParameterTests final : juce::UnitTest
    {
        ParameterTests() : UnitTest ("Parameters", "SimpleGain") {}

        void runTest() override
        {
            SimpleGainAudioProcessor processor;
            const auto* gain = processor.apvts.getParameter ("gain");

            beginTest ("The gain parameter exists, with the permanent ID and version hint");
            expect (gain != nullptr, "no parameter with ID \"gain\"");
            if (gain == nullptr)
                return;
            expectEquals (gain->getParameterID(), juce::String ("gain"));
            expectEquals (gain->getVersionHint(), 1);

            beginTest ("The range is -60 dB to +30 dB, defaulting to 0 dB");
            expectWithinAbsoluteError (gain->getNormalisableRange().start, -60.0f, 0.001f);
            expectWithinAbsoluteError (gain->getNormalisableRange().end, 30.0f, 0.001f);
            expectWithinAbsoluteError (gain->convertFrom0to1 (gain->getDefaultValue()), 0.0f, 0.001f);

            beginTest ("There is no skew: the middle of the fader is the middle of the range");
            expectWithinAbsoluteError (gain->convertFrom0to1 (0.5f), -15.0f, 0.001f);

            beginTest ("The host is shown readable text, not a bare number");
            expectEquals (gain->getText (gain->convertTo0to1 (-6.0f), 100), juce::String ("-6.0 dB"));
            expectEquals (gain->getText (gain->convertTo0to1 (0.0f), 100), juce::String ("0.0 dB"));
            expectEquals (gain->getText (gain->convertTo0to1 (30.0f), 100), juce::String ("30.0 dB"));
            expectEquals (gain->getLabel(), juce::String ("dB"));

            beginTest ("The fader moves in 0.1 dB steps");
            setGainDb (processor, -6.04f);
            expectWithinAbsoluteError (gainDb (processor), -6.0f, 0.001f);

            beginTest ("Only matching mono or stereo layouts are accepted");
            {
                auto layout = [] (const juce::AudioChannelSet& in, const juce::AudioChannelSet& out)
                {
                    juce::AudioProcessor::BusesLayout result;
                    result.inputBuses.add (in);
                    result.outputBuses.add (out);
                    return result;
                };

                const auto mono   = juce::AudioChannelSet::mono();
                const auto stereo = juce::AudioChannelSet::stereo();
                const auto surround = juce::AudioChannelSet::create5point1();

                expect (processor.isBusesLayoutSupported (layout (mono, mono)),     "mono -> mono");
                expect (processor.isBusesLayoutSupported (layout (stereo, stereo)), "stereo -> stereo");
                expect (! processor.isBusesLayoutSupported (layout (mono, stereo)), "mono -> stereo must be refused");
                expect (! processor.isBusesLayoutSupported (layout (stereo, mono)), "stereo -> mono must be refused");
                expect (! processor.isBusesLayoutSupported (layout (surround, surround)), "5.1 must be refused");
            }
        }
    };

    // Never read: constructing it registers it with JUCE's test runner.
    [[maybe_unused]] ParameterTests parameterTests;
}
