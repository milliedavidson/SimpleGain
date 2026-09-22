// =============================================================================
//  PluginEditor.cpp  —  the IMPLEMENTATION of our GUI
// =============================================================================

#include "PluginEditor.h"


//==============================================================================
SimpleGainAudioProcessorEditor::SimpleGainAudioProcessorEditor (SimpleGainAudioProcessor& p)
    : AudioProcessorEditor (&p),   // base class wants a POINTER, so we pass &p
      processorRef (p)             // our member is a REFERENCE, so we bind to p
{
    // `&p` takes the ADDRESS of p, producing a pointer. This is the inverse of
    // the `*this` we used in createEditor(). Pointers and references are two
    // views of the same underlying thing, and `&` / `*` convert between them.
    //
    // Note also: processorRef MUST be initialised here in the initializer list.
    // A reference member cannot be assigned later — there's no such thing as
    // an unbound reference. The compiler will refuse to build otherwise.

    // Every JUCE component starts with zero size. Nothing draws until you
    // give it one.
    setSize (400, 300);
}

SimpleGainAudioProcessorEditor::~SimpleGainAudioProcessorEditor()
{
}


//==============================================================================
void SimpleGainAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Fill the background. findColour() pulls from the current LookAndFeel —
    // JUCE's theming system, comparable to a WPF ResourceDictionary.
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (24.0f));
    g.drawFittedText ("SimpleGain",
                      getLocalBounds(),
                      juce::Justification::centred,
                      1);

    // getLocalBounds() returns this component's own rectangle, with its
    // top-left at (0,0) — coordinates are always relative to the parent.
}

void SimpleGainAudioProcessorEditor::resized()
{
    // No child components yet, so nothing to lay out. We'll fill this in when
    // we add the gain knob.
}
