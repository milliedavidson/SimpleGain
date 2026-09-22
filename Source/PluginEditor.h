// =============================================================================
//  PluginEditor.h  —  the DECLARATION of our GUI
// =============================================================================
//  Key idea: the editor is SEPARATE from the processor and OPTIONAL.
//  A DAW can load, run and render your plugin with the window closed — or
//  never opened at all. So the GUI must never own anything the audio needs.
//
//  The editor lives on the MESSAGE THREAD (the UI thread). The processor's
//  processBlock lives on the AUDIO THREAD. Keeping those apart is the central
//  design constraint of the whole plugin.
// =============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"


//==============================================================================
/**
    Our plugin window.

    It inherits from juce::AudioProcessorEditor, which itself inherits from
    juce::Component — JUCE's base class for anything drawable. Component is
    roughly the equivalent of a WPF UIElement or a WinForms Control.
*/
class SimpleGainAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    /** The constructor takes a REFERENCE to the processor that created us.

        A reference (`&`), not a pointer (`*`), and not a copy. This says three
        things at once:
          - we can reach the processor to read/write its state
          - we do NOT own it (it outlives us — it created us)
          - it can never be null (references always refer to something)

        That last point is a real advantage over C#, where any reference
        parameter might be null unless you've enabled nullable reference types.
    */
    explicit SimpleGainAudioProcessorEditor (SimpleGainAudioProcessor&);

    // `explicit` blocks accidental implicit conversion. Without it, C++ would
    // happily convert a SimpleGainAudioProcessor into an editor anywhere one
    // was expected. C# never does implicit conversions like this, so the
    // keyword has no C# counterpart — but in C++ it's good hygiene on any
    // single-argument constructor.

    ~SimpleGainAudioProcessorEditor() override;

    //==========================================================================
    /** Draw the component. JUCE calls this whenever the window needs
        repainting — same idea as WPF's OnRender or WinForms' OnPaint.
        You never call it yourself; you call repaint() to request it. */
    void paint (juce::Graphics&) override;

    /** Called whenever this component changes size. Position your child
        components here. This is JUCE's layout pass. */
    void resized() override;

private:
    //==========================================================================
    /** A reference back to the processor that owns us.

        `&` again: a member reference. It must be bound in the constructor's
        initializer list and can never be reseated afterwards. That's stricter
        than a C# field holding an object, and deliberately so — it encodes
        "this editor belongs to exactly this processor, for life".
    */
    SimpleGainAudioProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleGainAudioProcessorEditor)
};
