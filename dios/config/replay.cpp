#include <dios/config/context.hpp>
#include <dios/proxy/replay.h>

namespace __dios
{
    using Context = Upcall< Fault< Scheduler < fs::Replay < Base > > > >;
}

#include <dios/config/common.hpp>
