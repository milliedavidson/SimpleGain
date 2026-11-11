// =============================================================================
//  EditorSnapshots.h — render the real editor to PNG files, with no window
//
//      SimpleGain_Tests --snapshots <folder>
//
//  Used to look at the GUI and to produce the pictures in documentation/.
// =============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

/** Writes the editor-*.png pictures into `directory`. Returns true if every file was written. */
bool writeEditorSnapshots (const juce::File& directory);
