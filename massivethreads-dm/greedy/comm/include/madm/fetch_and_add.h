#ifndef MADI_COMM_FETCH_AND_ADD_H
#define MADI_COMM_FETCH_AND_ADD_H

#include "madm_comm-decls.h"
#include "ampeer.h"
#include "threadsafe.h"

namespace madi {
namespace comm {

    template <class T>
    class fetch_and_add_sync {
        bool done_;
        T result_;

    public:
        fetch_and_add_sync() : done_(false), result_(0) {}

        void fill(T result)
        {
            MADI_DPUTSR3("AM_FETCH_AND_ADD FILL: sync=%p,done=%d,res=%ld",
                         this, (int)done_, (long)result_);

            result_ = result;
            threadsafe::wbarrier();
            done_ = true;
        }

        bool try_get(T *result)
        {
            if (!done_)
                return false;

            threadsafe::rbarrier();

            MADI_DPUTSR3("AM_FETCH_AND_ADD DONE: sync=%p,done=%d,res=%ld",
                         this, (int)done_, (long)result_);

            *result = result_;
            return true;
        }
    };

    template <class T>
    class fetch_and_add_rep {
    public:
        T result_;
        fetch_and_add_sync<T> *sync_;

    public:
        fetch_and_add_rep(T result, fetch_and_add_sync<T> *sync) :
            result_(result), sync_(sync) {}

        static void amhandle(void *data, size_t size, int pid, aminfo *info)
        {
            const fetch_and_add_rep<T>& rep = *(fetch_and_add_rep<T> *)data;

            MADI_DPUTSR3("AM_FETCH_AND_ADD REP: sync=%p,res=%ld,pid=%d",
                         rep.sync_, (long)rep.result_, pid);

            rep.sync_->fill(rep.result_);

            MADI_DPUTSR3("AM_FETCH_AND_ADD REP: DONE");
        }
    };

    template <class T>
    class fetch_and_add_req {

        struct packet {
            T *p;
            T value;
            fetch_and_add_sync<T> *sync_ptr;

            packet(T *ptr, T v, fetch_and_add_sync<T> *sp) :
                p(ptr), value(v), sync_ptr(sp) {}
        };

        fetch_and_add_sync<T> sync_;
        packet packet_;

    public:
        fetch_and_add_req(T *p, T value) :
            sync_(), packet_(p, value, &sync_) {}

        void request(int tag, int target)
        {
            MADI_DPUTSR3("AM_FETCH_AND_ADD START: sync=%p", &sync_);

            amrequest(tag, &packet_, sizeof(packet_), target);
        }

        bool test(T *result)
        {
            return sync_.try_get(result);
        }

        T wait()
        {
            T result;

            while (!test(&result))
                madi::comm::poll();

            return result;
        }

        static void amhandle(void *data, size_t size, int pid, aminfo *info,
                             int rep_tag)
        {
            const packet& req = *(packet *)data;

            MADI_DPUTSR3("AM_FETCH_AND_ADD REQ: p=%p,val=%d,pid=%d",
                         req.p, req.value, pid);

            T result = threadsafe::fetch_and_add(req.p, req.value);

            fetch_and_add_rep<T> rep(result, req.sync_ptr);

            amreply(rep_tag, &rep, sizeof(rep), info);

            MADI_DPUTSR3("AM_FETCH_AND_ADD REQ: DONE");
        }
    };

}
}

#endif
