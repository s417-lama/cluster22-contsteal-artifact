#ifndef MADM_LOGGER_KIND_H
#define MADM_LOGGER_KIND_H

#include "madm/madm_comm_config.h"

namespace madi {

    class logger_kind {
    public:
        enum class kind {
            INIT = 0,

            WORKER_BUSY,
            WORKER_SCHED,
            WORKER_THREAD_FORK,
            WORKER_THREAD_DIE,
            WORKER_JOIN_RESOLVED,
            WORKER_JOIN_UNRESOLVED,
            WORKER_RESUME_PARENT,
            WORKER_RESUME_STOLEN,

            TASKQ_PUSH,
            TASKQ_POP,
            TASKQ_STEAL,
            TASKQ_EMPTY,

            FUTURE_POOL_SYNC,
            FUTURE_POOL_FILL,
            FUTURE_POOL_GET,

            COMM_PUT,
            COMM_GET,
            COMM_FENCE,
            COMM_FETCH_AND_ADD,
            COMM_TRYLOCK,
            COMM_LOCK,
            COMM_UNLOCK,
            COMM_POLL,
            COMM_MALLOC,
            COMM_FREE,

            OTHER,
            __N_KINDS,
        };

        static constexpr bool kind_included(kind k, kind kinds[], int n) {
            return n > 0 && (k == *kinds || kind_included(k, kinds + 1, n - 1));
        }

        static constexpr bool is_valid_kind(kind k) {
            kind enabled_kinds[] = MADI_LOGGER_ENABLED_KINDS;
            kind disabled_kinds[] = MADI_LOGGER_DISABLED_KINDS;

            static_assert(!(sizeof(enabled_kinds) > 0 && sizeof(disabled_kinds) > 0),
                          "Enabled kinds and disabled kinds cannot be specified at the same time.");

            if (sizeof(enabled_kinds) > 0) {
                return kind_included(k, enabled_kinds, sizeof(enabled_kinds) / sizeof(*enabled_kinds));
            } else if (sizeof(disabled_kinds) > 0) {
                return !kind_included(k, disabled_kinds, sizeof(disabled_kinds) / sizeof(*disabled_kinds));
            } else {
                return true;
            }
        }

        static constexpr const char* kind_name(kind k) {
            switch (k) {
                case kind::INIT:                    return "";

                case kind::WORKER_BUSY:             return "worker_busy";
                case kind::WORKER_SCHED:            return "worker_sched";
                case kind::WORKER_THREAD_FORK:      return "worker_thread_fork";
                case kind::WORKER_THREAD_DIE:       return "worker_thread_die";
                case kind::WORKER_JOIN_RESOLVED:    return "worker_join_resolved";
                case kind::WORKER_JOIN_UNRESOLVED:  return "worker_join_unresolved";
                case kind::WORKER_RESUME_PARENT:    return "worker_resume_parent";
                case kind::WORKER_RESUME_STOLEN:    return "worker_resume_stolen";

                case kind::TASKQ_PUSH:              return "taskq_push";
                case kind::TASKQ_POP:               return "taskq_pop";
                case kind::TASKQ_STEAL:             return "taskq_steal";
                case kind::TASKQ_EMPTY:             return "taskq_empty";

                case kind::FUTURE_POOL_SYNC:        return "future_pool_sync";
                case kind::FUTURE_POOL_FILL:        return "future_pool_fill";
                case kind::FUTURE_POOL_GET:         return "future_pool_get";

                case kind::COMM_PUT:                return "comm_put";
                case kind::COMM_GET:                return "comm_get";
                case kind::COMM_FENCE:              return "comm_fence";
                case kind::COMM_FETCH_AND_ADD:      return "comm_fetch_and_add";
                case kind::COMM_TRYLOCK:            return "comm_trylock";
                case kind::COMM_LOCK:               return "comm_lock";
                case kind::COMM_UNLOCK:             return "comm_unlock";
                case kind::COMM_POLL:               return "comm_poll";
                case kind::COMM_MALLOC:             return "comm_malloc";
                case kind::COMM_FREE:               return "comm_free";

                default:                            return "other";
            }
        }
    };

}

#endif
