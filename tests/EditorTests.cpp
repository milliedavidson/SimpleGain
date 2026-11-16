// =============================================================================
//  EditorTests.cpp — Phase 6: the GUI
//
//  No display is needed: JUCE components can be created, laid out and painted into
//  an image without a window. These check the parts that matter — the knob is
//  bound to the parameter in BOTH directions, the layout stays inside the window,
//  the editor can be opened and closed repeatedly, and it is drawn the way the
//  brand plan describes. Why each test exists: documentation/editor-gui.md
// =============================================================================
#include "TestSupport.h"
#include "PluginEditor.h"

#include <memory>

namespace
{
    using namespace support;

    constexpr double tolerance = 0.01;

    /** The editor's gain knob, found by the ID the editor gives it (so tests need no access to its internals). */
    juce::Slider* findKnob (const juce::Component& editor)
    {
        return dynamic_cast<juce::Slider*> (editor.findChildWithID ("gainSlider"));
    }

    /** The label inside the knob that shows the value, and the font JUCE will actually draw it with. */
    juce::Label* findValueLabel (const juce::Slider& knob)
    {
        for (auto* child : knob.getChildren())
            if (auto* label = dynamic_cast<juce::Label*> (child))
                return label;

        return nullptr;
    }

    /** The grip JUCE adds to the bottom-right corner of a resizable window, if there is one. */
    juce::Rectangle<int> resizeGrip (const juce::Component& editor)
    {
        for (auto* child : editor.getChildren())
            if (dynamic_cast<juce::ResizableCornerComponent*> (child) != nullptr)
                return child->getBounds();

        return {};
    }

    float valueTextHeight (juce::Label& label)
    {
        return label.getLookAndFeel().getLabelFont (label).getHeight();
    }

    juce::String typefaceOf (const juce::Font& font)
    {
        const auto typeface = font.getTypefacePtr();
        return typeface != nullptr ? typeface->getName() : juce::String();
    }

    //==========================================================================
    // Colours from the brand plan, and tests that tell them apart in a picture. Apricot and amber
    // share a hue, so saturation separates them. Each test also needs a bright, solid pixel, so
    // the blurred edge of a shape (part colour, part background) never counts.
    const juce::Colour base { 0xff121417 };

    bool isBase (const juce::Colour c)
    {
        return std::abs (c.getRed() - base.getRed()) <= 2 && std::abs (c.getGreen() - base.getGreen()) <= 2
            && std::abs (c.getBlue() - base.getBlue()) <= 2;
    }

    bool looksApricot (const juce::Colour c)    { return c.getSaturation() > 0.45f && c.getSaturation() < 0.72f && c.getBrightness() > 0.75f; }
    bool looksAmber (const juce::Colour c)      { return c.getSaturation() > 0.8f && c.getBrightness() > 0.75f; }
    bool looksTrack (const juce::Colour c)      { return c.getSaturation() < 0.35f && c.getBrightness() > 0.15f && c.getBrightness() < 0.3f; }
    bool isDrawnOn (const juce::Colour c)       { return ! isBase (c); }

    /** The editor painted with the gain at some value, plus where the knob's parts are in that picture. */
    struct KnobPicture
    {
        juce::Image image;
        juce::Point<float> centre;
        float radius = 0.0f;                    // the ring's radius: the middle of its stroke
        juce::Rectangle<int> valueBox;          // where the value text sits
        float startAngle = 0.0f, endAngle = 0.0f;

        /** The angle, clockwise from 12 o'clock, of a point some fraction of the way along the arc. */
        [[nodiscard]] float angleAt (const float proportion) const
        {
            return startAngle + proportion * (endAngle - startAngle);
        }

        /** Does any pixel along the line out from the centre at `angle`, between two distances given
            as fractions of the ring's radius, satisfy `test`? */
        template <typename Test>
        [[nodiscard]] bool anyAlong (const float angle, const float from, const float to, Test test) const
        {
            for (float distance = from * radius; distance <= to * radius; distance += 0.5f)
            {
                const auto p = centre.getPointOnCircumference (distance, angle);
                if (test (image.getPixelAt (juce::roundToInt (p.x), juce::roundToInt (p.y))))
                    return true;
            }

            return false;
        }

        /** On the ring itself, give or take a little either side of its stroke. */
        template <typename Test>
        [[nodiscard]] bool onRing (const float angle, Test test) const
        {
            return anyAlong (angle, 0.9f, 1.1f, test);
        }

        [[nodiscard]] int drawnPixelsIn (const juce::Rectangle<int> area) const
        {
            int count = 0;
            for (int y = area.getY(); y < area.getBottom(); ++y)
                for (int x = area.getX(); x < area.getRight(); ++x)
                    if (isDrawnOn (image.getPixelAt (x, y)))
                        ++count;

            return count;
        }

        [[nodiscard]] float brightestIn (const juce::Rectangle<int> area) const
        {
            float brightest = 0.0f;
            for (int y = area.getY(); y < area.getBottom(); ++y)
                for (int x = area.getX(); x < area.getRight(); ++x)
                    brightest = juce::jmax (brightest, image.getPixelAt (x, y).getBrightness());

            return brightest;
        }
    };

    KnobPicture pictureAt (const float db)
    {
        SimpleGainAudioProcessor processor;
        setGainDb (processor, db);
        SimpleGainAudioProcessorEditor editor (processor);

        KnobPicture picture;
        auto* knob = findKnob (editor);
        if (knob == nullptr)
            return picture;

        const auto layout = knob->getLookAndFeel().getSliderLayout (*knob);
        const auto rotary = knob->getRotaryParameters();
        const auto offset = knob->getPosition();

        picture.image = editor.createComponentSnapshot (editor.getLocalBounds());
        picture.centre = (layout.sliderBounds.getCentre() + offset).toFloat();
        picture.radius = SimpleGainLookAndFeel::ringRadius (layout.sliderBounds.toFloat());
        picture.valueBox = layout.textBoxBounds + offset;
        picture.startAngle = rotary.startAngleRadians;
        picture.endAngle = rotary.endAngleRadians;
        return picture;
    }

    struct EditorTests final : juce::UnitTest
    {
        EditorTests() : UnitTest ("Editor", "SimpleGain") {}

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
            valueTextScales();
            drawsSomething();
            knobSweep();
            knobArc();
            slimRing();
            amberWarmUp();
            flatKnob();
            valueInTheMiddle();
            nameAndFonts();
            openingAndClosing();
        }

        //======================================================================
        void ownEditor()
        {
            beginTest ("The plugin offers its own editor, not JUCE's generic one");

            SimpleGainAudioProcessor processor;
            expect (processor.hasEditor());

            const std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
            expect (dynamic_cast<SimpleGainAudioProcessorEditor*> (editor.get()) != nullptr,
                    "createEditor() returned something other than SimpleGainAudioProcessorEditor");
        }

        void sizeAndLimits()
        {
            beginTest ("The editor opens at 480 x 320, the family's smallest size, and grows to 200% keeping its shape");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);

            const auto* constrainer = editor.getConstrainer();
            expect (constrainer != nullptr, "the editor isn't resizable");
            if (constrainer == nullptr)
                return;

            expectEquals (editor.getWidth(), 480);
            expectEquals (editor.getHeight(), 320);
            expectEquals (constrainer->getMinimumWidth(), 480);
            expectEquals (constrainer->getMinimumHeight(), 320);
            expectEquals (constrainer->getMaximumWidth(), 960);
            expectEquals (constrainer->getMaximumHeight(), 640);
            expectWithinAbsoluteError (constrainer->getFixedAspectRatio(), 1.5, 0.001);
        }

        //======================================================================
        void knobStartsAtTheFader()
        {
            beginTest ("The knob starts at the fader's position, not at the default");

            SimpleGainAudioProcessor processor;
            setGainDb (processor, -6.0f);

            SimpleGainAudioProcessorEditor editor (processor);
            const auto* knob = findKnob (editor);
            expect (knob != nullptr, "no knob with the ID \"gainSlider\"");
            if (knob != nullptr)
                expectWithinAbsoluteError (knob->getValue(), -6.0, tolerance);
        }

        void turningTheKnob()
        {
            beginTest ("Turning the knob moves the parameter");

            SimpleGainAudioProcessor processor;
            const SimpleGainAudioProcessorEditor editor (processor);
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
            const SimpleGainAudioProcessorEditor editor (processor);
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
            const SimpleGainAudioProcessorEditor editor (processor);
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
            const SimpleGainAudioProcessorEditor editor (processor);
            const auto* knob = findKnob (editor);
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
            const auto* knob = findKnob (editor);
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

                for (const auto* child : editor.getChildren())
                    expect (editor.getLocalBounds().contains (child->getBounds()), "a child sticks out at " + label);
            }
        }

        void valueTextScales()
        {
            beginTest ("The value text grows and shrinks with the window, and always fits its box");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            const auto* knob = findKnob (editor);
            const auto* constrainer = editor.getConstrainer();
            if (knob == nullptr || constrainer == nullptr)
                return expect (false, "no knob or no constrainer");

            // Resize, then find the value label AGAIN rather than keeping a pointer to it: JUCE
            // owns it and can rebuild it, which would leave a kept pointer dangling.
            // -> documentation/editor-gui.md section 7
            const auto textHeightAt = [&] (const int width, const int height, const juce::String& name)
            {
                editor.setSize (width, height);
                auto* label = findValueLabel (*knob);
                if (label == nullptr)
                    return expect (false, "no value label at the " + name + " size"), 0.0f;

                const auto textHeight = valueTextHeight (*label);
                expectLessOrEqual (textHeight, static_cast<float> (label->getHeight()), "text taller than its box at the " + name + " size");
                return textHeight;
            };

            const auto smallest = textHeightAt (constrainer->getMinimumWidth(), constrainer->getMinimumHeight(), "smallest");
            const auto largest  = textHeightAt (constrainer->getMaximumWidth(), constrainer->getMaximumHeight(), "largest");

            // The window is twice as tall at its largest as at its smallest, so the text should grow by about that much.
            expectGreaterThan (largest, smallest * 1.6f, "the value text doesn't scale with the window");

            // The look-and-feel is only meant for the knob's value box: the title keeps the font it was given.
            if (auto* title = dynamic_cast<juce::Label*> (editor.getChildComponent (0)))
                expectWithinAbsoluteError (valueTextHeight (*title), title->getFont().getHeight(), 0.01f,
                                           "the title's font was changed by the look-and-feel");
            else
                expect (false, "the editor's first child isn't the title label");
        }

        void drawsSomething()
        {
            beginTest ("It actually draws: a flat Base background, and a knob that is visible and changes the picture");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            auto* knob = findKnob (editor);
            if (knob == nullptr)
                return expect (false, "no knob");

            const auto withKnob = editor.createComponentSnapshot (editor.getLocalBounds());
            expect (withKnob.isValid());
            expect (knob->isVisible(), "the knob is in the window but not visible");

            // The bottom-right corner holds JUCE's resize grip, so check just above it.
            const auto right = withKnob.getWidth() - 3, bottom = withKnob.getHeight() - 3;
            const auto aboveTheGrip = resizeGrip (editor).getY() - 3;
            for (const auto [x, y] : { std::pair { 2, 2 }, std::pair { right, 2 }, std::pair { 2, bottom }, std::pair { right, aboveTheGrip } })
                expect (isBase (withKnob.getPixelAt (x, y)), "the background isn't flat Base (#121417) at a corner");

            // The grip is drawn quietly, in the track's colour, not JUCE's bright grey.
            float brightestInGrip = 0.0f;
            const auto grip = resizeGrip (editor);
            for (int y = grip.getY(); y < grip.getBottom(); ++y)
                for (int x = grip.getX(); x < grip.getRight(); ++x)
                    brightestInGrip = juce::jmax (brightestInGrip, withKnob.getPixelAt (x, y).getBrightness());

            expect (! grip.isEmpty(), "no resize grip");
            expectLessThan (brightestInGrip, 0.25f, "the resize grip is brighter than the ring's track");

            // Compare against the same window with the knob hidden: the knob's own area must change.
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
        void knobSweep()
        {
            beginTest ("The knob sweeps from 1.2 pi to 2.8 pi, leaving a gap at the bottom");

            SimpleGainAudioProcessor processor;
            const SimpleGainAudioProcessorEditor editor (processor);
            const auto* knob = findKnob (editor);
            if (knob == nullptr)
                return expect (false, "no knob");

            constexpr float pi = juce::MathConstants<float>::pi;
            const auto rotary = knob->getRotaryParameters();
            expectWithinAbsoluteError (rotary.startAngleRadians, pi * 1.2f, 0.001f);
            expectWithinAbsoluteError (rotary.endAngleRadians, pi * 2.8f, 0.001f);
            expect (rotary.stopAtEnd);
        }

        void knobArc()
        {
            beginTest ("The apricot arc runs from the minimum to the value, over a dim track, with nothing in the gap");

            const auto atMinimum = pictureAt (-60.0f);
            const auto atUnity   = pictureAt (0.0f);        // two-thirds of the way round: about 1:30
            const auto atMaximum = pictureAt (30.0f);

            if (atMinimum.radius <= 0.0f || atUnity.radius <= 0.0f || atMaximum.radius <= 0.0f)
                return expect (false, "no knob");

            const auto at = [] (const float degrees) { return juce::degreesToRadians (degrees); };

            for (const float degrees : { -90.0f, 0.0f, 90.0f })
            {
                const auto where = juce::String (degrees) + " degrees";
                expect (! atMinimum.onRing (at (degrees), looksApricot), "arc drawn at " + where + " at minimum gain");
                expect (atMinimum.onRing (at (degrees), looksTrack), "no track at " + where + " at minimum gain");
                expect (atMaximum.onRing (at (degrees), looksApricot), "arc missing at " + where + " at maximum gain");
            }

            expect (atUnity.onRing (at (-90.0f), looksApricot) && atUnity.onRing (at (0.0f), looksApricot), "arc missing before the value at 0 dB");
            expect (! atUnity.onRing (at (90.0f), looksApricot), "arc runs past the value at 0 dB");
            expect (atUnity.onRing (at (90.0f), looksTrack), "no track after the value at 0 dB");

            // The gap at the bottom stays empty: no track, no arc, at any gain.
            for (const auto* picture : { &atMinimum, &atUnity, &atMaximum })
                expect (! picture->onRing (juce::MathConstants<float>::pi, isDrawnOn), "something drawn in the gap at the bottom");
        }

        void slimRing()
        {
            beginTest ("The ring is slim: 2.5% of the knob's diameter");

            const auto picture = pictureAt (30.0f);
            if (picture.radius <= 0.0f)
                return expect (false, "no knob");

            // Count the drawn pixels straight up through the top of the ring.
            const auto x = juce::roundToInt (picture.centre.x);
            const auto area = juce::Rectangle<int>::leftTopRightBottom (x, juce::roundToInt (picture.centre.y - picture.radius * 1.2f),
                                                                        x + 1, juce::roundToInt (picture.centre.y - picture.radius * 0.8f));
            const auto thickness = static_cast<float> (picture.drawnPixelsIn (area));
            const auto expected = 0.025f * picture.radius * 2.0f;

            expectGreaterOrEqual (thickness, expected - 1.0f, "ring thinner than 2.5% of the diameter");
            expectLessOrEqual (thickness, expected + 2.0f, "ring thicker than 2.5% of the diameter");
        }

        void amberWarmUp()
        {
            beginTest ("Past 85% of the range the arc turns Volcanic Amber, and not before");

            const auto at10 = pictureAt (10.0f);            // 78% of the way round
            const auto at20 = pictureAt (20.0f);            // 89%
            const auto at30 = pictureAt (30.0f);            // 100%

            if (at10.radius <= 0.0f || at20.radius <= 0.0f || at30.radius <= 0.0f)
                return expect (false, "no knob");

            for (float proportion = 0.0f; proportion <= 1.0f; proportion += 0.02f)
                expect (! at10.onRing (at10.angleAt (proportion), looksAmber), "amber at +10 dB, below the knee");

            expect (at20.onRing (at20.angleAt (0.8f), looksApricot), "not apricot below the knee at +20 dB");
            expect (at20.onRing (at20.angleAt (0.87f), looksAmber), "not amber just past the knee at +20 dB");
            expect (! at20.onRing (at20.angleAt (0.8f), looksAmber), "amber below the knee at +20 dB");

            expect (at30.onRing (at30.angleAt (0.5f), looksApricot), "not apricot half-way at +30 dB");
            expect (at30.onRing (at30.angleAt (0.95f), looksAmber), "not amber near the end at +30 dB");
        }

        void flatKnob()
        {
            beginTest ("The knob is flat: no face, ticks, glow or thumb dot");

            for (const float db : { -60.0f, 0.0f, 30.0f })
            {
                const auto picture = pictureAt (db);
                if (picture.radius <= 0.0f)
                    return expect (false, "no knob");

                const auto label = juce::String (db) + " dB";

                // Inside the ring away from the text, and just outside it, there is only background.
                for (const float degrees : { 0.0f, 45.0f, 135.0f, 180.0f, 225.0f, 315.0f })
                {
                    const auto angle = juce::degreesToRadians (degrees);
                    expect (! picture.anyAlong (angle, 0.55f, 0.88f, isDrawnOn), "something drawn inside the ring at " + label);
                    expect (! picture.anyAlong (angle, 1.12f, 1.2f, isDrawnOn), "something drawn outside the ring at " + label);
                }
            }
        }

        void valueInTheMiddle()
        {
            beginTest ("The value sits in the middle of the ring, with a muted caption beneath it");

            const auto picture = pictureAt (0.0f);
            if (picture.radius <= 0.0f)
                return expect (false, "no knob");

            const auto box = picture.valueBox.toFloat();
            expectWithinAbsoluteError (box.getCentreX(), picture.centre.x, 1.0f, "value not centred across the ring");
            expect (std::abs (box.getCentreY() - picture.centre.y) < picture.radius * 0.15f, "value not in the middle of the ring");
            expectLessThan (box.getWidth(), picture.radius * 1.8f, "value box wider than the inside of the ring");

            const auto caption = juce::Rectangle<float> (picture.centre.x - picture.radius * 0.4f, box.getBottom(),
                                                         picture.radius * 0.8f, picture.radius * 0.3f).toNearestInt();
            expectGreaterThan (picture.drawnPixelsIn (caption), 20, "nothing drawn beneath the value");
            expectLessThan (picture.brightestIn (caption), 0.7f, "the caption isn't muted");
        }

        void nameAndFonts()
        {
            beginTest ("SIMPLY GAIN at the top in the mono font, the value in Inter, and the lockup bottom left");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            const auto* knob = findKnob (editor);
            auto* title = dynamic_cast<juce::Label*> (editor.getChildComponent (0));
            if (knob == nullptr || title == nullptr)
                return expect (false, "no knob or no title");

            expectEquals (title->getText(), juce::String ("SIMPLY GAIN"));
            expectEquals (typefaceOf (title->getFont()), juce::String ("JetBrains Mono"));
            expect (title->getBounds().getCentreY() < editor.getHeight() / 5, "the title isn't at the top");

            if (auto* value = findValueLabel (*knob))
                expectEquals (typefaceOf (value->getLookAndFeel().getLabelFont (*value)), juce::String ("Inter"));
            else
                expect (false, "no value label");

            // The lockup is painted along the bottom left, and the bottom right stays empty (apart from the resize grip).
            KnobPicture picture;
            picture.image = editor.createComponentSnapshot (editor.getLocalBounds());
            const auto bottomStrip = editor.getLocalBounds().removeFromBottom (editor.getHeight() / 8);
            const auto [left, right] = std::pair { bottomStrip.withWidth (editor.getWidth() / 2),
                                                   bottomStrip.withLeft (editor.getWidth() * 3 / 4).withRight (resizeGrip (editor).getX()) };
            expectGreaterThan (picture.drawnPixelsIn (left), 100, "no lockup at the bottom left");
            expectLessThan (picture.brightestIn (left), 0.7f, "the lockup isn't muted");
            expectEquals (picture.drawnPixelsIn (right), 0, "something drawn at the bottom right");
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
                const auto* knob = findKnob (editor);
                if (knob == nullptr)
                    return expect (false, "no knob");

                expectWithinAbsoluteError (knob->getValue(), -9.0, tolerance, "open number " + juce::String (i));
            }

            // The processor outlives its editors. Moving the parameter now, with no editor open, must be harmless:
            // the closed editors must have disconnected themselves.
            setGainDb (processor, 12.0f);
            expectWithinAbsoluteError (static_cast<double> (gainDb (processor)), 12.0, tolerance);

            const SimpleGainAudioProcessorEditor reopened (processor);
            if (const auto* knob = findKnob (reopened))
                expectWithinAbsoluteError (knob->getValue(), 12.0, tolerance, "reopened after the parameter moved");
        }
    };

    // Never read: constructing it registers it with JUCE's test runner.
    [[maybe_unused]] EditorTests editorTests;
}
