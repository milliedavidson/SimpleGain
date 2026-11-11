// =============================================================================
//  StateTests.cpp — Phase 5: the gain survives closing and reopening a project
//
//  A host stores whatever getStateInformation() returns inside the user's project
//  file, and hands it back to setStateInformation() later — possibly years later,
//  possibly from a newer or older version of the plugin. (Corrupt data is tested
//  separately, in CorruptStateTests.cpp.)
//  Why each test exists: documentation/state-save-and-restore.md
// =============================================================================
#include "TestSupport.h"

#include <initializer_list>
#include <memory>

namespace
{
    using namespace support;

    std::unique_ptr<juce::XmlElement> toXml (const juce::MemoryBlock& block)
    {
        return juce::AudioProcessor::getXmlFromBinary (block.getData(), static_cast<int> (block.getSize()));
    }

    juce::MemoryBlock toBlock (const juce::XmlElement& xml)
    {
        juce::MemoryBlock block;
        juce::AudioProcessor::copyXmlToBinary (xml, block);
        return block;
    }

    /** A hand-built saved state, so tests can describe documents we never wrote ourselves. */
    juce::MemoryBlock stateWithGain (const juce::String& value, const juce::String& rootTag = "PARAMETERS",
                                     const juce::String& parameterId = "gain")
    {
        juce::XmlElement root (rootTag);
        auto* parameter = root.createNewChildElement ("PARAM");
        parameter->setAttribute ("id", parameterId);
        parameter->setAttribute ("value", value);
        return toBlock (root);
    }

    constexpr float tolerance = 1.0e-5f;

    struct StateTests final : public juce::UnitTest
    {
        StateTests() : juce::UnitTest ("State save and restore", "SimpleGain") {}

        void runTest() override
        {
            roundTrip();
            appliedFromTheFirstSample();
            loadedDuringPlayback();
            wrongOrUnexpectedContent();
            savedFormat();
        }

        //======================================================================
        void roundTrip()
        {
            beginTest ("A saved gain comes back after reopening the project");

            for (const float db : { -60.0f, -12.3f, 0.0f, 7.7f, 30.0f })
            {
                const auto saved = savedStateAt (db);

                SimpleGainAudioProcessor reopened;          // a new instance, as when a project is opened
                expectWithinAbsoluteError (gainDb (reopened), 0.0f, 0.001f);

                loadState (reopened, saved);
                expectWithinAbsoluteError (gainDb (reopened), db, 0.01f, "gain " + juce::String (db) + " dB");
            }
        }

        void appliedFromTheFirstSample()
        {
            beginTest ("A restored gain is applied from the very first sample, with no fade-in");

            // Hosts normally restore state BEFORE prepareToPlay, so prepareToPlay must pick it up.
            Rig rig (48000.0, 512);
            loadState (rig.processor, savedStateAt (-12.3f));
            rig.prepare();
            rig.processOneBlock();

            expectWithinAbsoluteError (rig.left.front(), dbToGain (-12.3f), tolerance);
            expectWithinAbsoluteError (biggestStep (rig.left), 0.0f, tolerance);
        }

        void loadedDuringPlayback()
        {
            beginTest ("State loaded while audio is playing glides rather than jumping");

            Rig rig (48000.0, 512);
            rig.prepare();
            rig.processBlocks (3);

            const auto before = rig.left.size();
            loadState (rig.processor, savedStateAt (24.0f));
            rig.processMilliseconds (60.0);

            const auto target = dbToGain (24.0f);
            const auto idealStep = (target - 1.0f) / static_cast<float> (std::floor (0.02 * 48000.0));

            expectLessThan (biggestStep (rig.left, before), idealStep * 1.1f);
            expectWithinAbsoluteError (rig.left.back(), target, tolerance);
        }

        //======================================================================
        void wrongOrUnexpectedContent()
        {
            beginTest ("A document that isn't ours is ignored");
            {
                SimpleGainAudioProcessor processor;
                setGainDb (processor, -6.0f);
                loadState (processor, stateWithGain ("20.0", "SOMETHING_ELSE"));

                expectWithinAbsoluteError (gainDb (processor), -6.0f, 0.01f);
            }

            beginTest ("The document tag must match exactly, including case");
            {
                // One flipped bit turns 'P' into 'p'. JUCE's hasTagName() would accept that; we don't.
                SimpleGainAudioProcessor processor;
                setGainDb (processor, -6.0f);
                loadState (processor, stateWithGain ("20.0", "parameters"));

                expectWithinAbsoluteError (gainDb (processor), -6.0f, 0.01f);
            }

            beginTest ("A parameter we don't have in the saved state is ignored");
            {
                SimpleGainAudioProcessor processor;
                loadState (processor, savedStateAt (-6.0f));
                loadState (processor, stateWithGain ("5.0", "PARAMETERS", "mystery"));

                // "mystery" does nothing -- and since the state no longer mentions "gain", gain goes back to its default.
                expectWithinAbsoluteError (gainDb (processor), 0.0f, 0.01f);
            }

            beginTest ("A parameter missing from the saved state returns to its default");
            {
                // Think of an old project saved before a parameter existed: the new parameter must get its default,
                // not whatever the previous state left behind.
                SimpleGainAudioProcessor processor;
                setGainDb (processor, -6.0f);
                loadState (processor, toBlock (juce::XmlElement ("PARAMETERS")));

                expectWithinAbsoluteError (gainDb (processor), 0.0f, 0.01f);
            }

            beginTest ("Out-of-range saved values are clamped to the parameter's range");
            {
                SimpleGainAudioProcessor processor;

                loadState (processor, stateWithGain ("999"));
                expectWithinAbsoluteError (gainDb (processor), 30.0f, 0.01f);

                loadState (processor, stateWithGain ("-999"));
                expectWithinAbsoluteError (gainDb (processor), -60.0f, 0.01f);
            }
        }

        //======================================================================
        void savedFormat()
        {
            beginTest ("The saved format is stable: a PARAMETERS document with one PARAM whose id is \"gain\"");
            {
                const auto saved = savedStateAt (-12.3f);
                const auto xml = toXml (saved);
                expect (xml != nullptr, "state isn't a readable XML document");

                if (xml != nullptr)
                {
                    logMessage ("Saved state at -12.3 dB is " + juce::String (static_cast<int> (saved.getSize()))
                                + " bytes: " + xml->toString (juce::XmlElement::TextFormat().singleLine()));

                    expect (xml->hasTagName ("PARAMETERS"));
                    expectEquals (xml->getNumChildElements(), 1);

                    if (const auto* parameter = xml->getFirstChildElement())
                    {
                        expect (parameter->hasTagName ("PARAM"));
                        expectEquals (parameter->getStringAttribute ("id"), juce::String ("gain"));
                        expectWithinAbsoluteError (static_cast<float> (parameter->getDoubleAttribute ("value")),
                                                   -12.3f, 0.01f);
                    }
                }
            }

            beginTest ("State saved by today's version still loads in every future version");
            {
                // A literal copy of today's format. If this test ever has to change, saved projects have broken.
                const auto golden = juce::parseXML ("<PARAMETERS><PARAM id=\"gain\" value=\"-6.0\"/></PARAMETERS>");
                expect (golden != nullptr);

                SimpleGainAudioProcessor processor;
                loadState (processor, toBlock (*golden));
                expectWithinAbsoluteError (gainDb (processor), -6.0f, 0.01f);
            }
        }
    };

    StateTests stateTests;
}
