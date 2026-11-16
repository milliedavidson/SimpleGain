// =============================================================================
//  WebsitePanelTests.cpp — the panel behind the ring mark, top left
//
//  The ring mark is a button. For now it opens a panel saying the website is
//  under construction. Only one panel is ever open: the mark's or the guide's.
//  Why: documentation/editor-gui.md section 16
// =============================================================================
#include "TestSupport.h"
#include "PluginEditor.h"

namespace
{
    using namespace support;

    struct WebsitePanelTests final : juce::UnitTest
    {
        WebsitePanelTests() : UnitTest ("Website panel", "SimpleGain") {}

        void runTest() override
        {
            theMarkIsAButton();
            opensAndCloses();
            saysUnderConstruction();
            onlyOnePanelAtATime();
        }

        struct Parts
        {
            juce::Button* mark = nullptr;
            juce::Button* question = nullptr;
            WebsitePanel* website = nullptr;
            GuideView* guide = nullptr;

            explicit Parts (const juce::Component& editor)
                : mark (dynamic_cast<juce::Button*> (editor.findChildWithID ("markButton"))),
                  question (dynamic_cast<juce::Button*> (editor.findChildWithID ("guideButton"))),
                  website (dynamic_cast<WebsitePanel*> (editor.findChildWithID ("websitePanel"))),
                  guide (dynamic_cast<GuideView*> (editor.findChildWithID ("guide")))
            {
            }

            [[nodiscard]] bool complete() const { return mark != nullptr && question != nullptr && website != nullptr && guide != nullptr; }
        };

        //======================================================================
        void theMarkIsAButton()
        {
            beginTest ("The ring mark, top left, is a button with a name screen readers can say");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            const Parts parts (editor);
            if (! parts.complete())
                return expect (false, "no mark button, guide button, website panel or guide");

            expect (parts.mark->getTitle().containsIgnoreCase ("website"), "screen readers can't name the mark button");
            expect (parts.mark->getBounds().getCentreX() < editor.getWidth() / 6, "the mark isn't on the left");
            expect (parts.mark->getBounds().getCentreY() < editor.getHeight() / 5, "the mark isn't at the top");
            expectWithinAbsoluteError (parts.mark->getBounds().getCentreY(), parts.question->getBounds().getCentreY(), 1,
                                       "the mark isn't level with the ? button");
            expectWithinAbsoluteError (parts.mark->getWidth(), parts.question->getWidth(), 1, "the mark isn't the ? button's size");
        }

        void opensAndCloses()
        {
            beginTest ("Clicking the mark fades the panel in over the whole window; clicking again fades it out");

            SimpleGainAudioProcessor processor;
            setGainDb (processor, -6.0f);
            SimpleGainAudioProcessorEditor editor (processor);
            editor.setVisible (true);       // JUCE only hit-tests visible components

            const Parts parts (editor);
            auto* knob = editor.findChildWithID ("gainSlider");
            if (! parts.complete() || knob == nullptr)
                return expect (false, "something's missing");

            expect (! parts.website->isVisible(), "the panel is showing when the window opens");
            expect (editor.getIndexOfChildComponent (parts.mark) > editor.getIndexOfChildComponent (parts.website),
                    "the mark is under the panel, so it can't close it");

            parts.mark->onClick();
            expect (parts.mark->getToggleState(), "the mark doesn't show the panel is open");
            expectLessThan (parts.website->getAlpha(), 0.2f, "the panel snapped on");
            parts.website->advance (1.0);
            expect (parts.website->isVisible() && parts.website->getAlpha() == 1.0f, "the panel isn't showing after a second");
            expect (parts.website->getBounds() == editor.getLocalBounds(), "the panel doesn't cover the whole window");
            expect (editor.getComponentAt (knob->getBounds().getCentre()) != knob, "the knob can still be clicked through the panel");

            parts.mark->onClick();
            expect (! parts.mark->getToggleState(), "the mark still shows the panel as open");
            parts.website->advance (1.0);
            expect (! parts.website->isVisible(), "the panel is still showing after closing");
            expectWithinAbsoluteError (gainDb (processor), -6.0f, 0.01f, "opening the panel changed the gain");
        }

        void saysUnderConstruction()
        {
            beginTest ("The panel says it's under construction, with the emoji, and that the website will be linked here");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            const Parts parts (editor);
            if (! parts.complete())
                return expect (false, "something's missing");

            expect (parts.website->getHeading().startsWith ("Under construction"), "the heading doesn't say it's under construction");
            expect (parts.website->getHeading().contains (juce::String::charToString (0x1f6a7)), "no construction emoji");
            expect (parts.website->getMessage().containsIgnoreCase ("website will be linked here in a future update"),
                    "the panel doesn't say the website is coming");

            // Both are really drawn, in the middle of the window: the heading near-white just above the
            // middle, the message muted grey just below it. Each is looked for only in its own band.
            parts.website->showNow (true);
            const auto image = parts.website->createComponentSnapshot (parts.website->getLocalBounds());
            const auto height = image.getHeight();
            int headingPixels = 0, messagePixels = 0;
            for (int y = height * 36 / 100; y < height * 62 / 100; ++y)
                for (int x = 0; x < image.getWidth(); ++x)
                {
                    const auto c = image.getPixelAt (x, y);
                    const auto grey = c.getSaturation() < 0.15f;
                    if (y < height / 2 && grey && c.getBrightness() > 0.85f)                                  ++headingPixels;
                    if (y >= height / 2 && grey && c.getBrightness() > 0.5f && c.getBrightness() < 0.7f)     ++messagePixels;
                }
            expectGreaterThan (headingPixels, 200, "the heading isn't drawn in the middle");
            expectGreaterThan (messagePixels, 100, "the message isn't drawn in the middle");
        }

        void onlyOnePanelAtATime()
        {
            beginTest ("Opening one panel closes the other: the mark's or the guide's, never both");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            const Parts parts (editor);
            if (! parts.complete())
                return expect (false, "something's missing");

            parts.question->onClick();
            parts.guide->advance (1.0);
            parts.mark->onClick();
            parts.guide->advance (1.0);
            parts.website->advance (1.0);
            expect (parts.website->isVisible() && ! parts.guide->isVisible(), "opening the mark's panel didn't close the guide");
            expect (parts.mark->getToggleState() && ! parts.question->getToggleState(), "the buttons don't show which panel is open");

            parts.question->onClick();
            parts.guide->advance (1.0);
            parts.website->advance (1.0);
            expect (parts.guide->isVisible() && ! parts.website->isVisible(), "opening the guide didn't close the mark's panel");
            expect (parts.question->getToggleState() && ! parts.mark->getToggleState(), "the buttons don't show which panel is open");
        }
    };

    // Never read: constructing it registers it with JUCE's test runner.
    [[maybe_unused]] WebsitePanelTests websitePanelTests;
}
