// =============================================================================
//  CorruptStateTests.cpp — whatever junk reaches setStateInformation, the plugin
//  must not crash
//
//  A crash while loading means the user's project won't open, and the plugin
//  takes the blame. These feed the loader empty, random, truncated and corrupted
//  data. Kept in their own group so they can be run on their own, for example
//  under a memory checker:
//      SimpleGain_Tests "Corrupt state data"
//  Why: documentation/state-save-and-restore.md section 6
// =============================================================================
#include "TestSupport.h"

namespace
{
    using namespace support;

    struct CorruptStateTests final : juce::UnitTest
    {
        CorruptStateTests() : UnitTest ("Corrupt state data", "SimpleGain") {}

        /** Whatever junk arrives, the plugin must not crash, and the gain must stay a sane number in range. */
        void survives (const void* data, const int size, const juce::String& label)
        {
            SimpleGainAudioProcessor processor;
            setGainDb (processor, -6.0f);
            processor.setStateInformation (data, size);

            const auto gain = gainDb (processor);
            expect (std::isfinite (gain) && gain >= -60.0f && gain <= 30.0f, "gain out of range after: " + label);
        }

        void runTest() override
        {
            beginTest ("Empty, garbage and truncated state never crashes the host");

            // Must exist for the whole test: it swallows JUCE's expected assertions only while it is alive.
            ScopedCorruptDataAssertionFilter filter;

            const auto valid = savedStateAt (-6.0f);
            const auto validBytes = static_cast<const char*> (valid.getData());
            const auto size = static_cast<int> (valid.getSize());

            // Everything below corrupts or truncates this saved state. If saving is broken and it comes back
            // empty, there is nothing to corrupt and this test would quietly check almost nothing: so fail loudly.
            expectGreaterThan (size, 8, "the saved state is empty, so there is nothing to corrupt");
            if (size <= 8)
                return;

            survives (nullptr, 0, "null pointer, size 0");
            survives (valid.getData(), 0, "valid pointer, size 0");
            survives (valid.getData(), -1, "negative size");
            survives (valid.getData(), 1, "1 byte");

            // Fixed seeds, so every run (and CI) corrupts the same bytes in the same ways. Failures are reproducible.
            for (int seed = 1; seed <= 8; ++seed)
            {
                juce::Random random (seed);

                for (const int length : { 7, 8, 9, 64, 4000 })
                {
                    juce::MemoryBlock noise (static_cast<size_t> (length));
                    for (int i = 0; i < length; ++i)
                        static_cast<char*> (noise.getData())[i] = static_cast<char> (random.nextInt (256));

                    survives (noise.getData(), length, "random bytes, length " + juce::String (length));
                }

                for (int index = 0; index < size; ++index)                   // every single-byte corruption
                {
                    juce::MemoryBlock corrupted (valid);
                    corrupted[index] = static_cast<char> (random.nextInt (256));
                    survives (corrupted.getData(), size, "seed " + juce::String (seed) + ", byte "
                                                         + juce::String (index) + " corrupted");
                }
            }

            // Every possible truncation. Each copy lives in a block EXACTLY that long, so code that reads past
            // the end touches unowned memory (and a memory checker can see it) rather than harmlessly reading
            // the rest of the original. The last byte is also set to each kind of multibyte UTF-8 lead byte:
            // the case a sloppy decoder reads past the end on.
            for (int length = 1; length < size; ++length)
            {
                juce::MemoryBlock exact (validBytes, static_cast<size_t> (length));
                survives (exact.getData(), length, "truncated to " + juce::String (length) + " bytes");

                for (const int lead : { 0xC3, 0xE2, 0xF0 })
                {
                    static_cast<char*> (exact.getData())[length - 1] = static_cast<char> (lead);
                    survives (exact.getData(), length, "truncated to " + juce::String (length)
                                                       + " bytes, ending in lead byte " + juce::String (lead));
                }
            }

            if (filter.swallowed > 0)
                logMessage ("JUCE's own debug assertions fired " + juce::String (filter.swallowed)
                            + " times on the corrupt inputs. Debug builds only; not a crash.");
        }
    };

    // Never read: constructing it registers it with JUCE's test runner.
    [[maybe_unused]] CorruptStateTests corruptStateTests;
}
