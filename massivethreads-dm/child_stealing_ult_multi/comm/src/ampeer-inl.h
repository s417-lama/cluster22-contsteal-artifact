#ifndef MADI_AMPEER_INL_H
#define MADI_AMPEER_INL_H

#include "ampeer.h"
#include "fetch_and_add.h"

#include <deque>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <climits>

#include <mpi-ext.h>

// disable sendbuf pool is unsafe
#define MADI_COMM_SENDBUF_POOL   0

bool ampeer_prof_enabled = false;

namespace madi {
namespace comm {

    ampeer_prof *g_amprof = NULL;

    int native_of_compute_pid(int pid);
    int compute_of_native_pid(int pid);

    template <class CB>
    ampeer<CB>::ampeer(CB& c, amhandler_t handler, int server_mod,
                       size_t n_max_sends, int fjmpi_tag_base)
        : me_(c.native_config().get_pid())
        , handler_(handler)
        , server_mod_(server_mod)
        , ambuf_size_(128)
        , sendbufs_(NULL), recvbufs_(NULL), remotebufs_(NULL)
        , pendings_()
        , c_(c)
        , fjmpi_tag_base_(fjmpi_tag_base)
        , ambufs_(NULL), server_addrs_(NULL)
    {
        process_config& config = c.native_config();
        int n_procs = config.get_n_procs();

        // allocate send/recv buffers
        size_t ambufs_size = ambuf_size_ * n_max_sends * 2;
        ambufs_ = (uint8_t *)c_.malloc(ambufs_size, config);

        memset(ambufs_, 0, ambufs_size);

        // initialize local buffer pools
        sendbufs_ = new buffer_pool(ambufs_, ambuf_size_, n_max_sends);
        recvbufs_ = new buffer_pool(ambufs_ + ambuf_size_ * n_max_sends,
                                    ambuf_size_, n_max_sends);

        for (size_t i = 0; i < n_max_sends; i++) {
            uint8_t *p = recvbufs_->bufptr(i);
            clear_recvbuf(p);
        }

        // allocate server buffers
        size_t serverbuf_size = ambuf_size_ * n_procs;

        // FIXME: current coll_malloc parameter must be save on all processes
//        size_t remotebuf_size = is_server(me_) ? serverbuf_size : 0;
        size_t remotebuf_size = serverbuf_size;

        server_addrs_ = (uint8_t **)c_.coll_malloc(remotebuf_size, config);

        memset(server_addrs_[me_], 0, remotebuf_size);

        // initialize server buffer pools
        remotebufs_ = new remote_buffer_pool(server_addrs_, ambuf_size_,
                                             config);

        // touch OS pages
        size_t touch_elems = 1024; 
        pendings_.reserve(touch_elems);
        memset(pendings_.data(), 0, sizeof(ampending<CB>) * touch_elems);

        g_amprof = new ampeer_prof;
    }

    template <class CB>
    ampeer<CB>::~ampeer()
    {
        g_amprof->output();
        delete g_amprof;

        delete sendbufs_;
        delete recvbufs_;
        delete remotebufs_;

        process_config& config = c_.native_config();
        c_.free((void *)ambufs_, config);
        c_.coll_free((void **)server_addrs_, config);
    }

    template <class CB>
    bool ampeer<CB>::is_server(int native_pid)
    { return native_pid % server_mod_ == 0; }

    template <class CB>
    int ampeer<CB>::server_pid(size_t native_pid)
    { return native_pid / server_mod_ * server_mod_; }

    template <class CB>
    int ampeer<CB>::sendbuf_id_of_tag(int tag)
    { return tag - fjmpi_tag_base_; }

    template <class CB>
    int ampeer<CB>::tag_of_sendbuf_id(int id)
    { return fjmpi_tag_base_ + id;}

    template <class CB>
    amheader& ampeer<CB>::get_amheader(uint8_t *ambuf)
    {
        uint8_t *pheader = ambuf + ambuf_size_ - sizeof(amheader);
        return *(amheader *)pheader;
    }

    uint8_t * get_amdataptr(amheader& h)
    {
        return (uint8_t *)&h - h.size;
    }

    template <class CB>
    size_t ampeer<CB>::fill_sendbuf(uint8_t *buf, uint8_t *replybuf, 
                                    int tag, void *p, size_t size)
    {
        MADI_CHECK(size <= UINT16_MAX);
        MADI_CHECK(size > 0);

        amheader& header = get_amheader(buf);

        header.replybuf = replybuf;
        header.initiator = me_;
        header.tag = (uint16_t)tag;
        header.size = (uint16_t)size;

        uint8_t *data = get_amdataptr(header);
        memcpy(data, p, size);

        size_t total_size = sizeof(amheader) + size;
        return ambuf_size_ - total_size;
    }

    template <class CB>
    bool ampeer<CB>::is_recvbuf_filled(uint8_t *buf)
    {
        return get_amheader(buf).size > 0;
    }

    template <class CB>
    void ampeer<CB>::clear_recvbuf(uint8_t *buf)
    {
        amheader& h = get_amheader(buf);

        h.size = 0;
    }
   
    template <class CB>
    void ampeer<CB>::do_request_nbi(int tag, void *p, size_t size, int pid,
                                    MADI_UNUSED const aminfo *info)
    {
        g_amprof->do_request_nbi.begin();

        int server = server_pid(pid);

        // already checked in send_with_pool
        MADI_ASSERT(!sendbufs_->empty());
        MADI_ASSERT(!remotebufs_->empty(server));

        // assumption: # of free sendbufs <= # of free recvbufs
        MADI_ASSERT(!recvbufs_->empty());

        size_t send_size = sizeof(amheader) + size;
        MADI_ASSERT(send_size <= ambuf_size_);

        // get local send buffer
        int sendbuf_id = 0;
        uint8_t *sendbuf = NULL;
        sendbufs_->pop(&sendbuf_id, &sendbuf);

        // get local recv buffer (for reply)
        int recvbuf_id = 0;
        uint8_t *recvbuf = NULL;
        recvbufs_->pop(&recvbuf_id, &recvbuf);

        memset(recvbuf, 0, ambuf_size_);

        // get remote recv buffer
        uint8_t *remotebuf = NULL;
        remotebufs_->pop(server, &remotebuf);

        // copy data to the local RDMA buffer
        size_t offset = fill_sendbuf(sendbuf, recvbuf, tag, p, size);

        // set send parameters
        int fjmpi_tag = tag_of_sendbuf_id(sendbuf_id);

        // send message
        c_.raw_put_with_notice(fjmpi_tag,
                               remotebuf + offset,
                               sendbuf + offset,
                               send_size, server, me_);

#if !MADI_COMM_SENDBUF_POOL
        // FIXME: unsafe?
        sendbufs_->push(sendbuf_id);
#endif

        g_amprof->do_request_nbi.end();
    }

    template <class CB>
    void ampeer<CB>::do_reply(int tag, void *p, size_t size, int pid,
                              const aminfo *info)
    {
        g_amprof->do_reply.begin();

        MADI_ASSERT(info != NULL);
        MADI_ASSERT(size != 0);

        MADI_ASSERT(!sendbufs_->empty());

        size_t send_size = sizeof(amheader) + size;
        MADI_ASSERT(send_size <= ambuf_size_);

        // get local send buffer
        int sendbuf_id = 0;
        uint8_t *sendbuf = NULL;
        sendbufs_->pop(&sendbuf_id, &sendbuf);

        // calculate remote buffer address
        uint8_t *remotebuf = info->replybuf;
        
        MADI_ASSERT(remotebuf != NULL);

        // as a flag distinguishing user reply or system reply
        uint8_t *replybuf = (info->replied) ? NULL : info->replybuf;

        g_amprof->fill_repbuf.begin();

        // copy data to the local RDMA buffer
        size_t offset = fill_sendbuf(sendbuf, replybuf, tag, p, size);

        g_amprof->fill_repbuf.end();

        // set send parameters
        int fjmpi_tag = tag_of_sendbuf_id(sendbuf_id);

#if 1
#define PUT c_.raw_put_ordered
#else
#define PUT c_.raw_put
#endif
        g_amprof->reply_put.begin();

        // send message
        PUT(fjmpi_tag, remotebuf + offset, sendbuf + offset,
            send_size, pid, me_);
        
        g_amprof->reply_put.end();
#undef PUT

#if !MADI_COMM_SENDBUF_POOL
        // FIXME: unsafe?
        sendbufs_->push(sendbuf_id);
#endif

        g_amprof->do_reply.end();
    }

    template <class CB>
    void ampeer<CB>::send_with_pool(typename ampeer_do_send<CB>::type f,
                                    int tag, void *p, size_t size, int pid,
                                    const aminfo *info, process_config& config)
    {
        int native_pid = config.native_pid(pid);
        bool sendbuf_ok = !sendbufs_->empty();
        bool remotebuf_ok = (f == &ampeer<CB>::do_reply)
                         || !remotebufs_->empty(native_pid);

        if (!sendbuf_ok)
            MADI_DPUTSR("caution: msg is pended because of sendbuf overflow");

        if (!remotebuf_ok)
            MADI_DPUTSR("caution: msg is pended because of remotebuf overflow");
 
        if (sendbuf_ok && remotebuf_ok) {
            (this->*f)(tag, p, size, native_pid, info);
        } else {
            ampending<CB> req;

            req.do_send = f;
            req.tag = tag;
            req.p = p;
            req.size = size;
            req.pid = native_pid;

            if (info != NULL)
                req.info = *info;

            pendings_.push_back(req);
        }
    }

    template <class CB>
    void ampeer<CB>::request_nbi(int tag, void *p, size_t size, int pid,
                                 process_config& config)
    {
        send_with_pool(&ampeer<CB>::do_request_nbi, tag, p, size, pid,
                       NULL, config);
    }

    template <class CB>
    void ampeer<CB>::amfence()
    {
        if (!sendbufs_->filled()) {
            g_amprof->fence.begin();

            while (!sendbufs_->filled())
                madi::comm::poll();  // internally calls ampeer<CB>::handle

            g_amprof->fence.end();
        }
    }

    template <class CB>
    void ampeer<CB>::request(int tag, void *p, size_t size, int pid,
                             process_config& config)
    {
        request_nbi(tag, p, size, pid, config);
        fence();
    }

    template <class CB>
    void ampeer<CB>::reply(int tag, void *p, size_t size, aminfo *info,
                           process_config& config)
    {
        g_amprof->reply.begin();

        send_with_pool(&ampeer<CB>::do_reply, tag, p, size, info->initiator,
                       info, config);
        fence();
        info->replied = true;
        
        g_amprof->reply.end();
    }
    
    namespace {
        bool amhandle_default(int tag, int pid, void *data, size_t size,
                              aminfo *info)
        {
            switch (tag) {
                case AM_FETCH_AND_ADD_INT_REQ:
                    fetch_and_add_req<int>::amhandle(data, size, pid, info,
                                                   AM_FETCH_AND_ADD_INT_REP);
                    return true;
                case AM_FETCH_AND_ADD_INT_REP:
                    fetch_and_add_rep<int>::amhandle(data, size, pid, info);
                    return true;
                case AM_FETCH_AND_ADD_LONG_REQ:
                    fetch_and_add_req<long>::amhandle(data, size, pid, info,
                                                   AM_FETCH_AND_ADD_LONG_REP);
                    return true;
                case AM_FETCH_AND_ADD_LONG_REP:
                    fetch_and_add_rep<long>::amhandle(data, size, pid, info);
                    return true;
                case AM_FETCH_AND_ADD_ULONG_REQ:
                    fetch_and_add_req<unsigned long>::amhandle(data, size,
                                                               pid, info,
                                                   AM_FETCH_AND_ADD_ULONG_REP);
                    return true;
                case AM_FETCH_AND_ADD_ULONG_REP:
                    fetch_and_add_rep<unsigned long>::amhandle(data, size,
                                                               pid, info);
                    return true;
                default:
                    MADI_NOT_REACHED;
                    return true;
            }

            return false;
        }
    }

    template <class CB>
    void ampeer<CB>::call_handler(int tag, int pid, void *p, size_t size, 
                                  aminfo *info)
    {
        bool result = amhandle_default(tag, pid, p, size, info);

        if (!result && handler_ != nullptr)
            handler_(tag, pid, p, size, info);
    }

    template <class CB>
    void ampeer<CB>::handle_request(int pid, process_config& config)
    {
        g_amprof->handle_request.begin();

        uint8_t *recvbuf = remotebufs_->recvbufptr(pid);
        amheader& h = get_amheader(recvbuf);
        uint8_t *data = get_amdataptr(h);
        aminfo info = { (int)h.initiator, h.replybuf, false };

        int abst_pid = config.abstract_pid(h.initiator);

        MADI_ASSERT(pid == (int)h.initiator);

        call_handler(h.tag, abst_pid, data, h.size, &info);

        // FIXME
        MADI_CHECK(info.replied);

        if (!info.replied) {
            // implicit reply for notifying remotebuf release to 
            // the initiator
            reply(AM_IMPLICIT_REPLY, NULL, 0, &info, config);
        }

        g_amprof->handle_request.end();
    }

    template <class PendingList, class CB>
    bool pop_sendable_by_pid(PendingList& pendings, int pid,
                             ampending<CB> *preq)
    {
        typename PendingList::iterator it = pendings.begin();
        for (; it != pendings.end(); ++it) {
            ampending<CB>& req = *it;

            if (req.pid == pid) {
                // pop it
                *preq = *it;
                pendings.erase(it);

                return true;
            }
        }

        return false;
    }

    template <class CB>
    void ampeer<CB>::handle_reply(int replybuf_id, uint8_t *recvbuf,
                                  process_config& config)
    {
        g_amprof->handle_reply.begin();

        amheader& h = get_amheader(recvbuf);

        int pid = h.initiator;

        if (h.replybuf != NULL) { // FIXME
            // reply sent by user
            int abst_pid = -1; // FIXME: config.abstract_pid(pid);
            void *data = get_amdataptr(h);
            call_handler(h.tag, abst_pid, data, h.size, NULL);
        }

        clear_recvbuf(recvbuf);
        recvbufs_->push(replybuf_id);

        remotebufs_->push(pid);

        if (!pendings_.empty() && !sendbufs_->empty()) {
            ampending<CB> req;
            bool found = pop_sendable_by_pid(pendings_, pid, &req);

            if (found) {
                // send the sendable ampending
                (this->*req.do_send)(req.tag, req.p, req.size, req.pid,
                                     &req.info);
            }
        }

        g_amprof->handle_reply.end();
    }

    template <class PendingList, class CB>
    bool pop_sendable(PendingList& pendings,
                      const remote_buffer_pool *remotebufs,
                      ampending<CB> *preq)
    {
        typename PendingList::iterator it = pendings.begin();
        for (; it != pendings.end(); ++it) {
            ampending<CB>& req = *it;

            if (!remotebufs->empty(req.pid)) {
                // pop it
                *preq = *it;
                pendings.erase(it);

                return true;
            }
        }

        return false;
    }

    template <class CB>
    void ampeer<CB>::handle_local_completion(int sendbuf_id)
    {
#if MADI_COMM_SENDBUF_POOL
        // deactivate send buffer
        sendbufs_->push(sendbuf_id);
#endif

        if (!pendings_.empty()) {

            ampending<CB> req;
            bool found = pop_sendable(pendings_, remotebufs_, &req);

            if (found) {
                MADI_DPUTSR2("caution: a pended msg is sending");

                // send the sendable ampending
                (this->*req.do_send)(req.tag, req.p, req.size, req.pid,
                                     &req.info);
            }
        }
    }

    template <class CB>
    bool ampeer<CB>::amhandle(int r, int tag, int pid, process_config& config)
    {
        int sendbuf_id = sendbuf_id_of_tag(tag);

        if (!sendbufs_->valid(sendbuf_id))
            return false;

        if (r == FJMPI_RDMA_HALFWAY_NOTICE) {
            MADI_ASSERT(is_server(me_));
            handle_request(pid, config);
            return true;
        } 
        
        if (r == FJMPI_RDMA_NOTICE) {
            handle_local_completion(sendbuf_id);
            return true;
        }

        MADI_NOT_REACHED;
    }

    template <class CB>
    void ampeer<CB>::ampoll(process_config& config)
    {
        if (!is_server(me_)) {
            // check done flags of all recv buffers
            int id = recvbufs_->from();
            for (; id < recvbufs_->to(); id++) {
                uint8_t *recvbuf = recvbufs_->bufptr(id);

                if (is_recvbuf_filled(recvbuf))
                    handle_reply(id, recvbuf, config);
            }
        }
    }

    template <class T> struct fetch_and_add_tag;
    template <> struct fetch_and_add_tag<int>
    { enum { value = AM_FETCH_AND_ADD_INT_REQ }; };
    template <> struct fetch_and_add_tag<long>
    { enum { value = AM_FETCH_AND_ADD_LONG_REQ }; };
    template <> struct fetch_and_add_tag<unsigned long>
    { enum { value = AM_FETCH_AND_ADD_ULONG_REQ }; };

    template <class CB>
    template <class T>
    T ampeer<CB>::fetch_and_add(T *dst, T value, int target,
                                process_config& config)
    {
        fetch_and_add_req<T> req(dst, value);

        int tag = fetch_and_add_tag<T>::value;
        req.request(tag, target);

        T result;
        while (!req.test(&result))
            madi::comm::poll();

        return result;
    }

}
}

#endif
