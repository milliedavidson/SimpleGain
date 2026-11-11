// =============================================================================
//  main.cpp — runs every registered test and reports the result
//
//  Each test file defines a UnitTest object at namespace scope; constructing it
//  registers it with JUCE, so there's no list to maintain here.
// =============================================================================
#include <juce_audio_processors/juce_audio_processors.h>

#include <iostream>

namespace
{
    /** JUCE's runner logs through its own Logger; send it to the terminal so ctest and CI show it. */
    struct ConsoleRunner final : public juce::UnitTestRunner
    {
        void logMessage (const juce::String& message) override  { std::cout << message << std::endl; }
    };
}

int main (int argc, char** argv)
{
    // The processor builds parameter trees and listeners that expect JUCE to be running.
    juce::ScopedJuceInitialiser_GUI initialiser;

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

    std::cout << "\n" << passes << " checks passed, " << failures << " failed" << std::endl;

    // No results at all would mean the tests silently didn't register — treat that as a failure too.
    return (failures == 0 && passes > 0) ? 0 : 1;
}
