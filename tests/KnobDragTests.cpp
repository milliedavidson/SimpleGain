// =============================================================================
//  KnobDragTests.cpp — Phase 6: dragging the knob with the mouse
//
//  These feed the knob fake mouse events, the same calls JUCE makes when you
//  click, drag and scroll. The knob should follow the pointer round its arc, and
//  the number in the middle moves up and down like a fine slider. But a click
//  on its own must never change the gain — with +30 dB at one end of the arc, a
//  stray click jumping there would be a nasty surprise.
//  Why: documentation/editor-gui.md
// =============================================================================
#include "TestSupport.h"
#include "PluginEditor.h"

#include <cmath>
#include <vector>

namespace
{
    using namespace support;

    constexpr double pi = juce::MathConstants<double>::pi;
    constexpr double oneDegree = pi / 180.0;
    constexpr double dragTolerance = 0.15;      // dB; the parameter snaps to 0.1 dB steps

    juce::Slider* findKnob (const juce::Component& editor)
    {
        return dynamic_cast<juce::Slider*> (editor.findChildWithID ("gainSlider"));
    }

    /** Where the knob's arc is, worked out the same way JUCE does it. Angles start at 12 o'clock
        and go clockwise; the arc runs from startAngle (the minimum) to endAngle (the maximum). */
    struct Arc
    {
        juce::Point<float> centre;
        float radius;
        double startAngle, endAngle;

        explicit Arc (juce::Slider& knob)
        {
            const auto bounds = knob.getLookAndFeel().getSliderLayout (knob).sliderBounds;
            const auto rotary = knob.getRotaryParameters();
            centre = bounds.getCentre().toFloat();
            radius = static_cast<float> (juce::jmin (bounds.getWidth(), bounds.getHeight())) * 0.4f;
            startAngle = rotary.startAngleRadians;
            endAngle = rotary.endAngleRadians;
        }

        double angleOf (juce::Slider& knob, double db) const
        {
            return startAngle + (endAngle - startAngle) * knob.valueToProportionOfLength (db);
        }

        [[nodiscard]] juce::Point<float> pointAt (double angle, float distance) const
        {
            return centre + juce::Point<float> { distance * static_cast<float> (std::sin (angle)),
                                                -distance * static_cast<float> (std::cos (angle)) };
        }

        [[nodiscard]] juce::Point<float> pointAt (double angle) const   { return pointAt (angle, radius); }
    };

    /** Sends a knob the mouse events a real click-and-drag would. */
    struct FakeMouse
    {
        juce::Slider& knob;
        juce::Point<float> downAt;

        [[nodiscard]] juce::MouseEvent event (juce::Point<float> at, bool dragged) const
        {
            const auto now = juce::Time::getCurrentTime();
            return { juce::Desktop::getInstance().getMainMouseSource(), at,
                     juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
                     juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation,
                     juce::MouseInputSource::defaultRotation, juce::MouseInputSource::defaultTiltX,
                     juce::MouseInputSource::defaultTiltY, &knob, &knob, now, downAt, now, 1, dragged };
        }

        void press (juce::Point<float> at)     { downAt = at; knob.mouseDown (event (at, false)); }
        void moveTo (juce::Point<float> at) const    { knob.mouseDrag (event (at, true)); }
        void release (juce::Point<float> at) const   { knob.mouseUp (event (at, true)); }

        /** Press and let go without moving: a plain click. */
        void click (juce::Point<float> at)     { press (at); knob.mouseUp (event (at, false)); }

        /** Drags round the arc one degree at a time, as a hand would, recording the gain after each step. */
        std::vector<float> dragAround (const Arc& arc, double fromAngle, double toAngle,
                                       SimpleGainAudioProcessor& processor) const
        {
            std::vector<float> gains;
            const auto steps = static_cast<int> (std::ceil (std::abs (toAngle - fromAngle) / oneDegree));

            for (int i = 1; i <= steps; ++i)
            {
                moveTo (arc.pointAt (fromAngle + (toAngle - fromAngle) * i / steps));
                gains.push_back (gainDb (processor));
            }

            return gains;
        }
    };

    /** A point on the number in the middle, a little left of its centre (well away from the knob's centre). */
    juce::Point<float> onTheNumber (juce::Slider& knob)
    {
        const auto box = knob.getLookAndFeel().getSliderLayout (knob).textBoxBounds.toFloat();
        return box.getCentre().translated (-box.getWidth() / 4.0f, 0.0f);
    }

    /** Counts the "the user is touching this control" messages a host uses to record automation. */
    struct GestureCounter final : juce::AudioProcessorParameter::Listener
    {
        int begins = 0, ends = 0;
        void parameterValueChanged (int, float) override {}
        void parameterGestureChanged (int, bool starting) override  { ++(starting ? begins : ends); }
    };

    struct KnobDragTests final : juce::UnitTest
    {
        KnobDragTests() : UnitTest ("Knob dragging", "SimpleGain") {}

        void runTest() override
        {
            clickingNeverJumps();
            followsThePointer();
            grabAnywhere();
            stopsAtTheEnds();
            oneGesturePerDrag();
            theNumberIsPartOfTheKnob();
            dragTheNumberUpAndDown();
            scrollOnTheNumber();
            theCursorShowsUpAndDown();
        }

        //======================================================================
        void clickingNeverJumps()
        {
            beginTest ("Clicking anywhere on the knob, without dragging, leaves the gain alone");

            for (const float startDb : { 0.0f, -12.3f })
            {
                SimpleGainAudioProcessor processor;
                setGainDb (processor, startDb);
                SimpleGainAudioProcessorEditor editor (processor);
                auto* knob = findKnob (editor);
                if (knob == nullptr)
                    return expect (false, "no knob");

                const Arc arc (*knob);
                FakeMouse mouse { *knob, {} };

                // The two ends of the arc, the top, the gap at the bottom, and dead centre.
                for (const auto at : { arc.pointAt (arc.endAngle), arc.pointAt (arc.startAngle), arc.pointAt (2.0 * pi),
                                       arc.pointAt (pi), arc.centre })
                {
                    mouse.press (at);
                    mouse.release (at);
                    expectWithinAbsoluteError (gainDb (processor), startDb, 0.01f,
                                               "a click at " + at.toString() + " moved the gain from " + juce::String (startDb));
                }
            }
        }

        void followsThePointer()
        {
            beginTest ("Dragging the dot round the arc moves the gain with the pointer, both ways");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            auto* knob = findKnob (editor);
            if (knob == nullptr)
                return expect (false, "no knob");

            const Arc arc (*knob);
            FakeMouse mouse { *knob, {} };
            const auto unity = arc.angleOf (*knob, 0.0);

            mouse.press (arc.pointAt (unity));
            mouse.dragAround (arc, unity, arc.angleOf (*knob, 15.0), processor);
            expectWithinAbsoluteError (static_cast<double> (gainDb (processor)), 15.0, dragTolerance, "clockwise to +15 dB");

            mouse.dragAround (arc, arc.angleOf (*knob, 15.0), arc.angleOf (*knob, -30.0), processor);
            expectWithinAbsoluteError (static_cast<double> (gainDb (processor)), -30.0, dragTolerance, "back anticlockwise to -30 dB");
            mouse.release (arc.pointAt (arc.angleOf (*knob, -30.0)));
        }

        void grabAnywhere()
        {
            beginTest ("Grabbing away from the dot and turning moves the gain by the angle turned, not to the pointer");

            SimpleGainAudioProcessor processor;
            setGainDb (processor, -12.3f);
            SimpleGainAudioProcessorEditor editor (processor);
            auto* knob = findKnob (editor);
            if (knob == nullptr)
                return expect (false, "no knob");

            const Arc arc (*knob);
            FakeMouse mouse { *knob, {} };

            // Grab a quarter turn away from the dot, a little inside the ring, then turn by the angle
            // between -12.3 dB and 0 dB. (Not on the number: dragging that is a fine slider.)
            const auto grab = arc.angleOf (*knob, -12.3) + pi / 2.0;
            const auto turn = arc.angleOf (*knob, 0.0) - arc.angleOf (*knob, -12.3);

            mouse.press (arc.pointAt (grab, arc.radius * 0.9f));
            expectWithinAbsoluteError (gainDb (processor), -12.3f, 0.01f, "the grab itself moved the gain");

            for (int i = 1; i <= 90; ++i)
                mouse.moveTo (arc.pointAt (grab + turn * i / 90, arc.radius * 0.9f));

            expectWithinAbsoluteError (static_cast<double> (gainDb (processor)), 0.0, dragTolerance, "turned by the angle from -12.3 to 0 dB");
            mouse.release (arc.pointAt (grab + turn, arc.radius * 0.9f));
        }

        void stopsAtTheEnds()
        {
            beginTest ("Turning past either end stops there, and never wraps round to the other end");

            for (const auto [startDb, direction, endDb] : { std::tuple { 20.0f, 1.0, 30.0f },
                                                            std::tuple { -50.0f, -1.0, -60.0f } })
            {
                SimpleGainAudioProcessor processor;
                setGainDb (processor, startDb);
                SimpleGainAudioProcessorEditor editor (processor);
                auto* knob = findKnob (editor);
                if (knob == nullptr)
                    return expect (false, "no knob");

                const Arc arc (*knob);
                FakeMouse mouse { *knob, {} };
                const auto from = arc.angleOf (*knob, startDb);

                // Keep turning for 3/4 of a circle: past the end and across the gap at the bottom.
                const auto overshoot = from + direction * 1.5 * pi;
                mouse.press (arc.pointAt (from));
                const auto gains = mouse.dragAround (arc, from, overshoot, processor);

                const auto label = juce::String (startDb) + " dB turned " + (direction > 0 ? "clockwise" : "anticlockwise");
                expectWithinAbsoluteError (gains.back(), endDb, 0.01f, label + " didn't stop at the end");

                bool everWentBackwards = false;
                for (size_t i = 1; i < gains.size(); ++i)
                    everWentBackwards |= (gains[i] - gains[i - 1]) * static_cast<float> (direction) < -0.01f;
                expect (! everWentBackwards, label + ": the gain jumped the wrong way (wrapped round?)");

                // Turning all the way back, without letting go, lands where the grab started:
                // the dot waited at the end until the pointer came back to it.
                mouse.dragAround (arc, overshoot, from, processor);
                mouse.release (arc.pointAt (from));
                expectWithinAbsoluteError (gainDb (processor), startDb, 0.15f, label + " and back didn't return to the start");
            }
        }

        void oneGesturePerDrag()
        {
            beginTest ("A drag is one gesture to the host, so automation records it as one move");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            auto* knob = findKnob (editor);
            auto* parameter = processor.apvts.getParameter ("gain");
            if (knob == nullptr || parameter == nullptr)
                return expect (false, "no knob or no parameter");

            GestureCounter gestures;
            parameter->addListener (&gestures);

            const Arc arc (*knob);
            FakeMouse mouse { *knob, {} };
            const auto unity = arc.angleOf (*knob, 0.0);

            mouse.press (arc.pointAt (unity));
            mouse.dragAround (arc, unity, unity + pi / 4.0, processor);
            mouse.release (arc.pointAt (unity + pi / 4.0));

            parameter->removeListener (&gestures);

            expectEquals (gestures.begins, 1, "gesture starts");
            expectEquals (gestures.ends, 1, "gesture ends");
            expectGreaterThan (gainDb (processor), 1.0f, "the drag didn't move the gain at all");
        }

        void theNumberIsPartOfTheKnob()
        {
            beginTest ("The number in the middle is part of the knob: clicks reach the knob, and a plain click opens it for typing");

            SimpleGainAudioProcessor processor;
            setGainDb (processor, -12.3f);
            SimpleGainAudioProcessorEditor editor (processor);
            auto* knob = findKnob (editor);
            if (knob == nullptr)
                return expect (false, "no knob");

            // A real click on the number, found the way JUCE finds it, reaches the knob, not the label.
            const auto grab = onTheNumber (*knob);
            editor.setVisible (true);       // JUCE only hit-tests visible components; no window is needed
            expect (editor.getComponentAt (grab.toInt() + knob->getPosition()) == knob, "a click on the number doesn't reach the knob");

            // Opening the box for typing grabs the keyboard, which needs a real window on screen,
            // so count the requests instead of opening it.
            auto* relative = dynamic_cast<RelativeRotarySlider*> (knob);
            if (relative == nullptr)
                return expect (false, "the knob isn't a RelativeRotarySlider");

            expect (relative->onNumberClicked != nullptr, "a click on the number does nothing");
            int openRequests = 0;
            relative->onNumberClicked = [&openRequests] { ++openRequests; };

            // A click on the number, without moving, opens it for typing and leaves the gain alone.
            FakeMouse mouse { *knob, {} };
            mouse.click (grab);
            expectWithinAbsoluteError (gainDb (processor), -12.3f, 0.01f, "clicking the number moved the gain");
            expectEquals (openRequests, 1, "clicking the number didn't open it for typing");

            // A drag that starts on the number, or a click on the ring, doesn't.
            const Arc arc (*knob);
            mouse.press (grab);
            mouse.moveTo (grab.translated (0.0f, -20.0f));
            mouse.release (grab.translated (0.0f, -20.0f));
            mouse.click (arc.pointAt (arc.angleOf (*knob, -30.0)));
            expectEquals (openRequests, 1, "a drag from the number, or a click on the ring, opened it for typing");
        }

        void dragTheNumberUpAndDown()
        {
            beginTest ("Dragging the number up or down fine-tunes the gain, 0.05 dB a pixel, and sideways does nothing");

            SimpleGainAudioProcessor processor;
            setGainDb (processor, -12.3f);
            SimpleGainAudioProcessorEditor editor (processor);
            auto* knob = findKnob (editor);
            auto* parameter = processor.apvts.getParameter ("gain");
            if (knob == nullptr || parameter == nullptr)
                return expect (false, "no knob or no parameter");

            GestureCounter gestures;
            parameter->addListener (&gestures);

            FakeMouse mouse { *knob, {} };
            const auto grab = onTheNumber (*knob);
            mouse.press (grab);
            expectWithinAbsoluteError (gainDb (processor), -12.3f, 0.01f, "the grab itself moved the gain");

            // Up is louder: 100 pixels is 5 dB, whatever the window's size. A pixel at a time, as a hand would.
            for (int i = 1; i <= 100; ++i)
                mouse.moveTo (grab.translated (0.0f, static_cast<float> (-i)));
            expectWithinAbsoluteError (gainDb (processor), -7.3f, 0.06f, "100 pixels up from -12.3 dB");

            // Sideways, even right across the knob, leaves it alone: it never turns the ring.
            for (int i = 1; i <= 80; ++i)
                mouse.moveTo (grab.translated (static_cast<float> (i), -100.0f));
            expectWithinAbsoluteError (gainDb (processor), -7.3f, 0.06f, "moving sideways changed the gain");

            // Down past where it started is quieter.
            for (int i = -100; i <= 100; ++i)
                mouse.moveTo (grab.translated (80.0f, static_cast<float> (i)));
            expectWithinAbsoluteError (gainDb (processor), -17.3f, 0.06f, "100 pixels down from -12.3 dB");
            mouse.release (grab.translated (80.0f, 100.0f));

            parameter->removeListener (&gestures);
            expectEquals (gestures.begins, 1, "gesture starts");
            expectEquals (gestures.ends, 1, "gesture ends");

            // It stops at the ends, like the ring.
            setGainDb (processor, 28.0f);
            mouse.press (grab);
            for (int i = 1; i <= 200; ++i)
                mouse.moveTo (grab.translated (0.0f, static_cast<float> (-i)));
            mouse.release (grab.translated (0.0f, -200.0f));
            expectWithinAbsoluteError (gainDb (processor), 30.0f, 0.01f, "dragging up past +30 dB");
        }

        void scrollOnTheNumber()
        {
            beginTest ("Scrolling over the number nudges the gain in small steps, up and down");

            SimpleGainAudioProcessor processor;
            setGainDb (processor, -12.3f);
            SimpleGainAudioProcessorEditor editor (processor);
            auto* knob = findKnob (editor);
            if (knob == nullptr)
                return expect (false, "no knob");

            // A trackpad reports 1/512 of JUCE's wheel unit for each pixel scrolled.
            const auto at = onTheNumber (*knob);
            auto time = juce::Time::getCurrentTime();
            const auto scroll = [&] (float pixels)
            {
                time += juce::RelativeTime::milliseconds (10);      // JUCE drops a repeat with the same time
                juce::MouseWheelDetails wheel {};
                wheel.deltaY = pixels / 512.0f;
                wheel.isSmooth = true;
                knob->mouseWheelMove (juce::MouseEvent { juce::Desktop::getInstance().getMainMouseSource(), at, {},
                                                         juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation,
                                                         juce::MouseInputSource::defaultRotation, juce::MouseInputSource::defaultTiltX,
                                                         juce::MouseInputSource::defaultTiltY, knob, knob, time, at, time, 0, false },
                                      wheel);
            };

            scroll (40.0f);
            const auto afterUp = gainDb (processor);
            expect (afterUp > -12.25f && afterUp < -10.3f, "40 pixels up from -12.3 dB gave " + juce::String (afterUp));

            scroll (-40.0f);
            expectWithinAbsoluteError (gainDb (processor), -12.3f, 0.11f, "and 40 pixels back down");
        }

        void theCursorShowsUpAndDown()
        {
            beginTest ("Over the number the pointer shows up-and-down arrows; over the ring it doesn't");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            auto* knob = findKnob (editor);
            if (knob == nullptr)
                return expect (false, "no knob");

            const Arc arc (*knob);
            const FakeMouse mouse { *knob, {} };

            knob->mouseMove (mouse.event (onTheNumber (*knob), false));
            expect (knob->getMouseCursor() == juce::MouseCursor::UpDownResizeCursor, "no up-and-down arrows over the number");

            knob->mouseMove (mouse.event (arc.pointAt (arc.angleOf (*knob, 0.0)), false));
            expect (knob->getMouseCursor() == juce::MouseCursor::NormalCursor, "the arrows stayed on over the ring");
        }
    };

    // Never read: constructing it registers it with JUCE's test runner.
    [[maybe_unused]] KnobDragTests knobDragTests;
}
