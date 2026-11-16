// =============================================================================
//  PluginEditor.h — the plugin window
//
//  The editor is separate from the processor, and optional: a host can run the
//  plugin with the window closed, and opens and closes it many times. It lives
//  on the MESSAGE thread; processBlock lives on the audio thread.
//
//  Comments are short on purpose — the reasoning lives in documentation/.
// =============================================================================

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Fading.h"
#include "Guide.h"
#include "WebsitePanel.h"
#include "CornerButtons.h"

#include <optional>


//==============================================================================
/**
    A LookAndFeel is JUCE's "theme": one object that decides how widgets draw
    themselves. Ours carries the brand fonts, draws the knob (a slim ring with
    the value in the middle) and puts the value box inside the ring.
    -> documentation/editor-gui.md sections 9 and 13
*/
class SimpleGainLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    SimpleGainLookAndFeel();

    /** The brand's fonts, at a height in pixels: Inter for text, JetBrains Mono for readouts,
        and Sora Bold for the logo. */
    [[nodiscard]] juce::FontOptions textFont (float height) const;
    [[nodiscard]] juce::FontOptions monoFont (float height) const;
    [[nodiscard]] juce::FontOptions logoFont (float height) const;

    /** The ring's radius (the middle of its stroke) for a knob drawn in `knobArea`. */
    static float ringRadius (juce::Rectangle<float> knobArea);

    juce::Font getLabelFont (juce::Label&) override;

    /** The value box, made so clicks pass through it to the knob. */
    juce::Label* createSliderTextBox (juce::Slider&) override;

    /** The knob fills its whole area, and the value box sits in the middle of the ring. */
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;

    /** The track, the arc (quiet, apricot, amber, then ember) and the caption under the value. */
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPosProportional,
                           float rotaryStartAngle, float rotaryEndAngle, juce::Slider&) override;

    /** The resize grip in the bottom-right corner, drawn quietly. */
    void drawCornerResizer (juce::Graphics&, int width, int height, bool isMouseOver, bool isMouseDragging) override;

private:
    // Typeface::Ptr is reference-counted, like a C# object reference: the font data lives
    // as long as something points at it.
    juce::Typeface::Ptr inter, jetBrainsMono, sora;
};


//==============================================================================
/**
    The brand logo: the name in lower case in Sora Bold, drawn from the left edge,
    with a sound wave under it that grows until its last crest knocks the final
    letter up. -> documentation/editor-gui.md section 14
*/
class Wordmark final : public juce::Component
{
public:
    explicit Wordmark (const SimpleGainLookAndFeel&);

    void setText (const juce::String&);
    [[nodiscard]] const juce::String& getText() const noexcept   { return text; }
    [[nodiscard]] juce::Font getFont() const;

    /** Where letter `index` is drawn, the knocked-up last one included. */
    [[nodiscard]] juce::Rectangle<float> letterBounds (int index) const;

    void paint (juce::Graphics&) override;

private:
    [[nodiscard]] juce::GlyphArrangement layOut() const;
    [[nodiscard]] juce::AffineTransform knockUp (const juce::PositionedGlyph&) const;
    [[nodiscard]] juce::Path wave (const juce::GlyphArrangement&) const;

    const SimpleGainLookAndFeel& lookAndFeel;     // a reference: the editor owns it, and outlives this
    juce::String text;
};


//==============================================================================
/**
    A knob you turn by dragging round it, like a real one, but RELATIVELY: a click
    on its own never changes the value, and the value moves by the angle you turn.
    (JUCE's own Rotary style jumps to wherever you click — +30 dB at one end.)
    The number in the middle is a fine slider: drag it up or down. A plain click
    on it opens it for typing. -> documentation/editor-gui.md section 10
*/
class RelativeRotarySlider final : public juce::Slider
{
public:
    RelativeRotarySlider();

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    /** How far the number moves the gain for each pixel dragged up or down. */
    static constexpr double numberDbPerPixel = 0.05;

    /** What a plain click on the number does: opens it for typing, unless a test swaps it out. */
    std::function<void()> onNumberClicked;

private:
    [[nodiscard]] bool isOnTheNumber (juce::Point<float> position);

    /** The same event, turned round the knob's centre so the grab lines up with the value. */
    juce::MouseEvent turnedToTheDot (const juce::MouseEvent&);

    // Like C#'s double?: empty until a grab has a pointer angle to start from.
    std::optional<double> lastPointerAngle;
    double dotAngleUnlimited = 0.0;     // where the value would be with no end stops

    struct NumberDrag { double startValue; float startY; };
    std::optional<NumberDrag> numberDrag;     // only while a drag that began on the number goes on
};


//==============================================================================
/**
    The window: the ring mark and the name at the top, a gain knob, the limit
    readout under it, the brand logo, and the ? button that opens the guide.

    juce::AudioProcessorEditor is a juce::Component, JUCE's base class for
    anything drawable — roughly WPF's UIElement or WinForms' Control.
    -> documentation/editor-gui.md sections 1 and 2
*/
class SimpleGainAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    /** Takes the processor by REFERENCE: it can't be null, and we don't own it
        (it outlives every editor it creates). `explicit` stops C++ quietly
        converting a processor into an editor. */
    explicit SimpleGainAudioProcessorEditor (SimpleGainAudioProcessor&);

    ~SimpleGainAudioProcessorEditor() override;

    /** JUCE calls this when the window needs drawing. We never call it ourselves. */
    void paint (juce::Graphics&) override;

    /** JUCE calls this whenever the size changes: position the children here. */
    void resized() override;

private:
    //==========================================================================
    // ORDER MATTERS. Members are constructed top to bottom and destroyed bottom
    // to top: the look-and-feel must outlive the widgets that use it (so it is
    // first), and the attachment's destructor touches the slider (so it is last).
    // -> documentation/editor-gui.md section 5
    SimpleGainLookAndFeel lookAndFeel;
    juce::Label title;
    RelativeRotarySlider gainSlider;
    FadingLabel limitReadout;
    Wordmark logo { lookAndFeel };
    GuideView guide { lookAndFeel };
    WebsitePanel websitePanel { lookAndFeel };
    GuideButton guideButton { lookAndFeel };
    MarkButton markButton { lookAndFeel };
    juce::AudioProcessorValueTreeState::SliderAttachment gainAttachment;

    /** Which panel is over the controls. Only one is ever open. */
    enum class Panel { none, guide, website };
    void showPanel (Panel);

    /** Is the knob past the start of the ember, the last stretch before the limit? */
    [[nodiscard]] bool isInTheRed();      // not const: JUCE's valueToProportionOfLength() isn't

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleGainAudioProcessorEditor)
};
