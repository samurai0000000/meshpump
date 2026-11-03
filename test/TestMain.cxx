/*
 * test/TestMain.cxx
 *
 * Copyright (C) 2026, Charles Chiou
 */

#include <CppUTest/CommandLineTestRunner.h>
#include <CppUTest/MemoryLeakWarningPlugin.h>

int main(int ac, char **av)
{
    MemoryLeakWarningPlugin::turnOffNewDeleteOverloads();
    return CommandLineTestRunner::RunAllTests(ac, av);
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
