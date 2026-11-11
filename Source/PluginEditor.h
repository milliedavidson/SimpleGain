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
    A LookAndFeel is JUCE's "theme": one object that decides how widgets draw
    themselves. Ours changes one thing — the value text in the knob's box scales
    with the box, instead of staying at a fixed size.
    -> documentation/editor-gui.md section 9
*/
class SimpleGainLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    juce::Font getLabelFont (juce::Label&) override;
};


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
    // to top: the look-and-feel must outlive the widgets that use it (so it is
    // first), and the attachment's destructor touches the slider (so it is last).
    // -> documentation/editor-gui.md section 5
    SimpleGainLookAndFeel lookAndFeel;
    juce::Label titleLabel;
    juce::Slider gainSlider;
    juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleGainAudioProcessorEditor)
};
