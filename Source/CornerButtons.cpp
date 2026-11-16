// =============================================================================
//  CornerButtons.cpp — the two buttons in the top corners
//
//  Comments are short on purpose — the reasoning lives in documentation/.
// =============================================================================

#include "CornerButtons.h"
#include "Brand.h"
#include "PluginEditor.h"       // SimpleGainLookAndFeel, for the fonts

namespace
{
    // The ring mark's outer ring, as fractions of its radius, measured from the brand plan.
    constexpr float ringLine  = 0.18f;
    constexpr float ringInk   = 1.0f + ringLine / 2.0f;      // the outer edge of the line
    constexpr float clickRoom = 1.35f;                       // the button is this much bigger than the ring
}


//==============================================================================
CornerButton::CornerButton (const juce::String& name, const SimpleGainLookAndFeel& lf)
    : juce::Button (name),
      lookAndFeel (lf)
{
    setTitle (name);                    // what a screen reader announces
}

juce::Rectangle<float> CornerButton::boundsFor (const juce::Point<float> centre, const float radius)
{
    const auto side = 2.0f * radius * ringInk * clickRoom;
    return juce::Rectangle<float> (side, side).withCentre (centre);
}

float CornerButton::lineFor (const float radius)
{
    return radius * ringLine;
}

void CornerButton::paintButton (juce::Graphics& g, const bool isMouseOver, const bool isMouseDown)
{
    // Muted, brightening under the mouse. While the panel is open: a ring with x, to close it.
    // -> documentation/editor-gui.md section 16
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const auto radius = bounds.getWidth() / (2.0f * ringInk * clickRoom);

    g.setColour (isMouseOver || isMouseDown ? tokens::text : tokens::muted);

    if (getToggleState())
    {
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), lineFor (radius));
        g.setFont (lookAndFeel.textFont (radius * 1.3f));
        g.drawText (juce::String::charToString (0x00d7), bounds, juce::Justification::centred);     // U+00D7, a multiplication sign
    }
    else
    {
        paintSymbol (g, centre, radius);
    }
}


//==============================================================================
GuideButton::GuideButton (const SimpleGainLookAndFeel& lf)
    : CornerButton ("Guide", lf)
{
}

void GuideButton::paintSymbol (juce::Graphics& g, const juce::Point<float> centre, const float radius)
{
    g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre), lineFor (radius));
    g.setFont (lookAndFeel.textFont (radius * 1.3f));
    g.drawText ("?", getLocalBounds().toFloat(), juce::Justification::centred);
}


//==============================================================================
MarkButton::MarkButton (const SimpleGainLookAndFeel& lf)
    : CornerButton ("maxximum displacement website", lf)
{
}

void MarkButton::paintSymbol (juce::Graphics& g, const juce::Point<float> centre, const float radius)
{
    // A speaker cone seen from the front, its middle pushed to one side.
    // -> documentation/editor-gui.md section 14
    const auto ring = [&] (const juce::Point<float> c, const float r)
    {
        g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), lineFor (radius));
    };

    ring (centre, radius);
    ring (centre.translated (radius * 0.25f, 0.0f), radius * 0.52f);
    g.fillEllipse (juce::Rectangle<float> (radius * 0.46f, radius * 0.46f).withCentre (centre.translated (radius * 0.44f, 0.0f)));
}
