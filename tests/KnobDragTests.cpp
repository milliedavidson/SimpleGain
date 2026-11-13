// =============================================================================
//  KnobDragTests.cpp — Phase 6: dragging the knob with the mouse
//
//  These feed the knob fake mouse events, the same calls JUCE makes when you
//  click and drag. The knob should follow the pointer round its arc, but a click
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

            // Grab a quarter turn away from the dot, and closer to the centre, then turn by the
            // angle between -12.3 dB and 0 dB.
            const auto grab = arc.angleOf (*knob, -12.3) + pi / 2.0;
            const auto turn = arc.angleOf (*knob, 0.0) - arc.angleOf (*knob, -12.3);

            mouse.press (arc.pointAt (grab, arc.radius * 0.6f));
            expectWithinAbsoluteError (gainDb (processor), -12.3f, 0.01f, "the grab itself moved the gain");

            for (int i = 1; i <= 90; ++i)
                mouse.moveTo (arc.pointAt (grab + turn * i / 90, arc.radius * 0.6f));

            expectWithinAbsoluteError (static_cast<double> (gainDb (processor)), 0.0, dragTolerance, "turned by the angle from -12.3 to 0 dB");
            mouse.release (arc.pointAt (grab + turn, arc.radius * 0.6f));
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
    };

    // Never read: constructing it registers it with JUCE's test runner.
    [[maybe_unused]] KnobDragTests knobDragTests;
}
