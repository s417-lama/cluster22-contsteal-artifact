#include "shmem/comm_memory.h"

#include "options.h"
#include "sys.h"
#include "madm_debug.h"
#include <cstring>
#include <climits>
#include <cerrno>

#if 0
#define MADI_SHM_DPUTS(fmt, ...) MADI_DPUTS(fmt, ##__VA_ARGS__)
#else
#define MADI_SHM_DPUTS(...)
#endif

namespace madi {
namespace comm {

    constexpr int comm_memory::MEMID_DEFAULT;

    // base address of default shared address space
#define CM_BASE_ADDR  (reinterpret_cast<uint8_t *>(0x30000000000))

    // # of max shared memory processes
    constexpr size_t CM_SHMPROC_BITS = 8;
    constexpr size_t CM_SHMPROC_SIZE = 1UL << CM_SHMPROC_BITS;

    // initial shared address space size for a process
    constexpr size_t CM_INIT_BITS = 22;
    constexpr size_t CM_INIT_SIZE = 1UL << CM_INIT_BITS;

    // max size of default shared address space
    constexpr size_t CM_MAX_BITS = 32;
    constexpr size_t CM_MAX_SIZE = 1UL << CM_MAX_BITS;


    int open_shared_file(int memid, int idx, bool creat)
    {
        MADI_ASSERT(memid >= 0);
        MADI_ASSERT(idx >= 0);

        int mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH;
        int oflag = O_RDWR;

        if (creat)
            oflag |= O_CREAT;

        char fname[NAME_MAX];
        sprintf(fname, "/massivethreadsdm.%d.%d", memid, idx);

        if (creat) {
            int r = shm_unlink(fname);

            if (r != 0 && errno != ENOENT)
                MADI_PERR_DIE("shm_unlink");
        }

        MADI_SHM_DPUTS("shm_open(%s, ...)", fname);

        int fd = shm_open(fname, oflag, mode);

        MADI_SHM_DPUTS("=> shm_open(%s, ...) = %d", fname, fd);

        if (fd < 0)
            MADI_PERR_DIE("shm_open");

        return fd;
    }

    void close_shared_file(int fd, bool unlink, int memid, int idx)
    {
        MADI_ASSERT(memid >= 0);
        MADI_ASSERT(idx >= 0);

        MADI_SHM_DPUTS("close(%d)", fd);

        close(fd);

        if (unlink) {
            char fname[NAME_MAX];
            sprintf(fname, "/massivethreadsdm.%d.%d", memid, idx);

            MADI_SHM_DPUTS("shm_unlink(%s)", fname);

            int r = shm_unlink(fname);

            if (r != 0)
                MADI_PERR_DIE("shm_unlink");
        }
    }

    void coll_shm_map::extend_shmem_region(int fd, uint8_t *addr, size_t offset,
                                           size_t size, int idx)
    {
        MADI_SHM_DPUTS("ftruncate(%d, %zu)", fd, offset + size);

        int r = ftruncate(fd, offset + size);

        if (r != 0)
            MADI_PERR_DIE("ftruncate");
    }

    void touch_pages(void *p, size_t size)
    {
        uint8_t *array = reinterpret_cast<uint8_t *>(p);
        size_t page_size = options.page_size;

        for (size_t i = 0; i < size; i += page_size)
            array[i] = 0;
    }

    uint8_t * do_mmap(uint8_t *addr, size_t size, int fd, size_t offset)
    {
        size_t page_size = options.page_size;
        size_t check_page_addr = ((uintptr_t)addr % page_size == 0);
        size_t check_page_size = (size % page_size == 0);
        MADI_CHECK(check_page_addr);
        MADI_CHECK(check_page_size);

        MADI_SHM_DPUTS("do_mmap(%p, %zu, %d, %zu)", addr, size, fd, offset);

        int prot = PROT_READ | PROT_WRITE;

        int flags = (fd == -1) ? (MAP_PRIVATE | MAP_ANONYMOUS) : MAP_SHARED;

#if 0
        if (addr != NULL)
            flags |= MAP_FIXED;
#endif

        void *p = mmap(addr, size, prot, flags, fd, offset);

        if (p == MAP_FAILED) {
            MADI_DIE("`mmap(%p, %zu, 0x%x, 0x%x, %d, %zu)' failed with `%s'",
                     addr, size, prot, flags, fd, offset,
                     strerror(errno));
        }
        if (addr != NULL && p != addr) {
            MADI_DPUTS("p = %p, addr = %p, size = %zu", p, addr, size);
            MADI_CHECK(p == addr);
        }

        touch_pages(p, size);

        return reinterpret_cast<uint8_t *>(p);
    }


    coll_shm_map::coll_shm_map(int memid,
                               const std::vector<uint8_t *>& addrs,
                               size_t size, address_type type,
                               process_config& config)
        : memid_(memid)
        , shm_maps_(config.get_native_n_procs())
        , size_(0)
        , addr_type_(type)
        , config_(config)
    {
        MADI_ASSERT(shm_maps_.size() == addrs.size());

        auto me = config.get_native_pid();

        auto& map = shm_maps_[me];
        map.addr  = addrs[me];
        map.fd    = open_shared_file(memid_, me, true);

        config.barrier();

        for (auto i = 0; i < shm_maps_.size(); i++) {
            if (i != me) {
                auto& map = shm_maps_[i];
                auto addr = addrs[i];

                map.addr = addr;
                map.fd   = open_shared_file(memid_, i, false);
            }
        }

        if (size > 0)
            extend_to(size);
    }

    coll_shm_map::coll_shm_map(int memid, uint8_t *addr, size_t size,
                               process_config& config)
        : coll_shm_map(memid,
                       [&]{ size_t me = config.get_native_pid();
                            size_t n_procs = config.get_native_n_procs();
                            std::vector<uint8_t *> addrs(n_procs, nullptr);
                            addrs[me] = addr;
                            return addrs; }(),
                       size, address_type::different, config)
    {
    }

    coll_shm_map::~coll_shm_map()
    {
        auto me = config_.get_native_pid();

        int id = 0;
        for (auto& map : shm_maps_) {

            munmap(map.addr, size_);

            bool unlink = (id == me);
            close_shared_file(map.fd, unlink, memid_, id);

            id++;
        }
    }

    uint8_t * coll_shm_map::address(size_t idx) const
    {
        return shm_maps_[idx].addr;
    }

    size_t coll_shm_map::size() const
    {
        return size_;
    }

    void * coll_shm_map::extend_to(size_t size)
    {
        auto before_size = size_;
        auto after_size = size;

        if (after_size <= before_size)
            return NULL;

        auto extend_size = after_size - before_size;

        auto me = config_.get_native_pid();
        auto& map = shm_maps_[me];
        extend_shmem_region(map.fd, map.addr, before_size, extend_size, me);

        config_.barrier();

        // mmap the shared regions in each process
        for (auto& map : shm_maps_) {
            auto addr = map.addr + before_size;
            auto p = do_mmap(addr, extend_size, map.fd, before_size);

            if (addr == NULL)
                map.addr = p;
        }

        size_ = after_size;

        return map.addr + before_size;
    }

    comm_memory::comm_memory(process_config& config)
        : me_(config.get_native_pid())
        , n_procs_(config.get_native_n_procs())
        , coll_shm_maps_()
        , region_begin_(CM_BASE_ADDR)
        , region_end_(region_begin_ + CM_SHMPROC_SIZE * CM_MAX_SIZE)
    {
        std::vector<uint8_t *> addrs(n_procs_);

        for (auto i = 0; i < n_procs_; i++)
            addrs[i] = CM_BASE_ADDR + CM_MAX_SIZE * i;

        auto csmap = std::make_unique<coll_shm_map>(MEMID_DEFAULT,
                                               addrs, 0,
                                               address_type::same, config);

        coll_shm_maps_.push_back(std::move(csmap));
    }

    int comm_memory::coll_mmap(uint8_t *addr, size_t size,
                               process_config& config)
    {
        auto memid = coll_shm_maps_.size();

        auto csmap = std::make_unique<coll_shm_map>(memid, addr, size, config);
        coll_shm_maps_.push_back(std::move(csmap));

        return memid;
    }

    void comm_memory::coll_munmap(int memid, process_config& config)
    {
        auto it = coll_shm_maps_.begin() + memid;
        coll_shm_maps_.erase(it);
    }

    size_t comm_memory::size() const
    {
        return coll_shm_maps_[MEMID_DEFAULT]->size();
    }

    void * comm_memory::extend_to(size_t size, process_config& config)
    {
#ifdef __APPLE__
        bool is_darwin = true;
#else
        bool is_darwin = false;
#endif

        void *result;
        if (!is_darwin || coll_shm_maps_[MEMID_DEFAULT]->size() == 0) {
            result = coll_shm_maps_[MEMID_DEFAULT]->extend_to(size);
        } else {
            // Darwin does not support ftruncate for non-zero file.
            // So we have to reconstruct shared memory files. I hate OS X.

            auto memid = MEMID_DEFAULT;
            auto me = config.get_native_pid();
            auto n_procs = config.get_native_n_procs();

            auto csmap = std::move(coll_shm_maps_[memid]);
            auto addr = csmap->address(me);
            auto curr_size = csmap->size();

            // save addresses
            std::vector<uint8_t *> addrs(n_procs);
            for (auto i = 0; i < addrs.size(); i++)
                addrs[i] = csmap->address(i);

            // save data in my region
            std::vector<uint8_t> buf(addr, addr + curr_size);

            // destruct shared memory map
            csmap.reset(nullptr);

            // reconstruct shared memory map
            auto new_csmap = std::make_unique<coll_shm_map>(memid, addrs, size,
                                                       address_type::same,
                                                       config);
            coll_shm_maps_[memid] = std::move(new_csmap);

            std::memcpy(addr, buf.data(), curr_size);

            config.barrier();

            result = addr + curr_size;
        }
        
        MADI_ASSERT(CM_BASE_ADDR <= result);
        MADI_ASSERT(result < CM_BASE_ADDR + CM_MAX_SIZE * n_procs_);

        return result;
    }

}
}
