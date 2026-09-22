// =============================================================================
//  PluginProcessor.cpp  —  the IMPLEMENTATION of our audio engine
// =============================================================================

#include "PluginProcessor.h"
#include "PluginEditor.h"

// Note the two include styles:
//   <angle brackets> — look in the system / library include paths (JUCE).
//   "quotes"         — look next to this file first (our own code).
// It's a convention, not a hard rule, but everyone follows it.


//==============================================================================
//  Constructor
//==============================================================================
SimpleGainAudioProcessor::SimpleGainAudioProcessor()
    // ----------------------------------------------------------------------
    // Everything between the `:` and the `{` is the MEMBER INITIALIZER LIST.
    // It runs before the constructor body and is how you pass arguments to
    // base-class and member constructors.
    //
    // In C# you'd write `: base(...)` for the base class and assign fields
    // inside the constructor body. In C++ the distinction is real: members
    // initialised here are CONSTRUCTED with that value; anything assigned in
    // the body is default-constructed first and then overwritten. For cheap
    // types that's just wasteful; for types with no default constructor it
    // won't compile at all. So: prefer the initializer list.
    // ----------------------------------------------------------------------
    : AudioProcessor (BusesProperties()
                        // Declare our audio input/output layout to the host.
                        // Default to stereo in, stereo out. The host may later
                        // negotiate mono via isBusesLayoutSupported().
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    // Constructor body. Nothing to do yet — parameters arrive in the next step.
}

// The destructor. Empty, because we own nothing that needs manual cleanup —
// which is the goal. Every member we'll add cleans itself up via RAII.
SimpleGainAudioProcessor::~SimpleGainAudioProcessor()
{
}

// Note the `SimpleGainAudioProcessor::` prefix on every definition above and
// below. In the header these were declared inside the class body; out here
// we must say which class they belong to. C# never needs this because the
// method body is always physically inside the class.


//==============================================================================
//  The audio lifecycle
//==============================================================================
void SimpleGainAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // The host tells us the conditions we're about to run under. This is the
    // safe place to allocate and reset. Right now we have nothing to prepare,
    // so we explicitly ignore the arguments.
    //
    // juce::ignoreUnused() exists purely to silence "unused parameter"
    // warnings. We compile with warnings-as-errors-level strictness (that's
    // what juce_recommended_warning_flags gave us in CMakeLists.txt), so
    // unused parameters would otherwise be noise. C# has the `_` discard for
    // a similar purpose.
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void SimpleGainAudioProcessor::releaseResources()
{
    // Called when playback stops. Nothing allocated, so nothing to free.
}

bool SimpleGainAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // `const auto&` — `auto` is C++'s `var`. The `&` avoids copying the object,
    // and `const` promises we won't modify it. This combination (`const auto&`)
    // is the default way to name a thing you only want to read.
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    const auto& mainInput  = layouts.getMainInputChannelSet();

    // Accept only mono or stereo output.
    if (mainOutput != juce::AudioChannelSet::mono()
        && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    // Input must match output. A gain plugin can't invent or discard channels.
    return mainInput == mainOutput;
}

void SimpleGainAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    // We take no MIDI, so ignore it.
    juce::ignoreUnused (midiMessages);

    // ----------------------------------------------------------------------
    // For now this is a PASS-THROUGH: audio comes in, the same audio goes out.
    // We touch nothing. This is deliberate — it proves the whole toolchain
    // works (build, load, route audio) before we add any DSP that could
    // confuse a "it doesn't work" with "it doesn't build".
    //
    // The one thing we must still do is clear any output channels the host
    // gave us that have no corresponding input. Buffers are reused between
    // calls and are NOT zeroed for you, so an untouched output channel
    // contains whatever was in it last time — i.e. loud garbage.
    // ----------------------------------------------------------------------
    const auto numInputChannels  = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();

    for (auto channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    // `++channel` rather than `channel++`: both work, but pre-increment is the
    // C++ habit. For simple ints they're identical; for heavier iterator types
    // post-increment has to make a copy, so the community defaults to `++x`.
}


//==============================================================================
//  The GUI
//==============================================================================
bool SimpleGainAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* SimpleGainAudioProcessor::createEditor()
{
    // `new` heap-allocates and returns a raw pointer. This is one of the very
    // few places modern C++ still uses a bare `new` — and it's only OK because
    // the JUCE host takes ownership and guarantees it will delete it.
    //
    // `*this` dereferences the `this` pointer to get a reference to ourselves,
    // which we hand to the editor so it can talk back to us. In C# you'd just
    // write `this`; in C++ `this` is a POINTER, so `*this` converts it to a
    // reference to match the editor's constructor signature.
    return new SimpleGainAudioProcessorEditor (*this);
}


//==============================================================================
//  Save / restore  (implemented properly once we have parameters)
//==============================================================================
void SimpleGainAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ignoreUnused (destData);
}

void SimpleGainAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
}


//==============================================================================
//  Boilerplate
//==============================================================================
const juce::String SimpleGainAudioProcessor::getName() const
{
    // JUCE_PROJECT_NAME is a #define that CMakeLists.txt generated for us from
    // the PRODUCT_NAME we set. JucePlugin_Name is JUCE's wrapper around it.
    return JucePlugin_Name;
}

bool   SimpleGainAudioProcessor::acceptsMidi()  const { return false; }
bool   SimpleGainAudioProcessor::producesMidi() const { return false; }
bool   SimpleGainAudioProcessor::isMidiEffect() const { return false; }
double SimpleGainAudioProcessor::getTailLengthSeconds() const { return 0.0; }

// Legacy VST2 "programs". Reporting 0 programs is illegal in some hosts,
// so the convention is to report exactly 1 and ignore the rest.
int  SimpleGainAudioProcessor::getNumPrograms()    { return 1; }
int  SimpleGainAudioProcessor::getCurrentProgram() { return 0; }

void SimpleGainAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String SimpleGainAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};   // `{}` value-initialises the return type — here, an empty String.
}

void SimpleGainAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}


//==============================================================================
//  THE ENTRY POINT
//==============================================================================
//  This is the one function every JUCE plugin must provide. The format wrappers
//  (AU, VST3, Standalone) each call it to create an instance of your processor.
//  It is the closest thing this project has to a `Main()`.
//
//  It sits OUTSIDE the class — a free function, which C# doesn't have at all
//  (everything in C# must live in a type). JUCE_CALLTYPE is a macro specifying
//  the calling convention, which matters on Windows and expands to nothing here.
//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleGainAudioProcessor();
}
