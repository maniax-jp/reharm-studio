#include <juce_core/juce_core.h>

// REHARM_AGENT_SANDBOX is defined by CMake whenever the build directory is not
// the canonical `build/`. The banner brands every line of output that matters,
// so a log excerpt from a sandbox run can never be mistaken for release
// verification.
#if REHARM_AGENT_SANDBOX
static const char* const sandboxNote =
    " [AGENT SANDBOX BUILD -- NOT release verification; run `make release-check`]";
#else
static const char* const sandboxNote = "";
#endif

int main (int argc, char* argv[])
{
#if REHARM_AGENT_SANDBOX
    std::cout << "================================================================\n"
                 " AGENT SANDBOX BUILD: results do NOT count as release\n"
                 " verification. Final checks must run in build/ via\n"
                 " `make release-check`.\n"
                 "================================================================"
              << std::endl;
#endif

    juce::UnitTestRunner runner;
    runner.runAllTests();

    int numFailures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        numFailures += runner.getResult (i)->failures;

    if (numFailures > 0)
    {
        std::cout << "Tests completed with " << numFailures << " failure(s)."
                  << sandboxNote << std::endl;
        return 1;
    }

    std::cout << "Tests completed. All passed." << sandboxNote << std::endl;
    return 0;
}
