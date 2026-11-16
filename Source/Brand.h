// =============================================================================
//  Brand.h — the brand plan's colours, shared by everything that draws
// =============================================================================

#pragma once

#include <juce_graphics/juce_graphics.h>

// The plugin family's shared colours, plus this plugin's own. Amber means one thing
// only: approaching the limit. `inline` lets every .cpp file include them and still
// share one copy. -> documentation/editor-gui.md section 13
namespace tokens
{
    inline const juce::Colour base    { 0xff121417 };     // the background
    inline const juce::Colour surface { 0xff1b1e22 };     // raised things: the value box while typing
    inline const juce::Colour line    { 0xff2a2e34 };     // the ring's track
    inline const juce::Colour text    { 0xffe8eaed };     // the value, and the guide's text
    inline const juce::Colour muted   { 0xff8a9099 };     // the name, the marks, the caption and the logo
    inline const juce::Colour apricot { 0xfff2a766 };     // Simply Gain's accent, used by no paid plugin
    inline const juce::Colour quiet   { 0xfff8d3b0 };     // a lighter apricot for the quiet end of the arc
    inline const juce::Colour amber   { 0xffff8a1f };     // Volcanic Amber
    inline const juce::Colour ember   { 0xffd93a1e };     // a deep red-orange for the last stretch
}
