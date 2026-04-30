#include "stdoutSilencer.h"
#if defined(_WIN32)

stdoutSilencer::stdoutSilencer() 
{
    // No-op on Windows
}

stdoutSilencer::~stdoutSilencer() 
{
    // No-op on Windows
}

#else
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>

stdoutSilencer::stdoutSilencer() 
{
    // Save original stdout file descriptor
    originalStdout = dup(STDOUT_FILENO);
    // Open /dev/null and redirect stdout
    int devNull = open("/dev/null", O_WRONLY);
    dup2(devNull, STDOUT_FILENO);
    close(devNull);
}

stdoutSilencer::~stdoutSilencer() {
    // Restore original stdout on destruction
    if (originalStdout != -1) {
        dup2(originalStdout, STDOUT_FILENO);
        close(originalStdout);
    }
}

#endif
