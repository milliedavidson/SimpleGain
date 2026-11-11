// =============================================================================
//  PluginEditor.cpp — the plugin window
//
//  Comments are short on purpose — the reasoning lives in documentation/.
// =============================================================================

#include "PluginEditor.h"

namespace
{
    // The same purple palette as the README banner.
    const juce::Colour backgroundTop    { 0xff1a1530 };
    const juce::Colour backgroundBottom { 0xff341f4d };
    const juce::Colour accent           { 0xffa78bfa };
    const juce::Colour track            { 0xff4c3d70 };
    const juce::Colour textLight        { 0xfff5f3ff };
    const juce::Colour textMuted        { 0xff9c8fc4 };

    constexpr int defaultWidth  = 340;
    constexpr int defaultHeight = 440;
}


//==============================================================================
SimpleGainAudioProcessorEditor::SimpleGainAudioProcessorEditor (SimpleGainAudioProcessor& p)
    : AudioProcessorEditor (&p),      // the base class wants a POINTER, so we pass &p
      // The slider is declared above this, so it already exists: safe to connect.
      // -> documentation/editor-gui.md section 4
      gainAttachment (p.apvts, "gain", gainSlider)
{
    titleLabel.setText ("SimpleGain", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, textLight);
    addAndMakeVisible (titleLabel);

    // A rotary knob with an editable value box. The ID lets the tests find it.
    // -> documentation/editor-gui.md section 3
    gainSlider.setComponentID ("gainSlider");
    gainSlider.setTitle ("Gain");                       // what a screen reader announces
    gainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 120, 28);   // resized() rescales it
    gainSlider.setColour (juce::Slider::rotarySliderFillColourId,    accent);
    gainSlider.setColour (juce::Slider::rotarySliderOutlineColourId, track);
    gainSlider.setColour (juce::Slider::thumbColourId,               textLight);
    gainSlider.setColour (juce::Slider::textBoxTextColourId,         textLight);
    gainSlider.setColour (juce::Slider::textBoxOutlineColourId,      track);
    gainSlider.setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    gainSlider.setColour (juce::Slider::textBoxHighlightColourId,    accent);
    addAndMakeVisible (gainSlider);

    // Resizable by the host or by dragging the corner, between 75% and 200%,
    // always keeping the opening shape. -> documentation/editor-gui.md section 8
    setResizable (true, true);
    setResizeLimits (defaultWidth * 3 / 4, defaultHeight * 3 / 4, defaultWidth * 2, defaultHeight * 2);
    getConstrainer()->setFixedAspectRatio (static_cast<double> (defaultWidth) / defaultHeight);
    setSize (defaultWidth, defaultHeight);
}

SimpleGainAudioProcessorEditor::~SimpleGainAudioProcessorEditor() = default;


//==============================================================================
void SimpleGainAudioProcessorEditor::paint (juce::Graphics& g)
{
    // -> documentation/editor-gui.md section 6
    g.setGradientFill (juce::ColourGradient::vertical (backgroundTop, backgroundBottom, getLocalBounds()));
    g.fillAll();

    // A small credit line along the bottom.
    g.setColour (textMuted);
    g.setFont (juce::FontOptions (static_cast<float> (getHeight()) * 0.035f));
    g.drawText ("Maxximum Displacement", getLocalBounds().removeFromBottom (getHeight() / 10),
                juce::Justification::centred);
}

void SimpleGainAudioProcessorEditor::resized()
{
    // Everything is proportional to the window, so it scales when resized.
    // `area` is carved up with removeFromTop/Bottom. -> documentation/editor-gui.md section 7
    const auto windowHeight = getHeight();
    auto area = getLocalBounds().reduced (getWidth() / 12);

    titleLabel.setFont (juce::FontOptions (static_cast<float> (windowHeight) / 14.0f, juce::Font::bold));
    titleLabel.setBounds (area.removeFromTop (windowHeight / 9));

    area.removeFromBottom (windowHeight / 12);          // room for the credit line drawn in paint()

    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, area.getWidth() / 2, windowHeight / 12);
    gainSlider.setBounds (area);
}
