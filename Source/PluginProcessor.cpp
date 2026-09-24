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
//  Parameters
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
    SimpleGainAudioProcessor::createParameterLayout()
{
    // ParameterLayout is a container the APVTS takes ownership of. We fill it
    // with parameters and hand it over.
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ----------------------------------------------------------------------
    // The gain parameter, measured in DECIBELS.
    //
    // Why dB and not a raw 0.0-2.0 multiplier? Because hearing is logarithmic.
    // Going 0.5 -> 1.0 and 1.0 -> 2.0 sound like equal-sized jumps in loudness
    // even though one adds 0.5 and the other adds 1.0. Decibels measure
    // *ratios*, so a fader in dB feels evenly spaced to a human ear.
    // ----------------------------------------------------------------------

    // NormalisableRange maps the range the USER sees (-60..+12 dB) onto the
    // 0.0-1.0 range every plugin host actually works in internally.
    //
    //   arg 1, 2 : minimum and maximum, in dB
    //   arg 3    : step size — the fader moves in 0.1 dB increments
    //
    // A note on "skew": JUCE lets you bend this mapping so one end of the
    // fader gets more travel. We deliberately DON'T here. Skew exists for
    // parameters measured in linear units — frequency in Hz, or a raw gain
    // multiplier — where a plain linear fader feels wrong. Our parameter is
    // already in dB, which IS a logarithmic scale, so it's already
    // perceptually even. Adding skew would effectively apply the log twice.
    juce::NormalisableRange<float> gainRange { -60.0f, 12.0f, 0.1f };

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        // The stable ID we look this parameter up by, in code and in saved
        // state. The `1` is a "version hint": if you ever reorganise
        // parameters in a future release, bumping this tells AU/VST3 hosts how
        // to keep old saved projects working. It must never change for an
        // existing parameter.
        juce::ParameterID { "gain", 1 },

        "Gain",          // the human-readable name the DAW displays
        gainRange,
        0.0f,            // default: 0 dB — i.e. unity, no change to the signal

        // Attributes are optional extras, built up with a chain of `with...`
        // calls. Each returns a NEW object rather than modifying in place —
        // the same immutable-builder style as C#'s `with` expressions on
        // records.
        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            // A LAMBDA — C++'s anonymous function. `[]` is the capture list:
            // C# closures capture variables implicitly, but C++ makes you
            // state what you're capturing, because lifetimes are manual and
            // capturing something that later dies is a real bug. Empty `[]`
            // means "capture nothing", which is exactly right here — this
            // outlives us, so it must not hold references to anything local.
            //
            // It converts the raw float into what the host displays, so you
            // get "-6.0 dB" instead of "-6.000000".
            .withStringFromValueFunction ([] (float valueInDb, int)
            {
                return juce::String (valueInDb, 1) + " dB";
            })));

    return layout;
}


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
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      // Construct the parameter state. Note this runs AFTER the AudioProcessor
      // base class above — members are initialised in declaration order, and
      // the base class always goes first.
      //
      //   *this           : the processor these parameters belong to
      //   nullptr         : no UndoManager (we don't need undo)
      //   "PARAMETERS"    : the XML tag name used when state is saved
      //   createParameterLayout() : the parameters themselves
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // Cache the pointer to the gain value ONCE, here, where it's safe to do
    // slow things. See the comment on gainDbParameter in the header for why
    // this matters so much.
    gainDbParameter = apvts.getRawParameterValue ("gain");

    // A sanity check that costs nothing in a Release build. If the ID above
    // and the ID in createParameterLayout() ever drift apart, this fires
    // immediately in the debugger rather than silently giving us a null
    // pointer to crash on later. It's the C++ equivalent of Debug.Assert.
    jassert (gainDbParameter != nullptr);
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

    // ----------------------------------------------------------------------
    //  THE ACTUAL DSP
    // ----------------------------------------------------------------------
    // Read the current fader position. `.load()` is how you read a
    // std::atomic — it's explicit so you can't accidentally do a non-atomic
    // read without noticing.
    const auto gainInDecibels = gainDbParameter->load();

    // Convert dB to a plain multiplier, because multiplying is what actually
    // makes sound louder or quieter. The maths is 10^(dB/20):
    //     0 dB -> 1.0    (unchanged)
    //    -6 dB -> 0.501  (about half)
    //   +6 dB -> 1.995   (about double)
    //   -60 dB -> 0.001  (effectively silent)
    const auto gainLinear = juce::Decibels::decibelsToGain (gainInDecibels);

    // Multiply every sample in every channel by that number. One line, and
    // it's the entire signal processing of this plugin.
    buffer.applyGain (gainLinear);

    // ----------------------------------------------------------------------
    // ⚠️ This is deliberately the NAIVE version, and it has a real flaw.
    //
    // We read the fader ONCE per block and apply that one value to all 512
    // samples. So when you drag the fader, the gain doesn't slide smoothly —
    // it jumps in steps, once per block. Each jump is a discontinuity in the
    // waveform, and your ear hears discontinuities as clicks. Drag fast and
    // you get a gritty "zipper" sound.
    //
    // Phase 4 fixes this with juce::SmoothedValue, which ramps the gain
    // per-sample instead of per-block. Left visibly broken for now so the
    // problem is audible before the fix appears.
    // ----------------------------------------------------------------------
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
    // TEMPORARY: JUCE can auto-generate a functional GUI straight from the
    // parameter list — a slider per parameter, correctly wired up. It's ugly,
    // but it means we can HEAR the gain working before spending any time on
    // interface code. We swap back to our own editor in Phase 6.
    return new juce::GenericAudioProcessorEditor (*this);
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
