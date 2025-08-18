/*
 * MeshPumpShell.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MESHPUMPSHELL_HXX
#define MESHPUMPSHELL_HXX

#include <MeshShell.hxx>

using namespace std;

class MeshPumpShell : public MeshShell {

public:

    MeshPumpShell(shared_ptr<MeshClient> client = NULL);
    ~MeshPumpShell();

protected:

    virtual int led(int argc, char **argv);
    virtual int unknown_command(int argc, char **argv);

};

#endif

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
