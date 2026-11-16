// =============================================================================
//  CornerButtons.h — the two buttons in the top corners
//
//  The ring mark, top left, opens the website panel; the ?, top right, opens the
//  guide. Both are rings of the same size, and show x while their panel is open.
//  -> documentation/editor-gui.md section 16
// =============================================================================

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class SimpleGainLookAndFeel;


//==============================================================================
/**
    What both corner buttons share: a ring, muted until the mouse is over it, and
    x inside while the button's panel is open. Each kind fills in its own symbol.
*/
class CornerButton : public juce::Button
{
public:
    CornerButton (const juce::String& name, const SimpleGainLookAndFeel&);

    /** The button's bounds for a ring of `radius` centred on `centre`, with room around it to click. */
    [[nodiscard]] static juce::Rectangle<float> boundsFor (juce::Point<float> centre, float radius);

    void paintButton (juce::Graphics&, bool isMouseOver, bool isMouseDown) override;

protected:
    /** Draws this button's symbol, in the colour already set, for a ring of `radius` at `centre`.
        `= 0` makes it "pure virtual": like a C# abstract method, every kind of button must write one. */
    virtual void paintSymbol (juce::Graphics&, juce::Point<float> centre, float radius) = 0;

    /** The ring's line thickness, for a ring of `radius`. */
    [[nodiscard]] static float lineFor (float radius);

    const SimpleGainLookAndFeel& lookAndFeel;
};


//==============================================================================
/** The ? in the top-right corner, which opens the guide. */
class GuideButton final : public CornerButton
{
public:
    explicit GuideButton (const SimpleGainLookAndFeel&);

private:
    void paintSymbol (juce::Graphics&, juce::Point<float> centre, float radius) override;
};


//==============================================================================
/** The ring mark in the top-left corner, the brand's small logo, which opens the website panel. */
class MarkButton final : public CornerButton
{
public:
    explicit MarkButton (const SimpleGainLookAndFeel&);

private:
    void paintSymbol (juce::Graphics&, juce::Point<float> centre, float radius) override;
};
