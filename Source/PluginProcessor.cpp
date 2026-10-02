// =============================================================================
//  PluginProcessor.cpp — the audio engine
//
//  Comments here are short on purpose. The reasoning lives in documentation/ —
//  see documentation/README.md for the index.
// =============================================================================

#include "PluginProcessor.h"
#include "PluginEditor.h"


//==============================================================================
//  Parameters
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
    SimpleGainAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Gain in DECIBELS, not a raw multiplier: dB is logarithmic, which matches
    // how hearing works, so the fader feels evenly spaced.
    //
    // Range is -60 dB (effectively silent) to +30 dB (about 32x), matching
    // Logic's own Gain plugin. No skew — dB is already a log scale, so skewing
    // would apply the logarithm twice.
    // -> documentation/parameters-and-automation.md sections 2 and 3
    juce::NormalisableRange<float> gainRange { -60.0f, 30.0f, 0.1f };

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        // The `1` is a version hint for hosts. Never change it for an existing
        // parameter — only bump it if parameters are reorganised in a release.
        juce::ParameterID { "gain", 1 },

        "Gain",          // the name the DAW displays
        gainRange,
        0.0f,            // default: unity — no change to the signal

        juce::AudioParameterFloatAttributes()
            .withLabel ("dB")
            // Turns the raw float into the text the host shows: "-6.0 dB"
            // rather than "-6.000000". `[]` captures nothing, which matters —
            // this lambda outlives the function that created it.
            // -> documentation/parameters-and-automation.md section 5
            .withStringFromValueFunction ([] (float valueInDb, int)
            {
                return juce::String (valueInDb, 1) + " dB";
            })));

    return layout;
}


//==============================================================================
//  Construction
//==============================================================================
SimpleGainAudioProcessor::SimpleGainAudioProcessor()
    // Everything between `:` and `{` is the member initializer list — members
    // are CONSTRUCTED with these values rather than assigned afterwards.
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      // *this = the owning processor, nullptr = no UndoManager,
      // "PARAMETERS" = the XML tag used when state is saved.
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // Looked up once, here, because this does a string lookup and processBlock
    // can't afford one. -> documentation/parameters-and-automation.md section 4
    gainDbParameter = apvts.getRawParameterValue ("gain");

    // C++'s Debug.Assert — fires if this ID ever drifts from the one above.
    jassert (gainDbParameter != nullptr);
}

SimpleGainAudioProcessor::~SimpleGainAudioProcessor() = default;


//==============================================================================
//  The audio lifecycle
//==============================================================================
void SimpleGainAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // The host's "here are the conditions, allocate now" call. Nothing to
    // prepare yet. ignoreUnused silences the unused-parameter warning, like
    // C#'s `_` discard.
    juce::ignoreUnused (sampleRate, samplesPerBlock);
}

void SimpleGainAudioProcessor::releaseResources()
{
    // Nothing allocated, so nothing to free.
}

bool SimpleGainAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainOutput = layouts.getMainOutputChannelSet();
    const auto& mainInput  = layouts.getMainInputChannelSet();

    if (mainOutput != juce::AudioChannelSet::mono()
        && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    // A gain plugin can't invent or discard channels, so in must match out.
    return mainInput == mainOutput;
}

void SimpleGainAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                             juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    // Buffers are reused between calls and are NOT zeroed, so any output
    // channel without a matching input still holds last call's audio.
    const auto numInputChannels  = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();

    for (auto channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    // .load() reads the atomic — explicit, so a non-atomic read can't happen
    // by accident.
    const auto gainInDecibels = gainDbParameter->load();

    // dB -> multiplier, because multiplying is what actually changes loudness.
    const auto gainLinear = juce::Decibels::decibelsToGain (gainInDecibels);

    buffer.applyGain (gainLinear);

    // KNOWN FLAW, deliberately left in: this reads the fader once per BLOCK and
    // applies one value to all ~512 samples, so fast fader moves step rather
    // than slide — audible as zipper noise. Phase 4 fixes it with SmoothedValue.
    // -> documentation/parameters-and-automation.md section 6
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
    // TEMPORARY: JUCE generates a working slider per parameter, so the gain is
    // audible before any interface code exists. Our own editor lands in Phase 6.
    //
    // Bare `new` is correct here — the host takes ownership and deletes it.
    return new juce::GenericAudioProcessorEditor (*this);
}


//==============================================================================
//  Save / restore — implemented in Phase 5
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
//  Boilerplate the host asks about
//==============================================================================
const juce::String SimpleGainAudioProcessor::getName() const
{
    return JucePlugin_Name;   // #define generated from PRODUCT_NAME in CMakeLists
}

bool   SimpleGainAudioProcessor::acceptsMidi()  const { return false; }
bool   SimpleGainAudioProcessor::producesMidi() const { return false; }
bool   SimpleGainAudioProcessor::isMidiEffect() const { return false; }
double SimpleGainAudioProcessor::getTailLengthSeconds() const { return 0.0; }

// Legacy VST2 "programs". Reporting 0 is illegal in some hosts, so report 1.
int  SimpleGainAudioProcessor::getNumPrograms()    { return 1; }
int  SimpleGainAudioProcessor::getCurrentProgram() { return 0; }

void SimpleGainAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String SimpleGainAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void SimpleGainAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}


//==============================================================================
//  Entry point — the one function every JUCE plugin must provide. Each format
//  wrapper (AU, VST3, Standalone) calls it to create an instance. A free
//  function, outside any class, which C# has no equivalent of.
//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleGainAudioProcessor();
}
