// =============================================================================
//  main.cpp — runs every registered test and reports the result
//
//  Each test file defines a UnitTest object at namespace scope; constructing it
//  registers it with JUCE, so there's no list to maintain here.
// =============================================================================
#include <juce_audio_processors/juce_audio_processors.h>
#include "EditorSnapshots.h"

#include <iostream>

namespace
{
    /** JUCE reports a debug assertion (a broken rule inside JUCE or our code) by logging it, which on its
        own fails nothing. This counts them so the run can fail: a use-after-free or a widget deleted while
        still in use shows up here even when every check passes. */
    struct AssertionCounter final : juce::Logger
    {
        int count = 0;

    protected:
        void logMessage (const juce::String& message) override
        {
            if (message.startsWith ("JUCE Assertion failure"))
                ++count;

            std::cout << message << std::endl;
        }
    };

    /** JUCE's runner logs through its own Logger; send it to the terminal so ctest and CI show it. */
    struct ConsoleRunner final : juce::UnitTestRunner
    {
    protected:
        void logMessage (const juce::String& message) override  { std::cout << message << std::endl; }
    };
}

int main (int argc, char** argv)
{
    // The processor builds parameter trees and listeners that expect JUCE to be running.
    juce::ScopedJuceInitialiser_GUI initialiser;

    // SimpleGain_Tests --snapshots <folder>  draws the editor to PNG files instead of running tests.
    if (argc > 2 && juce::String (argv[1]) == "--snapshots")
        return writeEditorSnapshots (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2])) ? 0 : 1;

    AssertionCounter assertions;
    juce::Logger::setCurrentLogger (&assertions);

    ConsoleRunner runner;

    // No argument runs everything; a name runs one group, e.g.  SimpleGain_Tests "State save and restore"
    if (argc > 1)
        runner.runTestsWithName (argv[1]);
    else
        runner.runAllTests();

    int passes = 0, failures = 0;

    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        passes   += runner.getResult (i)->passes;
        failures += runner.getResult (i)->failures;
    }

    juce::Logger::setCurrentLogger (nullptr);

    std::cout << "\n" << passes << " checks passed, " << failures << " failed, "
              << assertions.count << " JUCE assertions" << std::endl;

    // No results at all would mean the tests silently didn't register — treat that as a failure too.
    return (failures == 0 && passes > 0 && assertions.count == 0) ? 0 : 1;
}
