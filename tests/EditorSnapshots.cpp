// =============================================================================
//  EditorSnapshots.cpp — render the real editor to PNG files, with no window
// =============================================================================
#include "EditorSnapshots.h"
#include "TestSupport.h"
#include "PluginEditor.h"

#include <vector>

namespace
{
    using namespace support;

    /** Paints the editor, as it would look at `width` x `height` with the gain at `db`,
        and with one of its panels open if `openPanel` names its button. */
    juce::Image snapshot (float db, int width, int height, float scale, const juce::String& openPanel = {})
    {
        SimpleGainAudioProcessor processor;
        setGainDb (processor, db);

        SimpleGainAudioProcessorEditor editor (processor);
        editor.setSize (width, height);

        if (auto* button = dynamic_cast<juce::Button*> (editor.findChildWithID (openPanel)))
        {
            button->onClick();
            if (auto* guide = dynamic_cast<GuideView*> (editor.findChildWithID ("guide")); guide != nullptr && openPanel == "guideButton")
                guide->showNow (true);
            if (auto* website = dynamic_cast<WebsitePanel*> (editor.findChildWithID ("websitePanel")); website != nullptr && openPanel == "markButton")
                website->showNow (true);
        }

        return editor.createComponentSnapshot (editor.getLocalBounds(), true, scale);
    }

    /** Lays pictures out left to right on a white background, bottoms aligned. */
    juce::Image sideBySide (const std::vector<juce::Image>& images, int gap)
    {
        int totalWidth = gap, tallest = 0;
        for (const auto& image : images)
        {
            totalWidth += image.getWidth() + gap;
            tallest = std::max (tallest, image.getHeight());
        }

        juce::Image result (juce::Image::ARGB, totalWidth, tallest + 2 * gap, true);
        juce::Graphics g (result);
        g.fillAll (juce::Colours::white);

        int x = gap;
        for (const auto& image : images)
        {
            g.drawImageAt (image, x, gap + tallest - image.getHeight());
            x += image.getWidth() + gap;
        }

        return result;
    }

    bool savePng (const juce::Image& image, const juce::File& file)
    {
        file.deleteFile();
        juce::FileOutputStream stream (file);
        return stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream);
    }
}

bool writeEditorSnapshots (const juce::File& directory)
{
    directory.createDirectory();

    // Sizes come from the editor itself, so the pictures can't drift from the code.
    SimpleGainAudioProcessor processor;
    SimpleGainAudioProcessorEditor probe (processor);
    const auto* limits = probe.getConstrainer();
    const auto defaultWidth = probe.getWidth(), defaultHeight = probe.getHeight();

    bool ok = true;

    // One sharp picture at the opening size.
    ok &= savePng (snapshot (0.0f, defaultWidth, defaultHeight, 2.0f), directory.getChildFile ("editor-default.png"));

    // The same window quiet, in the amber, and in the red with the limit readout showing.
    ok &= savePng (sideBySide ({ snapshot (-30.0f, defaultWidth, defaultHeight, 1.0f),
                                 snapshot (20.0f,  defaultWidth, defaultHeight, 1.0f),
                                 snapshot (28.0f,  defaultWidth, defaultHeight, 1.0f) }, 24),
                  directory.getChildFile ("editor-values.png"));

    // The guide open, at the opening size.
    ok &= savePng (snapshot (0.0f, defaultWidth, defaultHeight, 2.0f, "guideButton"), directory.getChildFile ("editor-guide.png"));

    // The website panel open, at the opening size.
    ok &= savePng (snapshot (0.0f, defaultWidth, defaultHeight, 2.0f, "markButton"), directory.getChildFile ("editor-website.png"));

    // The smallest (and opening) size, 150%, and the largest the window can be dragged to.
    ok &= savePng (sideBySide ({ snapshot (0.0f, limits->getMinimumWidth(), limits->getMinimumHeight(), 1.0f),
                                 snapshot (0.0f, defaultWidth * 3 / 2, defaultHeight * 3 / 2, 1.0f),
                                 snapshot (0.0f, limits->getMaximumWidth(), limits->getMaximumHeight(), 1.0f) }, 24),
                  directory.getChildFile ("editor-sizes.png"));

    return ok;
}
