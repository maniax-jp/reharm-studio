#include <juce_core/juce_core.h>

int main (int argc, char* argv[])
{
    juce::UnitTestRunner runner;
    runner.runAllTests();
    
    std::cout << "Tests completed." << std::endl;
    return 0;
}
