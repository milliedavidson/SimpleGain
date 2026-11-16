// =============================================================================
//  WebsitePanel.cpp — the panel behind the ring mark, top left
//
//  Comments are short on purpose — the reasoning lives in documentation/.
// =============================================================================

#include "WebsitePanel.h"
#include "Brand.h"
#include "PluginEditor.h"       // SimpleGainLookAndFeel, for the fonts

WebsitePanel::WebsitePanel (const SimpleGainLookAndFeel& lf)
    : lookAndFeel (lf),
      // U+1F6A7 is the construction sign. No brand font has it, so JUCE falls back to
      // macOS's own emoji font for that one character. -> documentation/editor-gui.md section 16
      heading ("Under construction " + juce::String::charToString (0x1f6a7)),
      message ("The website will be linked here in a future update.")
{
}

void WebsitePanel::paint (juce::Graphics& g)
{
    // Opaque Base over the controls, and the two lines in the middle.
    g.fillAll (tokens::base);

    const auto bounds = getLocalBounds().toFloat();
    const auto height = bounds.getHeight();

    g.setColour (tokens::text);
    g.setFont (lookAndFeel.logoFont (height * 0.08f));
    g.drawText (heading, bounds.withHeight (height * 0.12f).withCentre (bounds.getCentre().translated (0.0f, -height * 0.05f)),
                juce::Justification::centred);

    g.setColour (tokens::muted);
    g.setFont (lookAndFeel.textFont (juce::jmax (12.0f, height * 0.045f)));
    g.drawText (message, bounds.withHeight (height * 0.08f).withCentre (bounds.getCentre().translated (0.0f, height * 0.06f)),
                juce::Justification::centred);
}
