#include "CppUTest/CommandLineTestRunner.h"

int main(int ac, char *av[])
{
    int exit_code = CommandLineTestRunner::RunAllTests(ac, av);
    return exit_code;
}
