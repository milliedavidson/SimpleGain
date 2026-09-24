// =============================================================================
//  PluginProcessor.h  —  the DECLARATION of our audio engine
// =============================================================================
//  In C#, a class lives in exactly one .cs file: declaration and implementation
//  together. C++ splits them in two:
//
//     .h  (header)  — WHAT exists: the class, its members, their signatures.
//     .cpp (source) — HOW it works: the actual bodies of those functions.
//
//  Why? Because a C++ compiler reads ONE .cpp file at a time, in isolation, and
//  knows nothing about any other file. If PluginEditor.cpp wants to call a
//  method on our processor, it needs to be told that method exists. It gets
//  told by #including this header.
//
//  Rule of thumb: the .h is the public contract; the .cpp is the secret.
// =============================================================================

// "Only paste the contents of this file in once, no matter how many times it's
//  #included." Without it, including this header twice would declare the class
//  twice and the compiler would error. C# needs no equivalent because `using`
//  is a namespace import, not a copy-paste.
#pragma once

// #include is LITERALLY a copy-paste performed by the preprocessor before
// compilation. It is NOT `using`. This line drops the entire text of JUCE's
// audio-processing module in at this point.
#include <juce_audio_processors/juce_audio_processors.h>


//==============================================================================
/**
    The audio engine.

    `: public juce::AudioProcessor` is inheritance. In C# you'd write
    `: AudioProcessor`. The extra `public` keyword matters: C++ also allows
    `private` and `protected` inheritance (which hide the base class from
    outside callers). You almost always want `public` — it means the same thing
    C# inheritance means.

    AudioProcessor is an abstract base class: it has methods marked `= 0`,
    which is C++ for C#'s `abstract`. We MUST implement every one of them or
    our class stays abstract and won't compile.

    `juce::` is a namespace qualifier. `::` is the scope-resolution operator —
    it's C++'s `.` for namespaces and static members.
*/
class SimpleGainAudioProcessor : public juce::AudioProcessor
{
public:
    //==========================================================================
    // Construction / destruction

    SimpleGainAudioProcessor();

    // The DESTRUCTOR. The `~` prefix means "clean-up function", and C++ runs it
    // automatically and deterministically the moment the object dies — when it
    // goes out of scope, or when something that owns it is destroyed.
    //
    // This is the single biggest difference from C#. There is no garbage
    // collector, and no nondeterministic finalizer. The nearest C# concept is
    // IDisposable + `using`, except here it's automatic and impossible to
    // forget. This pattern is called RAII (Resource Acquisition Is
    // Initialisation) and it's the backbone of modern C++.
    //
    // `override` here is because the base class destructor is virtual.
    ~SimpleGainAudioProcessor() override;

    //==========================================================================
    // The audio lifecycle — the three methods that actually matter

    /** Called by the host before playback starts, and again whenever the
        sample rate or block size changes.

        THIS is where you allocate memory, size buffers, and reset state.
        It runs on a normal thread with no deadline, so it's safe here.
    */
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;

    /** Called when playback stops. Free anything expensive. */
    void releaseResources() override;

    /** THE REAL-TIME CALLBACK. Everything in this plugin exists to serve this
        one method.

        The host hands us a block of audio; we modify it IN PLACE; we return.
        At 48kHz with a 256-sample block this runs ~187 times a second and has
        about 5 milliseconds to complete. Miss that deadline and the user hears
        a click in their recording.

        So inside processBlock: no allocation, no locks, no file I/O, no
        logging, no anything that could block. Nothing here has a C# analogue,
        because in C# the GC could pause you at any moment.

        Note the `&` on the parameters. That's a REFERENCE — an alias for the
        caller's object, not a copy. It's the closest thing to how C# passes
        class instances by default, and it's how we can modify the host's
        buffer in place. (An `AudioBuffer<float>` passed by value would copy
        the entire block of audio — a disaster here.)
    */
    void processBlock (juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer& midiMessages) override;

    /** Tells the host which channel configurations we can handle.
        We'll accept mono->mono and stereo->stereo, and refuse anything else.

        Note `const` appears twice and means two different things:
          - `const BusesLayout&`  : we promise not to modify the argument.
          - `... ) const override`: we promise this method doesn't modify
                                    THIS OBJECT. C# has no equivalent; the
                                    nearest thing is a readonly struct method.
    */
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    //==========================================================================
    // The GUI

    /** A factory method. The host calls this when the user opens the plugin
        window. We return a NEW editor each time; the host owns and deletes it. */
    juce::AudioProcessorEditor* createEditor() override;

    /** Do we have a custom GUI at all? (If false, the host draws a generic one.) */
    bool hasEditor() const override;

    //==========================================================================
    // Save / restore — filled in properly in a later step

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    // Parameters

    /** Builds the list of parameters this plugin exposes to the host.

        `static` means the same thing as in C#: it belongs to the class, not to
        any instance. It has to be static because we call it in the member
        initializer list to construct `apvts` — at which point `this` object is
        still half-built, so calling a normal member function would be unsafe.

        Returns a ParameterLayout *by value*. In C# that would imply a copy; in
        modern C++ the compiler moves it instead (and usually elides the move
        entirely), so nothing is actually copied. This is why C++ can return big
        objects from functions without the cost you'd expect.
    */
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    /** The parameter state object.

        This one object does three jobs that would otherwise be three separate
        chunks of code:
          1. exposes "gain" to the host, so a DAW can automate it
          2. serialises to/from XML, so settings survive a project reload
          3. binds to GUI controls, so a slider and the parameter stay in sync

        It's public because the editor needs to reach it to attach a slider.
    */
    juce::AudioProcessorValueTreeState apvts;

    //==========================================================================
    // Boilerplate the host asks about. Mostly one-liners.

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;

    /** How long sound keeps ringing after input stops — matters for reverbs
        and delays so the host knows when it can stop processing. A gain has
        no tail, so: 0. */
    double getTailLengthSeconds() const override;

    /** "Programs" are an old VST2 concept for built-in presets. Modern plugins
        use the host's own preset system, so we report the minimum legal
        answer: exactly one program, named after the plugin. */
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

private:
    //==========================================================================
    /** A cached pointer to the gain parameter's current value.

        Two things are going on here, and both matter.

        **Why cache it?** `apvts.getRawParameterValue("gain")` does a string
        lookup. processBlock runs ~187 times a second with a hard deadline, so
        we do the lookup ONCE in the constructor and keep the pointer.

        **Why `std::atomic`?** This value is written by the message thread (the
        user dragging a slider) and read by the audio thread (processBlock) —
        two threads touching one variable. With a plain `float` that's a data
        race, which in C++ is formally *undefined behaviour*: the compiler is
        allowed to assume nobody else touches it and cache it in a register
        forever, so your slider silently stops working in Release builds.
        `std::atomic<float>` tells the compiler "hands off" and guarantees you
        read a whole value, never a half-written one.

        On x86 an atomic float load costs the same as a normal one — this
        safety is effectively free. The nearest C# equivalents are `volatile`
        and `Interlocked`, but the consequences here are harsher: a glitch on
        the audio thread is an audible click in someone's recording.

        A raw pointer (`*`) rather than a reference (`&`) because it's assigned
        in the constructor *body*, not the initializer list — and references
        can't be rebound after construction. We don't own it; the APVTS does.
    */
    std::atomic<float>* gainDbParameter = nullptr;

    //==========================================================================
    // A MACRO — a preprocessor text substitution, expanding into real code
    // before the compiler ever sees it. This one does two things:
    //
    //   1. Deletes the copy constructor and copy assignment, so this class
    //      CANNOT be accidentally copied. In C# every class is a reference
    //      type and copying isn't a thing; in C++ every class is copyable by
    //      default, which for an audio processor would be a catastrophe.
    //
    //   2. Adds a leak detector that shouts in the debugger if an instance
    //      is still alive at shutdown — a poor man's GC diagnostic.
    //
    // Note there's no semicolon after it; the macro supplies its own.
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleGainAudioProcessor)
};
