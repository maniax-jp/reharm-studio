#include <juce_core/juce_core.h>

int main (int argc, char* argv[])
{
    juce::UnitTestRunner runner;
    runner.runAllTests();

    int numFailures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        numFailures += runner.getResult (i)->failures;

    if (numFailures > 0)
    {
        std::cout << "Tests completed with " << numFailures << " failure(s)." << std::endl;
        return 1;
    }

    std::cout << "Tests completed. All passed." << std::endl;
    return 0;
}
