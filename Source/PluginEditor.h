// =============================================================================
//  PluginEditor.h — the plugin window
//
//  The editor is separate from the processor, and optional: a host can run the
//  plugin with the window closed, and opens and closes it many times. It lives
//  on the MESSAGE thread; processBlock lives on the audio thread.
//
//  Comments are short on purpose — the reasoning lives in documentation/.
// =============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"


//==============================================================================
/**
    The window: a title, a gain knob and its value box.

    juce::AudioProcessorEditor is a juce::Component, JUCE's base class for
    anything drawable — roughly WPF's UIElement or WinForms' Control.
    -> documentation/editor-gui.md sections 1 and 2
*/
class SimpleGainAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    /** Takes the processor by REFERENCE: it can't be null, and we don't own it
        (it outlives every editor it creates). `explicit` stops C++ quietly
        converting a processor into an editor. */
    explicit SimpleGainAudioProcessorEditor (SimpleGainAudioProcessor&);

    ~SimpleGainAudioProcessorEditor() override;

    /** JUCE calls this when the window needs drawing. We never call it ourselves. */
    void paint (juce::Graphics&) override;

    /** JUCE calls this whenever the size changes: position the children here. */
    void resized() override;

private:
    //==========================================================================
    // ORDER MATTERS. Members are constructed top to bottom and destroyed bottom
    // to top, and the attachment's destructor touches the slider — so the
    // attachment must be declared AFTER the slider, to be destroyed before it.
    // -> documentation/editor-gui.md section 5
    juce::Label titleLabel;
    juce::Slider gainSlider;
    juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleGainAudioProcessorEditor)
};
