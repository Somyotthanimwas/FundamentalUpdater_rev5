#include "ProcessRunner.h"

#include <cstdlib>
#include <iostream>

bool ProcessRunner::Run(
    const std::string& command
)
{
    std::cout
        << "Running: "
        << command
        << std::endl;

    int result = std::system(command.c_str());

    std::cout
        << "Raw system result = "
        << result
        << std::endl;

    if (result == -1)
    {
        std::cerr
            << "System call failed"
            << std::endl;

        return false;
    }

    // Windows system() returns the command exit status directly.
    int exitCode = result;

    std::cout
        << "Exit code = "
        << exitCode
        << std::endl;

    if (exitCode == 0)
    {
        return true;
    }

    std::cerr
        << "Process failed. ExitCode = "
        << exitCode
        << std::endl;

    return false;
}