// =============================================================================
//  GuideTests.cpp — the quick-start guide behind the ? button
//
//  The guide is a page laid over the controls. Its text is a small Markdown file
//  compiled into the plugin, so these check the reader as well as the page: it
//  opens and closes, it can be read at every size, and it says the right things
//  in the right order. Why: documentation/editor-gui.md section 15
// =============================================================================
#include "TestSupport.h"
#include "PluginEditor.h"

namespace
{
    using namespace support;

    const juce::Colour base { 0xff121417 }, text { 0xffe8eaed }, apricot { 0xfff2a766 };

    bool isDrawnOn (const juce::Colour c)
    {
        return std::abs (c.getRed() - base.getRed()) > 2 || std::abs (c.getGreen() - base.getGreen()) > 2
            || std::abs (c.getBlue() - base.getBlue()) > 2;
    }

    /** The smallest box holding everything drawn in `area` of `image`. */
    juce::Rectangle<int> inkIn (const juce::Image& image, const juce::Rectangle<int> area)
    {
        int left = area.getRight(), right = area.getX() - 1, top = area.getBottom(), bottom = area.getY() - 1;
        for (int y = area.getY(); y < area.getBottom(); ++y)
            for (int x = area.getX(); x < area.getRight(); ++x)
                if (isDrawnOn (image.getPixelAt (x, y)))
                    left = juce::jmin (left, x), right = juce::jmax (right, x), top = juce::jmin (top, y), bottom = juce::jmax (bottom, y);

        return right < left ? juce::Rectangle<int>() : juce::Rectangle<int>::leftTopRightBottom (left, top, right + 1, bottom + 1);
    }

    float brightestIn (const juce::Image& image, const juce::Rectangle<int> area)
    {
        float brightest = 0.0f;
        for (int y = area.getY(); y < area.getBottom(); ++y)
            for (int x = area.getX(); x < area.getRight(); ++x)
                brightest = juce::jmax (brightest, image.getPixelAt (x, y).getBrightness());

        return brightest;
    }

    struct GuideTests final : juce::UnitTest
    {
        GuideTests() : UnitTest ("Guide", "SimpleGain") {}

        void runTest() override
        {
            readsTheMarkdownItNeeds();
            stylesBoldAndCode();
            saysTheRightThings();
            buttonTopRight();
            opensAndCloses();
            readableAtEverySize();
        }

        //======================================================================
        void readsTheMarkdownItNeeds()
        {
            beginTest ("The guide's reader understands a title, headings, paragraphs and bullets");

            const auto blocks = guide::parse ("# the title\n\n## A heading\nOne line\nand the next.\n\n"
                                              "- **First** bullet\n  carried on\n- Second\n\nLast paragraph.\n");
            using Kind = guide::Block::Kind;

            expectEquals (static_cast<int> (blocks.size()), 6);
            if (blocks.size() != 6)
                return;

            expect (blocks[0].kind == Kind::title && blocks[0].text == "the title", "the title");
            expect (blocks[1].kind == Kind::heading && blocks[1].text == "A heading", "the heading");
            expect (blocks[2].kind == Kind::paragraph && blocks[2].text == "One line and the next.", "lines of a paragraph joined");
            expect (blocks[3].kind == Kind::bullet && blocks[3].text == "**First** bullet carried on", "an indented line carries a bullet on");
            expect (blocks[4].kind == Kind::bullet && blocks[4].text == "Second", "a second bullet");
            expect (blocks[5].kind == Kind::paragraph && blocks[5].text == "Last paragraph.", "the last paragraph");
        }

        void stylesBoldAndCode()
        {
            beginTest ("**Bold** is drawn in apricot, `code` in the mono font, and the marks themselves are hidden");

            SimpleGainLookAndFeel lookAndFeel;
            const auto styled = guide::style ("a **b** `c`", juce::Font (lookAndFeel.textFont (14.0f)),
                                              juce::Font (lookAndFeel.monoFont (14.0f)), text, apricot);

            expectEquals (styled.getText(), juce::String ("a b c"));

            const auto attributeAt = [&styled] (const int index) -> const juce::AttributedString::Attribute*
            {
                for (int i = 0; i < styled.getNumAttributes(); ++i)
                    if (styled.getAttribute (i).range.contains (index))
                        return &styled.getAttribute (i);
                return nullptr;
            };

            const auto* plain = attributeAt (0);
            const auto* bold = attributeAt (2);
            const auto* code = attributeAt (4);
            if (plain == nullptr || bold == nullptr || code == nullptr)
                return expect (false, "some text has no style");

            expect (plain->colour == text, "plain text isn't the Text colour");
            expect (bold->colour == apricot, "bold text isn't apricot");
            expectEquals (code->font.getTypefacePtr() != nullptr ? code->font.getTypefacePtr()->getName() : juce::String(),
                          juce::String ("JetBrains Mono"), "code isn't in the mono font");
        }

        void saysTheRightThings()
        {
            beginTest ("The quick start begins like you're five, ends for the nerds, and covers every control");

            const auto blocks = guide::parse (guide::quickStart());
            if (blocks.empty())
                return expect (false, "the guide is empty");

            using Kind = guide::Block::Kind;
            expect (blocks.front().kind == Kind::title && blocks.front().text == "quick start", "it doesn't start with its title");

            int eli5 = -1, nerds = -1;
            juce::String everything;
            for (int i = 0; i < static_cast<int> (blocks.size()); ++i)
            {
                const auto& block = blocks[static_cast<size_t> (i)];
                everything << block.text << "\n";
                if (block.kind == Kind::heading && block.text.containsIgnoreCase ("five"))   eli5 = i;
                if (block.kind == Kind::heading && block.text.containsIgnoreCase ("nerds"))  nerds = i;
            }

            expect (eli5 > 0, "no \"like you're five\" section");
            expect (nerds > eli5, "the nerds' section isn't after the simple one");

            // Every way to use the knob is explained, so nobody has to discover them by accident.
            for (const auto* phrase : { "Turn the ring", "Drag the number", "Click the number", "Double-click the ring",
                                        "0 dB", "Approaching the limit", "20 ms", "latency" })
                expect (everything.contains (phrase), "the guide doesn't mention: " + juce::String (phrase));
        }

        //======================================================================
        void buttonTopRight()
        {
            beginTest ("The ? button sits top right, muted and round, as far from the right as the ring mark is from the left");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            auto* button = dynamic_cast<juce::Button*> (editor.findChildWithID ("guideButton"));
            if (button == nullptr)
                return expect (false, "no guide button");

            expectEquals (button->getTitle(), juce::String ("Guide"), "screen readers can't name the button");

            const auto image = editor.createComponentSnapshot (editor.getLocalBounds());
            const auto width = editor.getWidth(), height = editor.getHeight();
            const auto mark = inkIn (image, { 0, 0, width / 6, height / 5 });
            const auto question = inkIn (image, { width - width / 6, 0, width / 6, height / 5 });

            expect (! question.isEmpty(), "nothing drawn top right");
            expect (button->getBounds().contains (question.getCentre()), "the drawing isn't on the button");
            expectWithinAbsoluteError (question.getWidth(), question.getHeight(), 2, "the button isn't round");
            expectWithinAbsoluteError (question.getWidth(), mark.getWidth(), 2, "the button isn't the ring mark's size");
            expectWithinAbsoluteError (width - question.getRight(), mark.getX(), 2, "not as far from the right as the mark is from the left");
            expectWithinAbsoluteError (question.getCentreY(), mark.getCentreY(), 2, "not level with the ring mark");
            expectLessThan (brightestIn (image, question), 0.7f, "the button isn't muted");
        }

        void opensAndCloses()
        {
            beginTest ("Clicking ? fades the guide in over the whole window; clicking again fades it out");

            SimpleGainAudioProcessor processor;
            setGainDb (processor, -6.0f);
            SimpleGainAudioProcessorEditor editor (processor);
            editor.setVisible (true);       // JUCE only hit-tests visible components

            auto* button = dynamic_cast<juce::Button*> (editor.findChildWithID ("guideButton"));
            auto* page = dynamic_cast<GuideView*> (editor.findChildWithID ("guide"));
            auto* knob = editor.findChildWithID ("gainSlider");
            if (button == nullptr || page == nullptr || knob == nullptr)
                return expect (false, "no guide button, guide or knob");

            expect (! page->isVisible(), "the guide is showing when the window opens");
            expect (editor.getIndexOfChildComponent (button) > editor.getIndexOfChildComponent (page),
                    "the button is under the guide, so it can't close it");

            // Open: it fades in rather than snapping on.
            button->onClick();
            expect (button->getToggleState(), "the button doesn't show the guide is open");
            expectLessThan (page->getAlpha(), 0.2f, "the guide snapped on");
            page->advance (1.0);
            expect (page->isVisible() && page->getAlpha() == 1.0f, "the guide isn't showing after a second");
            expect (page->getBounds() == editor.getLocalBounds(), "the guide doesn't cover the whole window");

            // While it's open, the knob can't be turned by accident underneath it.
            expect (editor.getComponentAt (knob->getBounds().getCentre()) != knob, "the knob can still be clicked through the guide");

            // Close.
            button->onClick();
            expect (! button->getToggleState(), "the button still shows the guide as open");
            page->advance (1.0);
            expect (! page->isVisible(), "the guide is still showing after closing");
            expectWithinAbsoluteError (gainDb (processor), -6.0f, 0.01f, "opening the guide changed the gain");
        }

        void readableAtEverySize()
        {
            beginTest ("The guide's text never drops below 12 pixels, scrolls when it's long, and ends with a link to the docs");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            auto* page = dynamic_cast<GuideView*> (editor.findChildWithID ("guide"));
            const auto* limits = editor.getConstrainer();
            if (page == nullptr || limits == nullptr)
                return expect (false, "no guide or no size limits");

            page->showNow (true);

            editor.setSize (limits->getMinimumWidth(), limits->getMinimumHeight());
            const auto smallest = page->getPage().getTextHeight();
            expectGreaterOrEqual (smallest, 12.0f, "the text is too small to read at the smallest size");
            expectGreaterThan (page->getPage().getHeight(), page->getViewport().getHeight(), "the guide fits without scrolling: is the text missing?");
            expect (page->getViewport().isVerticalScrollBarShown(), "no scroll bar on a page that scrolls");

            editor.setSize (limits->getMaximumWidth(), limits->getMaximumHeight());
            expectGreaterThan (page->getPage().getTextHeight(), smallest * 1.5f, "the text doesn't grow with the window");

            // The link sits below all the text, on the page.
            const auto& link = page->getPage().getLink();
            expect (link.getURL().toString (false).startsWith ("https://github.com/milliedavidson/simply-gain"), "the link doesn't go to the docs");
            expect (page->getPage().getLocalBounds().contains (link.getBounds()), "the link is off the page");
            expectGreaterThan (link.getY(), page->getPage().getHeight() * 3 / 4, "the link isn't at the end");

            // And it really draws something.
            const auto image = page->getPage().createComponentSnapshot (page->getPage().getLocalBounds());
            expect (! inkIn (image, image.getBounds()).isEmpty(), "the guide's page is blank");
        }
    };

    // Never read: constructing it registers it with JUCE's test runner.
    [[maybe_unused]] GuideTests guideTests;
}
