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
    juce::NormalisableRange gainRange { -60.0f, 30.0f, 0.1f };

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
            .withStringFromValueFunction ([] (const float valueInDb, int)
            {
                return juce::String (valueInDb, 1) + " dB";
            })));

    return layout;
}


//==============================================================================
//  Construction
//==============================================================================
SimpleGainAudioProcessor::SimpleGainAudioProcessor()
    // Everything between `:` and `{` is the member initialiser list — members
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
void SimpleGainAudioProcessor::prepareToPlay (const double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    // A 20 ms glide, converted to samples using the host's real sample rate.
    smoothedGain.reset (sampleRate, gainRampSeconds);

    // reset() leaves the value at 0, so start AT the fader's position — or
    // playback would fade in from silence.
    // -> documentation/smoothing-and-the-audio-loop.md section 3
    smoothedGain.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (gainDbParameter->load()));
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

    // RAII guard: until this scope ends, denormal floats are flushed to zero.
    // Left alone they can stall the CPU. -> documentation/smoothing-and-the-audio-loop.md section 7
    juce::ScopedNoDenormals noDenormals;

    // Buffers are reused between calls and are NOT zeroed, so any output
    // channel without a matching input still holds last call's audio.
    const auto numInputChannels  = getTotalNumInputChannels();
    const auto numOutputChannels = getTotalNumOutputChannels();

    for (auto channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, buffer.getNumSamples());

    // .load() reads the atomic — explicit, so a non-atomic read can't happen
    // by accident. dB -> multiplier, because multiplying is what changes loudness.
    const auto gainInDecibels = gainDbParameter->load();

    // Cheap to call every block: it does nothing unless the value has changed.
    smoothedGain.setTargetValue (juce::Decibels::decibelsToGain (gainInDecibels));

    const auto numSamples  = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (smoothedGain.isSmoothing())
    {
        // Samples OUTER, channels INNER: the gain must advance once per sample
        // and be the same for every channel.
        // -> documentation/smoothing-and-the-audio-loop.md sections 4 and 5
        //
        // `channels[channel][sample]` indexes a raw float array — no bounds check.
        // -> documentation/smoothing-and-the-audio-loop.md section 6
        auto* const* channels = buffer.getArrayOfWritePointers();

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto gain = smoothedGain.getNextValue();

            for (int channel = 0; channel < numChannels; ++channel)
                channels[channel][sample] *= gain;
        }
    }
    else
    {
        // Steady gain: one multiply across the whole block.
        buffer.applyGain (smoothedGain.getTargetValue());
    }
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
    // A factory: the host calls this each time the window opens, and deletes
    // the result when it closes. Bare `new` is correct here because the host
    // takes ownership. -> documentation/editor-gui.md section 11
    return new SimpleGainAudioProcessorEditor (*this);
}


//==============================================================================
//  Save / restore
//==============================================================================
void SimpleGainAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // parameters -> ValueTree -> XML -> bytes. copyState() is a thread-safe
    // snapshot of every parameter's current value; it takes a lock, which is
    // fine here because hosts call this off the audio thread.
    // -> documentation/state-save-and-restore.md sections 2 and 8
    if (const auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SimpleGainAudioProcessor::setStateInformation (const void* data, const int sizeInBytes)
{
    // These bytes come out of a project file, so trust nothing: it must be
    // readable XML AND our document, otherwise ignore it. The tag is compared
    // exactly: hasTagName() ignores case, but XML tags are case-sensitive.
    // -> documentation/state-save-and-restore.md section 6
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->getTagName() == apvts.state.getType().toString())
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
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
