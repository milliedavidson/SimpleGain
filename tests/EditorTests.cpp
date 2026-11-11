// =============================================================================
//  EditorTests.cpp — Phase 6: the GUI
//
//  No display is needed: JUCE components can be created, laid out and painted into
//  an image without a window. These check the parts that matter — the knob is
//  bound to the parameter in BOTH directions, the layout stays inside the window,
//  and the editor can be opened and closed repeatedly without disturbing anything.
//  Why each test exists: documentation/editor-gui.md
// =============================================================================
#include "TestSupport.h"
#include "PluginEditor.h"

#include <memory>

namespace
{
    using namespace support;

    constexpr double tolerance = 0.01;

    /** The editor's gain knob, found by the ID the editor gives it (so tests need no access to its internals). */
    juce::Slider* findKnob (juce::Component& editor)
    {
        return dynamic_cast<juce::Slider*> (editor.findChildWithID ("gainSlider"));
    }

    struct EditorTests final : public juce::UnitTest
    {
        EditorTests() : juce::UnitTest ("Editor", "SimpleGain") {}

        void runTest() override
        {
            ownEditor();
            sizeAndLimits();
            knobStartsAtTheFader();
            turningTheKnob();
            hostAutomation();
            textBox();
            rangeAndReset();
            layoutAtEverySize();
            drawsSomething();
            openingAndClosing();
        }

        //======================================================================
        void ownEditor()
        {
            beginTest ("The plugin offers its own editor, not JUCE's generic one");

            SimpleGainAudioProcessor processor;
            expect (processor.hasEditor());

            std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            expect (dynamic_cast<SimpleGainAudioProcessorEditor*> (editor.get()) != nullptr,
                    "createEditor() returned something other than SimpleGainAudioProcessorEditor");
        }

        void sizeAndLimits()
        {
            beginTest ("The editor opens inside its resize limits, with a fixed aspect ratio");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);

            const auto* constrainer = editor.getConstrainer();
            expect (constrainer != nullptr, "the editor isn't resizable");
            if (constrainer == nullptr)
                return;

            expectGreaterThan (editor.getWidth(), 0);
            expectGreaterThan (editor.getHeight(), 0);
            expect (editor.getWidth() >= constrainer->getMinimumWidth() && editor.getWidth() <= constrainer->getMaximumWidth());
            expect (editor.getHeight() >= constrainer->getMinimumHeight() && editor.getHeight() <= constrainer->getMaximumHeight());

            const auto openedAspect = static_cast<double> (editor.getWidth()) / editor.getHeight();
            expectWithinAbsoluteError (constrainer->getFixedAspectRatio(), openedAspect, 0.01);
        }

        //======================================================================
        void knobStartsAtTheFader()
        {
            beginTest ("The knob starts at the fader's position, not at the default");

            SimpleGainAudioProcessor processor;
            setGainDb (processor, -6.0f);

            SimpleGainAudioProcessorEditor editor (processor);
            auto* knob = findKnob (editor);
            expect (knob != nullptr, "no knob with the ID \"gainSlider\"");
            if (knob != nullptr)
                expectWithinAbsoluteError (knob->getValue(), -6.0, tolerance);
        }

        void turningTheKnob()
        {
            beginTest ("Turning the knob moves the parameter");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            auto* knob = findKnob (editor);
            if (knob == nullptr)
                return expect (false, "no knob");

            for (const double db : { -60.0, -12.3, 0.0, 7.7, 30.0 })
            {
                knob->setValue (db, juce::sendNotificationSync);
                expectWithinAbsoluteError (static_cast<double> (gainDb (processor)), db, tolerance,
                                           "knob set to " + juce::String (db));
            }
        }

        void hostAutomation()
        {
            beginTest ("When the host moves the parameter, the knob follows");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            auto* knob = findKnob (editor);
            if (knob == nullptr)
                return expect (false, "no knob");

            for (const float db : { -60.0f, -12.3f, 0.0f, 7.7f, 30.0f })
            {
                setGainDb (processor, db);                     // what automation playback does
                expectWithinAbsoluteError (knob->getValue(), static_cast<double> (db), tolerance,
                                           "parameter set to " + juce::String (db));
            }
        }

        void textBox()
        {
            beginTest ("The text box shows what the host shows, and accepts what a person types");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            auto* knob = findKnob (editor);
            if (knob == nullptr)
                return expect (false, "no knob");

            expectEquals (knob->getTextFromValue (-6.0), juce::String ("-6.0 dB"));
            expectEquals (knob->getTextFromValue (0.0), juce::String ("0.0 dB"));
            expectWithinAbsoluteError (knob->getValueFromText ("-12"), -12.0, tolerance);
            expectWithinAbsoluteError (knob->getValueFromText ("-12 dB"), -12.0, tolerance);
            expectWithinAbsoluteError (knob->getValueFromText ("+4.5"), 4.5, tolerance);
        }

        void rangeAndReset()
        {
            beginTest ("The knob covers the parameter's range, and double-click resets to 0 dB");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            auto* knob = findKnob (editor);
            if (knob == nullptr)
                return expect (false, "no knob");

            expectWithinAbsoluteError (knob->getMinimum(), -60.0, tolerance);
            expectWithinAbsoluteError (knob->getMaximum(), 30.0, tolerance);
            expect (knob->isDoubleClickReturnEnabled(), "double-click reset is off");
            expectWithinAbsoluteError (knob->getDoubleClickReturnValue(), 0.0, tolerance);
        }

        //======================================================================
        void layoutAtEverySize()
        {
            beginTest ("Everything stays inside the window, at every size it can be resized to");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            auto* knob = findKnob (editor);
            const auto* constrainer = editor.getConstrainer();
            if (knob == nullptr || constrainer == nullptr)
                return expect (false, "no knob or no constrainer");

            const auto defaultSize = editor.getBounds();

            for (const auto [width, height] : { std::pair { constrainer->getMinimumWidth(), constrainer->getMinimumHeight() },
                                                std::pair { defaultSize.getWidth(), defaultSize.getHeight() },
                                                std::pair { constrainer->getMaximumWidth(), constrainer->getMaximumHeight() } })
            {
                editor.setSize (width, height);
                const auto label = juce::String (width) + " x " + juce::String (height);

                expect (editor.getLocalBounds().contains (knob->getBounds()), "knob outside the window at " + label);
                expectGreaterThan (knob->getWidth(), 80, "knob too small at " + label);
                expectGreaterThan (knob->getHeight(), 80, "knob too small at " + label);

                for (auto* child : editor.getChildren())
                    expect (editor.getLocalBounds().contains (child->getBounds()), "a child sticks out at " + label);
            }
        }

        void drawsSomething()
        {
            beginTest ("It actually draws: a gradient background, and a knob that is visible and changes the picture");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            auto* knob = findKnob (editor);
            if (knob == nullptr)
                return expect (false, "no knob");

            const auto withKnob = editor.createComponentSnapshot (editor.getLocalBounds());
            expect (withKnob.isValid());
            expect (knob->isVisible(), "the knob is in the window but not visible");

            const auto topLeft    = withKnob.getPixelAt (2, 2);
            const auto bottomLeft = withKnob.getPixelAt (2, withKnob.getHeight() - 3);
            expect (topLeft != bottomLeft, "no vertical gradient in the background");

            // The gradient alone would make the middle differ from a corner, so compare against the same
            // window with the knob hidden: the knob's own area must change.
            knob->setVisible (false);
            const auto withoutKnob = editor.createComponentSnapshot (editor.getLocalBounds());

            int changedPixels = 0;
            for (int y = 0; y < withKnob.getHeight(); ++y)
                for (int x = 0; x < withKnob.getWidth(); ++x)
                    if (withKnob.getPixelAt (x, y) != withoutKnob.getPixelAt (x, y))
                        ++changedPixels;

            expectGreaterThan (changedPixels, 1000, "hiding the knob barely changes the picture: it isn't being drawn");
        }

        //======================================================================
        void openingAndClosing()
        {
            beginTest ("The editor can be opened and closed repeatedly without disturbing the gain");

            SimpleGainAudioProcessor processor;
            setGainDb (processor, -9.0f);

            for (int i = 0; i < 50; ++i)
            {
                SimpleGainAudioProcessorEditor editor (processor);
                auto* knob = findKnob (editor);
                if (knob == nullptr)
                    return expect (false, "no knob");

                expectWithinAbsoluteError (knob->getValue(), -9.0, tolerance, "open number " + juce::String (i));
            }

            // The processor outlives its editors. Moving the parameter now, with no editor open, must be harmless:
            // the closed editors must have disconnected themselves.
            setGainDb (processor, 12.0f);
            expectWithinAbsoluteError (static_cast<double> (gainDb (processor)), 12.0, tolerance);

            SimpleGainAudioProcessorEditor reopened (processor);
            if (auto* knob = findKnob (reopened))
                expectWithinAbsoluteError (knob->getValue(), 12.0, tolerance, "reopened after the parameter moved");
        }
    };

    EditorTests editorTests;
}
