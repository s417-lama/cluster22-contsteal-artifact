#include "comm_system.h"
#include "comm_base.h"
#include "collectives-inl.h"

namespace madi {
namespace comm {

    // template instantiation for comm_system class
    template class collectives<comm_base>;

    template void collectives<comm_base>::broadcast(int *, size_t, pid_t);
    template void collectives<comm_base>::broadcast(unsigned int *, size_t, pid_t);
    template void collectives<comm_base>::broadcast(long *, size_t, pid_t);
    template void collectives<comm_base>::broadcast(unsigned long *, size_t, pid_t);

    template void collectives<comm_base>::reduce(
                        int *, const int [], size_t, pid_t, reduce_op);
    template void collectives<comm_base>::reduce(
                        unsigned int *, const unsigned int [], size_t,
                        pid_t, reduce_op);
    template void collectives<comm_base>::reduce(
                        long *, const long [], size_t, pid_t, reduce_op);
    template void collectives<comm_base>::reduce(
                        unsigned long*, const unsigned long[], size_t,
                        pid_t, reduce_op);

}
}

