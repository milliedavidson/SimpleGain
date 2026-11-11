// =============================================================================
//  TestSupport.h — helpers shared by the tests
//
//  The core trick: feed a constant 1.0 through the REAL processor, so every
//  output sample *is* the gain that was applied to it. That turns "does it
//  click?" into plain arithmetic on a list of numbers.
// =============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

namespace support
{
    /** A processor wired up the way a host would, plus the buffer to push audio through it. */
    class Rig
    {
    public:
        Rig (double sampleRateToUse, int blockSizeToUse)
            : sampleRate (sampleRateToUse), blockSize (blockSizeToUse), buffer (2, blockSizeToUse)
        {
            processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
        }

        /** What a host does before playback starts. */
        void prepare()  { processor.prepareToPlay (sampleRate, blockSize); }

        /** One block of constant 1.0 in; both channels' output is appended to `left` / `right`. */
        void processOneBlock()
        {
            for (int channel = 0; channel < 2; ++channel)
                juce::FloatVectorOperations::fill (buffer.getWritePointer (channel), 1.0f, blockSize);

            processor.processBlock (buffer, midi);

            left .insert (left .end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + blockSize);
            right.insert (right.end(), buffer.getReadPointer (1), buffer.getReadPointer (1) + blockSize);
        }

        void processBlocks (int count)
        {
            for (int i = 0; i < count; ++i)
                processOneBlock();
        }

        /** Enough whole blocks to cover at least this much audio. */
        void processMilliseconds (double milliseconds)
        {
            processBlocks (static_cast<int> (std::ceil (milliseconds * 0.001 * sampleRate / blockSize)));
        }

        SimpleGainAudioProcessor processor;
        double sampleRate;
        int blockSize;
        juce::AudioBuffer<float> buffer;
        juce::MidiBuffer midi;
        std::vector<float> left, right;
    };

    /** Sets the gain parameter to a real dB value, the way a host would. */
    inline void setGainDb (SimpleGainAudioProcessor& processor, float db)
    {
        auto* parameter = processor.apvts.getParameter ("gain");
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (db));
    }

    /** The gain parameter's current value, in dB. */
    inline float gainDb (SimpleGainAudioProcessor& processor)
    {
        return processor.apvts.getRawParameterValue ("gain")->load();
    }

    inline float dbToGain (float db)   { return juce::Decibels::decibelsToGain (db); }

    /** Largest change between two adjacent samples, from index `from` onwards. */
    inline float biggestStep (const std::vector<float>& samples, size_t from = 1)
    {
        float biggest = 0.0f;

        for (size_t i = std::max<size_t> (from, 1); i < samples.size(); ++i)
            biggest = std::max (biggest, std::abs (samples[i] - samples[i - 1]));

        return biggest;
    }

    /** Milliseconds from index `from` until the output is within 1% of the way to `target`
        (or -1 if it never gets there). */
    inline double msToSettle (const std::vector<float>& samples, size_t from, float start, float target,
                              double sampleRate)
    {
        for (size_t i = from; i < samples.size(); ++i)
            if (std::abs (samples[i] - target) <= 0.01f * std::abs (target - start))
                return 1000.0 * static_cast<double> (i - from) / sampleRate;

        return -1.0;
    }

    /** On corrupt data JUCE's own code can fire debug assertions (Debug builds only) -- bad UTF-8 in
        String::fromUTF8, bad names in the XML parser, XML text nodes in ValueTree::fromXml. They are JUCE's
        diagnostics about the data, not crashes, and a corrupt project file produces exactly them.

        Tests that feed corrupt data on purpose use this to swallow assertions raised inside JUCE's own files
        (juce_*.cpp), count them, and let everything else through -- including any assertion in OUR code. */
    class ScopedCorruptDataAssertionFilter final : public juce::Logger
    {
    public:
        ScopedCorruptDataAssertionFilter()
        {
            previous = juce::Logger::getCurrentLogger();
            juce::Logger::setCurrentLogger (this);
        }

        ~ScopedCorruptDataAssertionFilter() override  { juce::Logger::setCurrentLogger (previous); }

        int swallowed = 0;

    private:
        void logMessage (const juce::String& message) override
        {
            if (message.startsWith ("JUCE Assertion failure in juce_"))
                ++swallowed;
            else if (previous != nullptr)
            {
                // Pass it on to main.cpp's counter, which fails the run. logMessage is protected,
                // so hand the message to the logger we replaced and then step back in.
                juce::Logger::setCurrentLogger (previous);
                juce::Logger::writeToLog (message);
                juce::Logger::setCurrentLogger (this);
            }
            else
                std::cout << message << std::endl;
        }

        juce::Logger* previous = nullptr;
    };

    /** The state a host would store for this processor. */
    inline juce::MemoryBlock saveState (SimpleGainAudioProcessor& processor)
    {
        juce::MemoryBlock block;
        processor.getStateInformation (block);
        return block;
    }

    /** The state a fresh processor would save with the fader at `db`. */
    inline juce::MemoryBlock savedStateAt (float db)
    {
        SimpleGainAudioProcessor processor;
        setGainDb (processor, db);
        return saveState (processor);
    }

    /** What a host does when it reopens a project. */
    inline void loadState (SimpleGainAudioProcessor& processor, const juce::MemoryBlock& block)
    {
        processor.setStateInformation (block.getData(), static_cast<int> (block.getSize()));
    }
}
