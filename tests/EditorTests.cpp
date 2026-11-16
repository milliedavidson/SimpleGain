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
    // Colours from the brand plan, and tests that tell them apart in a picture. Quiet, apricot and
    // amber share a hue, so saturation separates them; ember is redder. Each test also needs a
    // bright, solid pixel, so the blurred edge of a shape (part colour, part background) never counts.
    const juce::Colour base { 0xff121417 };

    bool isBase (const juce::Colour c)
    {
        return std::abs (c.getRed() - base.getRed()) <= 2 && std::abs (c.getGreen() - base.getGreen()) <= 2
            && std::abs (c.getBlue() - base.getBlue()) <= 2;
    }

    bool looksApricot (const juce::Colour c)    { return c.getSaturation() > 0.45f && c.getSaturation() < 0.72f && c.getBrightness() > 0.75f; }
    bool looksAmber (const juce::Colour c)      { return c.getHue() > 0.06f && c.getHue() < 0.12f && c.getSaturation() > 0.8f && c.getBrightness() > 0.75f; }
    bool looksEmber (const juce::Colour c)      { return (c.getHue() < 0.05f || c.getHue() > 0.97f) && c.getSaturation() > 0.7f && c.getBrightness() > 0.6f; }
    bool looksQuiet (const juce::Colour c)      { return c.getHue() > 0.05f && c.getHue() < 0.12f && c.getSaturation() > 0.15f && c.getSaturation() < 0.42f && c.getBrightness() > 0.85f; }
    bool looksWarm (const juce::Colour c)       { return looksQuiet (c) || looksApricot (c); }
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

        /** The colour in the middle of the ring's stroke at `angle`. */
        [[nodiscard]] juce::Colour colourOnRing (const float angle) const
        {
            const auto p = centre.getPointOnCircumference (radius, angle);
            return image.getPixelAt (juce::roundToInt (p.x), juce::roundToInt (p.y));
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

        /** How many rows of `area` have anything drawn in them. */
        [[nodiscard]] int drawnRowsIn (const juce::Rectangle<int> area) const
        {
            int rows = 0;
            for (int y = area.getY(); y < area.getBottom(); ++y)
                if (drawnPixelsIn ({ area.getX(), y, area.getWidth(), 1 }) > 0)
                    ++rows;

            return rows;
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

        // Just the knob's own drawing: the limit readout below it has tests of its own.
        if (auto* readout = editor.findChildWithID ("limitReadout"))
            readout->setVisible (false);

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
            quietEnd();
            coloursBlend();
            amberWarmUp();
            emberAtTheLimit();
            limitReadout();
            flatKnob();
            valueInTheMiddle();
            readableCaption();
            nameAndFonts();
            plainTitle();
            brandLogo();
            ringMark();
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

            // The look-and-feel is only meant for the knob's value box: the readout keeps the font it was given.
            if (auto* readout = dynamic_cast<juce::Label*> (editor.findChildWithID ("limitReadout")))
                expectWithinAbsoluteError (valueTextHeight (*readout), readout->getFont().getHeight(), 0.01f,
                                           "the readout's font was changed by the look-and-feel");
            else
                expect (false, "no limit readout label");
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
            beginTest ("The arc runs from the minimum to the value, over a dim track, with nothing in the gap");

            const auto atMinimum = pictureAt (-60.0f);
            const auto atUnity   = pictureAt (0.0f);        // two-thirds of the way round: about 1:30
            const auto atMaximum = pictureAt (30.0f);

            if (atMinimum.radius <= 0.0f || atUnity.radius <= 0.0f || atMaximum.radius <= 0.0f)
                return expect (false, "no knob");

            const auto at = [] (const float degrees) { return juce::degreesToRadians (degrees); };

            for (const float degrees : { -90.0f, 0.0f, 90.0f })
            {
                const auto where = juce::String (degrees) + " degrees";
                expect (! atMinimum.onRing (at (degrees), looksWarm), "arc drawn at " + where + " at minimum gain");
                expect (atMinimum.onRing (at (degrees), looksTrack), "no track at " + where + " at minimum gain");
                expect (atMaximum.onRing (at (degrees), looksWarm), "arc missing at " + where + " at maximum gain");
            }

            expect (atUnity.onRing (at (-90.0f), looksWarm) && atUnity.onRing (at (0.0f), looksWarm), "arc missing before the value at 0 dB");
            expect (! atUnity.onRing (at (90.0f), looksWarm), "arc runs past the value at 0 dB");
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
            beginTest ("Around 85% of the range the arc warms to Volcanic Amber, and not before");

            const auto at10 = pictureAt (10.0f);            // 78% of the way round
            const auto at22 = pictureAt (22.0f);            // 91%
            const auto at30 = pictureAt (30.0f);            // 100%

            if (at10.radius <= 0.0f || at22.radius <= 0.0f || at30.radius <= 0.0f)
                return expect (false, "no knob");

            for (float proportion = 0.0f; proportion <= 1.0f; proportion += 0.02f)
                expect (! at10.onRing (at10.angleAt (proportion), looksAmber), "amber at +10 dB, below the knee");

            expect (at22.onRing (at22.angleAt (0.8f), looksApricot), "not apricot below the knee at +22 dB");
            expect (at22.onRing (at22.angleAt (0.9f), looksAmber), "not amber past the knee at +22 dB");
            expect (! at22.onRing (at22.angleAt (0.8f), looksAmber), "amber below the knee at +22 dB");

            expect (at30.onRing (at30.angleAt (0.5f), looksApricot), "not apricot half-way at +30 dB");
            expect (at30.onRing (at30.angleAt (0.9f), looksAmber), "not amber between the knee and the red at +30 dB");
        }

        void quietEnd()
        {
            beginTest ("The low end of the arc is a lighter, quieter apricot that blends into apricot by the middle");

            const auto picture = pictureAt (30.0f);
            if (picture.radius <= 0.0f)
                return expect (false, "no knob");

            const auto start = picture.colourOnRing (picture.angleAt (0.03f));
            const auto blend = picture.colourOnRing (picture.angleAt (0.25f));
            const auto middle = picture.colourOnRing (picture.angleAt (0.6f));

            expect (looksQuiet (start), "the start of the arc isn't the quiet colour: " + start.toDisplayString (false));
            expect (looksApricot (middle), "the arc isn't apricot past the middle: " + middle.toDisplayString (false));
            expect (blend.getSaturation() > start.getSaturation() + 0.04f && blend.getSaturation() < middle.getSaturation() - 0.04f,
                    "a quarter of the way round isn't a blend of the two: " + blend.toDisplayString (false));
        }

        void emberAtTheLimit()
        {
            beginTest ("Around 95% of the range the arc deepens to red-orange, and not before");

            const auto at25 = pictureAt (25.0f);            // 94%: amber, not yet red
            const auto at28 = pictureAt (28.0f);            // 98%
            const auto at30 = pictureAt (30.0f);

            if (at25.radius <= 0.0f || at28.radius <= 0.0f || at30.radius <= 0.0f)
                return expect (false, "no knob");

            for (float proportion = 0.0f; proportion <= 1.0f; proportion += 0.01f)
                expect (! at25.onRing (at25.angleAt (proportion), looksEmber), "red at +25 dB, below 95%");

            expect (at28.onRing (at28.angleAt (0.975f), looksEmber), "not red near the end at +28 dB");
            expect (at30.onRing (at30.angleAt (0.98f), looksEmber), "not red at the end at +30 dB");
            expect (! at30.onRing (at30.angleAt (0.9f), looksEmber), "red before 95% at +30 dB");
        }

        void coloursBlend()
        {
            beginTest ("The colours blend into each other along the arc: no sudden step anywhere");

            const auto picture = pictureAt (30.0f);
            if (picture.radius <= 0.0f)
                return expect (false, "no knob");

            // Walk the middle of the stroke in steps of 0.5% of the range (about 2 pixels at the opening size).
            // A hard edge jumps a long way in one step: apricot to amber changes blue by 71 out of 255.
            int biggestStep = 0;
            float whereBiggest = 0.0f;
            auto previous = picture.colourOnRing (picture.angleAt (0.02f));

            for (float proportion = 0.025f; proportion <= 0.985f; proportion += 0.005f)
            {
                const auto colour = picture.colourOnRing (picture.angleAt (proportion));
                const auto step = juce::jmax (std::abs (colour.getRed() - previous.getRed()),
                                              std::abs (colour.getGreen() - previous.getGreen()),
                                              std::abs (colour.getBlue() - previous.getBlue()));
                if (step > biggestStep)
                    biggestStep = step, whereBiggest = proportion;

                previous = colour;
            }

            expectLessThan (biggestStep, 25, "a hard colour edge at " + juce::String (whereBiggest * 100.0f, 1) + "% of the way round");
        }

        void limitReadout()
        {
            beginTest ("Entering the red, \"Approaching the limit.\" fades in by the end of the arc, and fades out again");

            SimpleGainAudioProcessor processor;
            setGainDb (processor, 20.0f);
            SimpleGainAudioProcessorEditor editor (processor);
            auto* readout = dynamic_cast<FadingLabel*> (editor.findChildWithID ("limitReadout"));
            const auto* knob = findKnob (editor);
            if (readout == nullptr || knob == nullptr)
                return expect (false, "no limit readout, or no knob");

            expectEquals (readout->getText(), juce::String ("Approaching the limit."));
            expectEquals (typefaceOf (readout->getFont()), juce::String ("JetBrains Mono"));
            if (const auto* logo = editor.findChildWithID ("logo"))
                expectLessOrEqual (readout->getBottom(), logo->getY(), "the readout runs into the logo");
            else
                expect (false, "no logo");

            // Where the cursor is when turning up to the limit: starting just under the arc's end, at the
            // bottom right (0.59 of the ring's radius right of its centre, 0.81 below), and running right.
            // Checked at every size: the text must always fit, inside the margin, without being cut short.
            const auto* limits = editor.getConstrainer();
            for (const auto height : { limits->getMinimumHeight(), editor.getHeight(), limits->getMaximumHeight() })
            {
                editor.setSize (height * 3 / 2, height);
                const auto size = juce::String (editor.getWidth()) + " x " + juce::String (height);
                const auto bounds = readout->getBounds().toFloat();
                const auto radius = SimpleGainLookAndFeel::ringRadius (knob->getBounds().toFloat());
                const auto arcEnd = knob->getBounds().toFloat().getCentre().translated (0.59f * radius, 0.81f * radius);
                const auto textWidth = juce::GlyphArrangement::getStringWidth (readout->getFont(), readout->getText());
                const auto margin = static_cast<float> (height) * 0.06f;

                expect (readout->getJustificationType().testFlags (juce::Justification::left), "the readout isn't left-aligned");
                expectWithinAbsoluteError (bounds.getX(), arcEnd.x, 1.5f, "the text doesn't start under the arc's end at " + size);
                expectLessOrEqual (textWidth, bounds.getWidth(), "the readout's text is cut short at " + size);
                expectLessOrEqual (bounds.getX() + textWidth, static_cast<float> (editor.getWidth()) - margin, "the text runs past the margin at " + size);
                expect (bounds.getCentreY() > arcEnd.y + static_cast<float> (height) * 0.03f, "the readout is cramped against the arc's end at " + size);
                expect (bounds.getCentreY() < arcEnd.y + static_cast<float> (height) * 0.12f, "the readout is too far below the arc's end at " + size);
            }

            editor.setSize (limits->getMinimumWidth(), limits->getMinimumHeight());
            expect (! readout->isVisible() || readout->getAlpha() == 0.0f, "showing at +20 dB, in the amber");

            // Into the red: it starts to fade in, rather than snapping on.
            setGainDb (processor, 26.0f);
            expectLessThan (readout->getAlpha(), 0.2f, "it snapped on");
            readout->advance (0.1);
            expect (readout->isVisible(), "not visible once fading in");
            expect (readout->getAlpha() > 0.05f && readout->getAlpha() < 0.95f, "not part-way through the fade after 0.1 s");
            readout->advance (1.0);
            expectEquals (readout->getAlpha(), 1.0f, "not fully shown after a second");

            // Amber text, readable on the background.
            auto image = editor.createComponentSnapshot (readout->getBounds());
            int amberPixels = 0;
            for (int y = 0; y < image.getHeight(); ++y)
                for (int x = 0; x < image.getWidth(); ++x)
                    if (const auto c = image.getPixelAt (x, y); c.getHue() > 0.06f && c.getHue() < 0.12f && c.getSaturation() > 0.6f && c.getBrightness() > 0.6f)
                        ++amberPixels;
            expectGreaterThan (amberPixels, 30, "the readout isn't drawn in amber");

            // Back out of the red: it fades away, then hides.
            setGainDb (processor, 25.0f);
            readout->advance (0.1);
            expect (readout->getAlpha() > 0.05f && readout->getAlpha() < 0.95f, "not part-way through fading out after 0.1 s");
            readout->advance (1.0);
            expect (! readout->isVisible() && readout->getAlpha() == 0.0f, "still showing after fading out");

            // Opened already in the red, it's simply there: nothing to fade from.
            setGainDb (processor, 29.0f);
            const SimpleGainAudioProcessorEditor reopened (processor);
            if (auto* shown = dynamic_cast<FadingLabel*> (reopened.findChildWithID ("limitReadout")))
                expect (shown->isVisible() && shown->getAlpha() == 1.0f, "not shown when opened at +29 dB");
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

        void readableCaption()
        {
            beginTest ("The caption under the value is big enough to read: about 10 pixels tall at the opening size");

            const auto picture = pictureAt (0.0f);
            if (picture.radius <= 0.0f)
                return expect (false, "no knob");

            const auto caption = juce::Rectangle<float> (picture.centre.x - picture.radius * 0.4f, static_cast<float> (picture.valueBox.getBottom()),
                                                         picture.radius * 0.8f, picture.radius * 0.35f).toNearestInt();
            expectGreaterOrEqual (picture.drawnRowsIn (caption), 9, "the caption is too small to read easily");
        }

        void nameAndFonts()
        {
            beginTest ("\"simply gain\" at the top in the logo's font (Sora), the value in Inter, and the brand logo bottom left");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            const auto* knob = findKnob (editor);
            auto* title = dynamic_cast<juce::Label*> (editor.findChildWithID ("title"));
            if (knob == nullptr || title == nullptr)
                return expect (false, "no knob or no title");

            expectEquals (title->getText(), juce::String ("simply gain"));
            expectEquals (typefaceOf (title->getFont()), juce::String ("Sora"));
            expect (title->getBounds().getCentreY() < editor.getHeight() / 5, "the title isn't at the top");
            expectWithinAbsoluteError (title->getBounds().getCentreX(), editor.getWidth() / 2, 1, "the title isn't centred");

            if (auto* value = findValueLabel (*knob))
                expectEquals (typefaceOf (value->getLookAndFeel().getLabelFont (*value)), juce::String ("Inter"));
            else
                expect (false, "no value label");

            // The logo is along the bottom left, and the bottom right stays empty (apart from the resize grip).
            KnobPicture picture;
            picture.image = editor.createComponentSnapshot (editor.getLocalBounds());
            const auto bottomStrip = editor.getLocalBounds().removeFromBottom (editor.getHeight() / 8);
            const auto [left, right] = std::pair { bottomStrip.withWidth (editor.getWidth() / 2),
                                                   bottomStrip.withLeft (editor.getWidth() * 3 / 4).withRight (resizeGrip (editor).getX()) };
            expectGreaterThan (picture.drawnPixelsIn (left), 100, "no logo at the bottom left");
            expectLessThan (picture.brightestIn (left), 0.7f, "the logo isn't muted");
            expectEquals (picture.drawnPixelsIn (right), 0, "something drawn at the bottom right");
        }

        void plainTitle()
        {
            beginTest ("The title is plain: muted, no wave under it, and its last letter level with its first");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            auto* title = editor.findChildWithID ("title");
            if (title == nullptr)
                return expect (false, "no title");

            KnobPicture picture;
            picture.image = editor.createComponentSnapshot (title->getBounds());
            const auto local = title->getLocalBounds();
            expectGreaterThan (picture.drawnPixelsIn (local), 200, "the title isn't drawn");
            expectLessThan (picture.brightestIn (local), 0.7f, "the title isn't muted");

            // The ink's left and right edges, then the top and bottom rows drawn in a narrow slice at each end.
            // "s" and "n" are both plain lower-case letters: no ascender, no descender, so they line up.
            // Only the middle half: the strip runs the full width, and the ring mark is painted at its left.
            int inkLeft = local.getRight(), inkRight = 0;
            for (int x = local.getWidth() / 4; x < local.getWidth() * 3 / 4; ++x)
                if (picture.drawnPixelsIn ({ x, 0, 1, local.getHeight() }) > 0)
                    inkLeft = juce::jmin (inkLeft, x), inkRight = juce::jmax (inkRight, x);

            const auto slice = juce::jmax (2, (inkRight - inkLeft) / 20);
            const auto rowsDrawn = [&] (const int fromX)
            {
                int top = local.getBottom(), bottom = 0;
                for (int y = 0; y < local.getHeight(); ++y)
                    if (picture.drawnPixelsIn ({ fromX, y, slice, 1 }) > 0)
                        top = juce::jmin (top, y), bottom = juce::jmax (bottom, y);
                return std::pair { top, bottom };
            };

            const auto [sTop, sBottom] = rowsDrawn (inkLeft);
            const auto [nTop, nBottom] = rowsDrawn (inkRight - slice + 1);
            expectWithinAbsoluteError (nTop, sTop, 1, "the last letter isn't level with the first: knocked up?");
            expectWithinAbsoluteError (nBottom, sBottom, 1, "something is drawn under the ends of the name: a wave?");
        }

        void brandLogo()
        {
            beginTest ("The brand logo sits bottom left: muted, with a growing wave under it whose last crest knocks the t upwards");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            auto* logo = dynamic_cast<Wordmark*> (editor.findChildWithID ("logo"));
            if (logo == nullptr)
                return expect (false, "no logo");

            expectEquals (logo->getText(), juce::String ("maxximum displacement"));
            expectEquals (typefaceOf (logo->getFont()), juce::String ("Sora"));
            expect (logo->getY() > editor.getHeight() * 3 / 4, "the logo isn't at the bottom");
            expectLessThan (static_cast<float> (logo->getX()) + logo->letterBounds (0).getX(), editor.getWidth() * 0.1f, "the logo doesn't start at the left");

            KnobPicture picture;
            picture.image = editor.createComponentSnapshot (logo->getBounds());
            const auto local = logo->getLocalBounds();
            expectGreaterThan (picture.drawnPixelsIn (local), 200, "the logo isn't drawn");
            expectLessThan (picture.brightestIn (local), 0.7f, "the logo isn't muted");

            // The last letter sits higher than the rest, but not so far it reads as a footnote mark.
            const auto last = logo->getText().length() - 1;
            const auto lifted = logo->letterBounds (last);
            const auto beforeIt = logo->letterBounds (last - 2);           // the e: no ascender, no descender
            const auto fontHeight = logo->getFont().getHeight();
            expect (lifted.getBottom() < beforeIt.getBottom() - 0.1f * fontHeight, "the t isn't knocked upwards");
            expect (lifted.getBottom() > beforeIt.getBottom() - 0.5f * fontHeight, "the t is lifted so far it reads as a footnote mark");
            expectGreaterThan (picture.drawnPixelsIn (lifted.toNearestInt()), 10, "nothing drawn where the t should be");

            // The wave runs under the text, left to right, and grows: it's taller under the end than under the start.
            const auto textBottom = juce::roundToInt (beforeIt.getBottom());
            const auto under = local.withTop (textBottom);
            const auto textWidth = lifted.getRight() - logo->letterBounds (0).getX();
            const auto left = under.withX (juce::roundToInt (logo->letterBounds (0).getX())).withWidth (juce::roundToInt (textWidth / 3.0f));
            const auto right = under.withX (juce::roundToInt (logo->letterBounds (last - 3).getX())).withRight (juce::roundToInt (lifted.getRight()));
            expectGreaterThan (picture.drawnPixelsIn (left), 10, "no wave under the start of the name");
            expectGreaterThan (picture.drawnRowsIn (right), picture.drawnRowsIn (left), "the wave doesn't grow towards the end");
        }

        void ringMark()
        {
            beginTest ("The ring mark sits in the top-left corner: muted, with its inner circles pushed off-centre");

            SimpleGainAudioProcessor processor;
            SimpleGainAudioProcessorEditor editor (processor);
            KnobPicture picture;
            picture.image = editor.createComponentSnapshot (editor.getLocalBounds());

            const auto corner = juce::Rectangle<int> (0, 0, editor.getWidth() / 6, editor.getHeight() / 5);
            expectGreaterThan (picture.drawnPixelsIn (corner), 50, "nothing in the top-left corner");
            expectLessThan (picture.brightestIn (corner), 0.7f, "the mark isn't muted");

            // Find the outer ring: the box around everything drawn in the corner.
            int left = corner.getRight(), right = 0, top = corner.getBottom(), bottom = 0;
            for (int y = corner.getY(); y < corner.getBottom(); ++y)
                for (int x = corner.getX(); x < corner.getRight(); ++x)
                    if (isDrawnOn (picture.image.getPixelAt (x, y)))
                        left = juce::jmin (left, x), right = juce::jmax (right, x), top = juce::jmin (top, y), bottom = juce::jmax (bottom, y);

            const auto size = right - left;
            expectWithinAbsoluteError (bottom - top, size, 2, "the mark isn't round");
            expectGreaterThan (size, 16, "the mark is too small to see");

            // Like a speaker cone pushed to one side: the middle is empty, the dot is to the right of it, and the left of it is empty.
            const auto cx = (left + right) / 2, cy = (top + bottom) / 2;
            const auto r = size / 2.0f;
            expect (! isDrawnOn (picture.image.getPixelAt (cx, cy)), "the middle of the mark is filled: it's centred, not pushed off-centre");
            expect (isDrawnOn (picture.image.getPixelAt (cx + juce::roundToInt (r * 0.4f), cy)), "no dot to the right of the middle");
            expect (! isDrawnOn (picture.image.getPixelAt (cx - juce::roundToInt (r * 0.55f), cy)), "something to the left of the middle");
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
