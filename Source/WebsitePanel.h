// =============================================================================
//  WebsitePanel.h — the panel behind the ring mark, top left
//
//  A placeholder for now: it will link to the website in a future update.
//  -> documentation/editor-gui.md section 16
// =============================================================================

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Fading.h"

class SimpleGainLookAndFeel;


//==============================================================================
/** A panel over the whole window, fading in and out, saying the website is on its way. */
class WebsitePanel final : public Fading<juce::Component>
{
public:
    explicit WebsitePanel (const SimpleGainLookAndFeel&);

    [[nodiscard]] const juce::String& getHeading() const noexcept   { return heading; }
    [[nodiscard]] const juce::String& getMessage() const noexcept   { return message; }

    void paint (juce::Graphics&) override;

private:
    const SimpleGainLookAndFeel& lookAndFeel;
    const juce::String heading, message;
};
