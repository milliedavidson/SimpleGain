// =============================================================================
//  Guide.h — the quick-start guide behind the ? button
//
//  The text is guide/quick-start.md, compiled into the plugin, so the guide works
//  offline. GitHub shows the same file, so it's also the online version.
//  -> documentation/editor-gui.md section 15
// =============================================================================

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Fading.h"

#include <vector>

// Declared, not included: these classes only hold a reference to it, so the compiler
// doesn't need its insides here. Guide.cpp includes the real thing.
class SimpleGainLookAndFeel;


namespace guide
{
    /** One piece of the guide's text. */
    struct Block
    {
        enum class Kind { title, heading, paragraph, bullet };
        Kind kind;
        juce::String text;
    };

    /** Reads the small part of Markdown the guide uses: "# title", "## heading",
        paragraphs, and "- " bullets. Lines that carry on a paragraph or bullet are joined. */
    [[nodiscard]] std::vector<Block> parse (const juce::String& markdown);

    /** One block's text, ready to lay out: **bold** in the strong colour, `code` in the code
        font, and the marks themselves removed. */
    [[nodiscard]] juce::AttributedString style (const juce::String& text, const juce::Font& normal, const juce::Font& code,
                                                juce::Colour normalColour, juce::Colour strongColour);

    /** The quick-start guide's Markdown, compiled in from guide/quick-start.md. */
    [[nodiscard]] juce::String quickStart();
}


//==============================================================================
/** The guide's text laid out for a width, with a link to the full docs at the end. */
class GuidePage final : public juce::Component
{
public:
    explicit GuidePage (const SimpleGainLookAndFeel&);

    /** The body text's height in pixels. Headings are sized from it. */
    void setTextHeight (float);
    [[nodiscard]] float getTextHeight() const noexcept                  { return textHeight; }

    /** Lays the text out to fit `width`, and makes the page tall enough to hold it all. */
    void setWidthAndLayOut (int width);

    /** The guide's title, shown above the page rather than scrolling with it. */
    [[nodiscard]] juce::String getGuideTitle() const;

    [[nodiscard]] const juce::HyperlinkButton& getLink() const noexcept { return link; }

    void paint (juce::Graphics&) override;

private:
    struct LaidOut
    {
        juce::TextLayout layout;
        juce::Point<float> at;
        bool isBullet = false;
    };

    const SimpleGainLookAndFeel& lookAndFeel;
    std::vector<guide::Block> blocks;
    std::vector<LaidOut> laidOut;
    float textHeight = 14.0f;
    juce::HyperlinkButton link;
};


//==============================================================================
/** The quick-start guide: its title and a scrolling page, laid over the whole window,
    fading in and out. */
class GuideView final : public Fading<juce::Component>
{
public:
    explicit GuideView (const SimpleGainLookAndFeel&);

    [[nodiscard]] GuidePage& getPage() noexcept             { return page; }
    [[nodiscard]] juce::Viewport& getViewport() noexcept    { return viewport; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    const SimpleGainLookAndFeel& lookAndFeel;
    GuidePage page;             // before the viewport, so it outlives it: the viewport only borrows it
    juce::Viewport viewport;
};
