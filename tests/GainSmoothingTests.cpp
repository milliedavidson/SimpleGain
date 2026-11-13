// =============================================================================
//  GainSmoothingTests.cpp — Phase 4: the gain glides instead of stepping
//
//  Why each test exists: documentation/smoothing-and-the-audio-loop.md
// =============================================================================
#include "TestSupport.h"

#include <initializer_list>

namespace
{
    using namespace support;

    constexpr float tolerance = 1.0e-5f;

    struct Settings
    {
        double sampleRate;
        int blockSize;
    };

    // Rates and block sizes real hosts use, including the extremes that exposed bugs in the scratch harness.
    constexpr std::initializer_list<Settings> allSettings { { 44100.0, 256 }, { 48000.0, 64 },
                                                        { 48000.0, 512 }, { 96000.0, 512 } };

    struct GainSmoothingTests final : juce::UnitTest
    {
        GainSmoothingTests() : juce::UnitTest ("Gain smoothing", "SimpleGain") {}

        void runTest() override
        {
            beginTest ("A fresh plugin passes audio at unity from the very first sample");
            {
                Rig rig (48000.0, 512);
                rig.prepare();
                rig.processOneBlock();

                expectWithinAbsoluteError (rig.left.front(), 1.0f, tolerance);
                expectWithinAbsoluteError (biggestStep (rig.left), 0.0f, tolerance);
            }

            beginTest ("It starts at the fader's position rather than fading in from silence");
            {
                // The smoother's own default is 0, so forgetting to initialise it would fade in over 20 ms.
                Rig rig (48000.0, 512);
                setGainDb (rig.processor, -6.0f);
                rig.prepare();
                rig.processOneBlock();

                expectWithinAbsoluteError (rig.left.front(), dbToGain (-6.0f), tolerance);
                expectWithinAbsoluteError (biggestStep (rig.left), 0.0f, tolerance);
            }

            beginTest ("A big fader jump glides: no two adjacent samples differ by more than the ideal step");
            for (const auto& [sampleRate, blockSize] : allSettings)
            {
                Rig rig (sampleRate, blockSize);
                rig.prepare();
                rig.processBlocks (4);

                const auto before = rig.left.size();
                setGainDb (rig.processor, 24.0f);
                rig.processMilliseconds (60.0);

                // The change is 1.0 -> +24 dB, spread over 20 ms of samples. Phase 3 changed it by 14.85 in one.
                const auto target = dbToGain (24.0f);
                const auto rampSamples = static_cast<float> (std::floor (0.02 * sampleRate));
                const auto idealStep = (target - 1.0f) / rampSamples;

                expectLessThan (biggestStep (rig.left, before), idealStep * 1.1f,
                                juce::String (sampleRate) + " Hz / " + juce::String (blockSize));
            }

            beginTest ("The glide takes about 20 ms whatever the sample rate or block size");
            for (const auto& [sampleRate, blockSize] : allSettings)
            {
                Rig rig (sampleRate, blockSize);
                rig.prepare();
                rig.processBlocks (4);

                const auto changeAt = rig.left.size();
                setGainDb (rig.processor, 24.0f);
                rig.processMilliseconds (60.0);

                const auto ms = msToSettle (rig.left, changeAt, 1.0f, dbToGain (24.0f), sampleRate);
                const auto label = juce::String (sampleRate) + " Hz / " + juce::String (blockSize);

                expectGreaterThan (ms, 19.0, label);
                expectLessThan (ms, 21.0, label);
            }

            beginTest ("Both channels get exactly the same gain on every sample");
            {
                // The classic bug: swap the loops and channel one ends up on a different part of the ramp.
                Rig rig (48000.0, 512);
                rig.prepare();
                rig.processBlocks (2);
                setGainDb (rig.processor, 24.0f);
                rig.processMilliseconds (60.0);

                int mismatches = 0;
                for (size_t i = 0; i < rig.left.size(); ++i)
                    if (rig.left[i] < rig.right[i] || rig.left[i] > rig.right[i])
                        ++mismatches;

                expectEquals (mismatches, 0);
            }

            beginTest ("Once the glide is over the gain is steady and exact");
            {
                Rig rig (48000.0, 512);
                rig.prepare();
                setGainDb (rig.processor, -12.0f);
                rig.processMilliseconds (100.0);

                expectWithinAbsoluteError (rig.left.back(), dbToGain (-12.0f), tolerance);
                expectWithinAbsoluteError (biggestStep (rig.left, rig.left.size() - 512), 0.0f, tolerance);
            }

            beginTest ("Dragging the fader every block never produces a big step");
            for (const auto& [sampleRate, blockSize] : allSettings)
            {
                Rig rig (sampleRate, blockSize);
                rig.prepare();
                rig.processBlocks (3);

                const auto before = rig.left.size();
                for (int db = 1; db <= 24; ++db)                 // +1 dB per block, like a fast drag
                {
                    setGainDb (rig.processor, static_cast<float> (db));
                    rig.processOneBlock();
                }
                rig.processMilliseconds (60.0);

                // Phase 3's largest step here was 1.72.
                expectLessThan (biggestStep (rig.left, before), 0.05f,
                                juce::String (sampleRate) + " Hz / " + juce::String (blockSize));
            }
        }
    };

    // Never read: constructing it registers it with JUCE's test runner.
    [[maybe_unused]] GainSmoothingTests gainSmoothingTests;
}
