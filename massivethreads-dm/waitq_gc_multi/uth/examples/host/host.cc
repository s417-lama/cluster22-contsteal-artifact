#include <uth.h>

#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include <assert.h>

namespace uth = madm::uth;

void real_main(int argc, char **argv)
{
    uth::pid_t me = uth::get_pid();

    int hostlen;
    char hostname[MPI_MAX_PROCESSOR_NAME];
    MPI_Get_processor_name(hostname, &hostlen);

    printf("%5zu: hostname = %s\n", me, hostname);

    uth::barrier();
}

int main(int argc, char **argv)
{
    uth::start(real_main, argc, argv);
    return 0;
}

