// =============================================================================
//  Fading.h — makes any component fade in and out instead of snapping
// =============================================================================

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/**
    Any component, made to fade in and out instead of snapping on and off. A
    timer moves the fade along; a test can call advance() itself.

    A template: Fading<juce::Label> is a label that fades, Fading<juce::Component>
    a plain component. Like a C# generic, but the compiler writes a separate class
    for each. -> documentation/editor-gui.md section 14
*/
template <typename ComponentType>
class Fading : public ComponentType, private juce::Timer
{
public:
    Fading()                                { applyProgress(); }

    /** Starts fading in or out from wherever it is now. */
    void fadeTo (const bool shouldShow)
    {
        target = shouldShow ? 1.0 : 0.0;

        if (! juce::approximatelyEqual (progress, target) && ! isTimerRunning())
        {
            lastTickMs = juce::Time::getMillisecondCounterHiRes();
            startTimerHz (60);              // the message thread calls timerCallback about 60 times a second
        }
    }

    /** Shows or hides it at once: for when the window opens. */
    void showNow (const bool shouldShow)
    {
        stopTimer();
        progress = target = shouldShow ? 1.0 : 0.0;
        applyProgress();
    }

    /** Moves the fade on by `seconds`. */
    void advance (const double seconds)
    {
        const auto step = seconds / fadeSeconds;
        progress = target > progress ? juce::jmin (target, progress + step) : juce::jmax (target, progress - step);
        applyProgress();

        if (juce::approximatelyEqual (progress, target))
            stopTimer();
    }

    static constexpr double fadeSeconds = 0.35;

private:
    void timerCallback() override
    {
        // Timers can arrive late, so step by the real time since the last tick (at most 0.1 s).
        const auto now = juce::Time::getMillisecondCounterHiRes();
        advance (juce::jmin (0.1, (now - lastTickMs) / 1000.0));
        lastTickMs = now;
    }

    void applyProgress()
    {
        // Eased (smoothstep), so it starts and settles gently instead of moving at one speed.
        // `this->` because setAlpha comes from the template's base, which C++ only looks in when told.
        const auto eased = progress * progress * (3.0 - 2.0 * progress);
        this->setAlpha (static_cast<float> (eased));
        this->setVisible (progress > 0.0);
    }

    double progress = 0.0, target = 0.0;    // 0 hidden, 1 shown; the alpha eases between them
    double lastTickMs = 0.0;
};

/** The limit readout: a label that fades. */
using FadingLabel = Fading<juce::Label>;
