// =============================================================================
//  PluginProcessor.h — the DECLARATION of the audio engine
//
//  C++ splits a class in two: the .h says WHAT exists (signatures), the .cpp
//  says HOW it works (bodies). The compiler reads one .cpp at a time in
//  isolation, so anything wanting to call us must #include this header.
//
//  Comments here are short on purpose — the reasoning lives in documentation/.
// =============================================================================

// Include-once guard. Without it, including this twice declares the class twice
// and fails to compile. #include is a literal copy-paste, not C#'s `using`.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>


//==============================================================================
/**
    The audio engine.

    `: public juce::AudioProcessor` is inheritance — C++ also has private and
    protected inheritance, so `public` is explicit. AudioProcessor is abstract
    (methods marked `= 0`, C++'s `abstract`), so every one must be implemented.
*/
class SimpleGainAudioProcessor : public juce::AudioProcessor
{
public:
    //==========================================================================
    // Construction

    SimpleGainAudioProcessor();

    // `~` is the destructor — runs automatically and deterministically when the
    // object dies. This is RAII, and it's why C++ needs no garbage collector.
    // Closest C# concept: IDisposable + `using`, but automatic.
    ~SimpleGainAudioProcessor() override;

    //==========================================================================
    // The audio lifecycle

    /** Host's "here are the conditions" call, before playback and on any
        sample-rate or block-size change. The safe place to allocate. */
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;

    /** Playback stopped — free anything expensive. */
    void releaseResources() override;

    /** THE REAL-TIME CALLBACK. Everything else exists to serve this.

        Runs ~94 times a second with a hard deadline of a few milliseconds.
        No allocation, no locks, no file I/O, no logging — anything that could
        block risks an audible click.

        `&` means a REFERENCE — an alias for the host's buffer, not a copy, which
        is how we modify the audio in place.
        -> documentation/plugin-hosting-and-threads.md sections 3 and 8
    */
    void processBlock (juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer& midiMessages) override;

    /** Which channel layouts we accept — mono->mono and stereo->stereo.

        `const` twice, meaning two different things: the argument won't be
        modified, and (after the parens) this object won't be either. */
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    //==========================================================================
    // The GUI

    /** Factory method — the host owns and deletes what this returns. */
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==========================================================================
    // Save / restore — implemented in Phase 5

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Parameters

    /** Builds the parameter list.

        `static` because it's called in the member initializer list to construct
        `apvts`, when `this` is still half-built. Returning by value is cheap —
        the compiler moves or elides rather than copying. */
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /** One object doing three jobs: exposing parameters to the host for
        automation, serialising to XML for save/load, and binding to GUI
        controls. Public because the editor needs it to attach a slider.
        -> documentation/parameters-and-automation.md section 1 */
    juce::AudioProcessorValueTreeState apvts;

    //==========================================================================
    // Boilerplate the host asks about

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;

    /** How long sound rings on after input stops — matters for reverbs and
        delays. A gain has no tail, so 0. */
    double getTailLengthSeconds() const override;

    /** Legacy VST2 preset slots. Modern plugins use the host's own preset
        system, so we report the minimum legal answer of one. */
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

private:
    //==========================================================================
    /** The gain parameter's current value.

        Cached because the lookup is by string and processBlock can't afford it.
        Atomic because the message thread writes it while the audio thread reads
        it — a plain float here is a data race, and in C++ that's undefined
        behaviour, not just a risk.

        Raw pointer rather than reference because it's assigned in the
        constructor body; references can't be rebound. The APVTS owns it, not us.
        -> documentation/parameters-and-automation.md section 4
    */
    std::atomic<float>* gainDbParameter = nullptr;

    // Macro: deletes the copy constructor (every C++ class is copyable by
    // default — for an audio processor that would be a disaster) and adds a
    // leak detector. Supplies its own semicolon.
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleGainAudioProcessor)
};
