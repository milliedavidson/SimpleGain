// =============================================================================
//  PluginEditor.cpp — the plugin window
//
//  Comments are short on purpose — the reasoning lives in documentation/.
// =============================================================================

#include "PluginEditor.h"

namespace
{
    // Near-black greys with one warm apricot accent.
    const juce::Colour backgroundTop    { 0xff18181b };
    const juce::Colour backgroundBottom { 0xff111113 };
    const juce::Colour accent           { 0xfff2a766 };
    const juce::Colour track            { 0xff2c2c31 };
    const juce::Colour textLight        { 0xffeceae6 };
    const juce::Colour textMuted        { 0xff77757c };

    constexpr int defaultWidth  = 340;
    constexpr int defaultHeight = 440;
}


//==============================================================================
juce::Font SimpleGainLookAndFeel::getLabelFont (juce::Label& label)
{
    // Only the knob's value box: half the box's height, so it scales and always fits.
    // -> documentation/editor-gui.md section 9
    if (dynamic_cast<juce::Slider*> (label.getParentComponent()) != nullptr)
        return juce::FontOptions (static_cast<float> (label.getHeight()) * 0.5f);

    return LookAndFeel_V4::getLabelFont (label);
}


void SimpleGainLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                              float position, float startAngle, float endAngle,
                                              juce::Slider& slider)
{
    // A slim track ring, with the accent arc laid over it up to the current value.
    // -> documentation/editor-gui.md section 9
    const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
    const auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto thickness = juce::jmax (2.0f, diameter * 0.025f);
    const auto radius = diameter * 0.5f - thickness;
    const auto centre = bounds.getCentre();

    const auto strokeArc = [&] (float from, float to, juce::Colour colour)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, from, to, true);
        g.setColour (colour);
        g.strokePath (arc, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    strokeArc (startAngle, endAngle, slider.findColour (juce::Slider::rotarySliderOutlineColourId));

    if (position > 0.0f)
        strokeArc (startAngle, startAngle + position * (endAngle - startAngle),
                   slider.findColour (juce::Slider::rotarySliderFillColourId));
}

juce::Slider::SliderLayout SimpleGainLookAndFeel::getSliderLayout (juce::Slider& slider)
{
    // The drag maths in RelativeRotarySlider reads sliderBounds' centre, so keep it the knob's square.
    const auto area = slider.getLocalBounds();
    const auto diameter = juce::jmin (area.getWidth(), area.getHeight());

    juce::Slider::SliderLayout layout;
    layout.sliderBounds = area.withSizeKeepingCentre (diameter, diameter);
    layout.textBoxBounds = layout.sliderBounds.withSizeKeepingCentre (juce::roundToInt (diameter * 0.74f),
                                                                      juce::roundToInt (diameter * 0.3f));
    return layout;
}


//==============================================================================
RelativeRotarySlider::RelativeRotarySlider()
{
    setSliderStyle (Slider::Rotary);      // circular dragging; the class makes it relative
}

void RelativeRotarySlider::mouseDown (const juce::MouseEvent& e)
{
    lastPointerAngle.reset();                   // a new grab: measure afresh
    Slider::mouseDown (turnedToTheDot (e));
}

void RelativeRotarySlider::mouseDrag (const juce::MouseEvent& e)
{
    Slider::mouseDrag (turnedToTheDot (e));
}

juce::MouseEvent RelativeRotarySlider::turnedToTheDot (const juce::MouseEvent& e)
{
    // JUCE measures angles from the knob's centre: 0 at 12 o'clock, clockwise, in radians.
    // -> documentation/editor-gui.md section 10
    const auto centre = getLookAndFeel().getSliderLayout (*this).sliderBounds.getCentre().toFloat();
    const auto fromCentre = e.position - centre;
    const auto distance = fromCentre.getDistanceFromOrigin();

    if (distance <= 5.0f)                       // too close to have an angle; JUCE ignores it too
        return e;

    const auto pointerAngle = std::atan2 (static_cast<double> (fromCentre.x), static_cast<double> (-fromCentre.y));
    const auto rotary = getRotaryParameters();

    if (! lastPointerAngle.has_value())         // the grab starts where the dot already is: no jump
        dotAngleUnlimited = rotary.startAngleRadians
                          + (rotary.endAngleRadians - rotary.startAngleRadians) * valueToProportionOfLength (getValue());
    else                                        // turn the dot by however far the pointer turned, the short way round
        dotAngleUnlimited += std::remainder (pointerAngle - *lastPointerAngle, juce::MathConstants<double>::twoPi);

    lastPointerAngle = pointerAngle;

    // Past an end the dot waits there until the pointer comes back, like a real knob's end stop.
    const auto angle = juce::jlimit (static_cast<double> (rotary.startAngleRadians),
                                     static_cast<double> (rotary.endAngleRadians), dotAngleUnlimited);
    return e.withNewPosition (centre + juce::Point { distance * static_cast<float> (std::sin (angle)),
                                                           -distance * static_cast<float> (std::cos (angle)) });
}


//==============================================================================
SimpleGainAudioProcessorEditor::SimpleGainAudioProcessorEditor (SimpleGainAudioProcessor& p)
    : AudioProcessorEditor (&p),      // the base class wants a POINTER, so we pass &p
      // The slider is declared above this, so it already exists: safe to connect.
      // -> documentation/editor-gui.md section 4
      gainAttachment (p.apvts, "gain", gainSlider)
{
    setLookAndFeel (&lookAndFeel);      // must be undone in the destructor

    titleLabel.setText ("SimpleGain", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centred);
    titleLabel.setColour (juce::Label::textColourId, textMuted);
    addAndMakeVisible (titleLabel);

    // A rotary knob with an editable value box. The ID lets the tests find it.
    // -> documentation/editor-gui.md section 3
    gainSlider.setComponentID ("gainSlider");
    gainSlider.setTitle ("Gain");                       // what a screen reader announces
    gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 120, 28);   // makes the value box; the look-and-feel places it
    gainSlider.setColour (juce::Slider::rotarySliderFillColourId,    accent);
    gainSlider.setColour (juce::Slider::rotarySliderOutlineColourId, track);
    gainSlider.setColour (juce::Slider::textBoxTextColourId,         textLight);
    gainSlider.setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
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

SimpleGainAudioProcessorEditor::~SimpleGainAudioProcessorEditor()
{
    setLookAndFeel (nullptr);           // children are still alive, so detach before they go
}


//==============================================================================
void SimpleGainAudioProcessorEditor::paint (juce::Graphics& g)
{
    // -> documentation/editor-gui.md section 6
    g.setGradientFill (juce::ColourGradient::vertical (backgroundTop, backgroundBottom, getLocalBounds()));
    g.fillAll();

    // Small caption under the value, inside the ring, and a credit line along the bottom.
    g.setColour (textMuted);
    g.setFont (juce::FontOptions (static_cast<float> (getHeight()) * 0.03f));
    const auto knob = gainSlider.getBounds();
    g.drawText ("GAIN", knob.withTop (knob.getCentreY() + knob.getHeight() / 8).withHeight (getHeight() / 20),
                juce::Justification::centred);

    g.drawText ("Maxximum Displacement", getLocalBounds().removeFromBottom (getHeight() / 10),
                juce::Justification::centred);
}

void SimpleGainAudioProcessorEditor::resized()
{
    // Everything is proportional to the window, so it scales when resized.
    // `area` is carved up with removeFromTop/Bottom. -> documentation/editor-gui.md section 7
    const auto windowHeight = getHeight();
    auto area = getLocalBounds().reduced (getWidth() / 12);

    titleLabel.setFont (juce::FontOptions (static_cast<float> (windowHeight) / 22.0f, juce::Font::bold));
    titleLabel.setBounds (area.removeFromTop (windowHeight / 9));

    area.removeFromBottom (windowHeight / 12);          // room for the credit line drawn in paint()

    gainSlider.setBounds (area);
}
