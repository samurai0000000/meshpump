/*
 * MeshPumpShell.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <string>
#include <LedMatrix.hxx>
#include <MeshPumpShell.hxx>

extern shared_ptr<LedMatrix> ledMatrix;

MeshPumpShell::MeshPumpShell(shared_ptr<MeshClient> client)
    : MeshShell(client)
{
    _help_list.push_back("led");
}

MeshPumpShell::~MeshPumpShell()
{

}

int MeshPumpShell::led(int argc, char **argv)
{
    int ret = 0;
    string message;
    int startArg = 1;
    unsigned int layer = 0;

    if (argc < 2) {
        goto done;
    }

    if ((argc > 2) && strcmp(argv[1], "0") == 0) {
        startArg = 2;
        layer = 0;
    } else if ((argc > 2) && strcmp(argv[1], "1") == 0) {
        startArg = 2;
        layer = 1;
    } else if ((argc > 2) && strcmp(argv[1], "2") == 0) {
        startArg = 2;
        layer = 2;
    } else if ((argc > 2) && strcmp(argv[1], "3") == 0) {
        startArg = 2;
        layer = 3;
    }

    for (int i = startArg; i < argc; i++) {
        if (i > startArg) {
            message += " ";
        }
        message += argv[i];
    }

    if (message == "\"\"") {
        message = "";  // This is a temporary hack
    }

    if (ledMatrix) {
        ledMatrix->setText(layer, message);
    }

done:

    return ret;
}

int MeshPumpShell::unknown_command(int argc, char **argv)
{
    int ret = 0;

    if (strcmp(argv[0], "led") == 0) {
        ret = this->led(argc, argv);
    } else {
        ret = MeshShell::unknown_command(argc, argv);
    }

    return ret;
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
