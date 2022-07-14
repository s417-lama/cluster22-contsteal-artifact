#include "../include/madm/allocator.h"
#include <cstdio>
#include <cstring>
#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

class mem_region {
    uint8_t *base_ptr_;
    size_t size_;
public:
    mem_region() : base_ptr_((uint8_t *)0x30000000000), size_(0)
    {
    }

    ~mem_region()
    {
        if (size_ != 0)
            munmap(base_ptr_, size_);
    }

    size_t size() { return size_; }

    void * extend_to(size_t size)
    {
        if (size == 0)
            size = 4096;

        uint8_t *addr = base_ptr_ + size_;
        size_t ext_size = size - size_;

        int prot = PROT_READ | PROT_WRITE;
        int flags = MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED;
        uint8_t *p = (uint8_t *)mmap(addr, ext_size, prot, flags, -1, 0);

        MADI_ASSERT(p == addr);

        size_ = size;
        return p;
    }
};

typedef madi::comm::allocator<mem_region> alloc;


int main(int argc, char **argv)
{
    mem_region mr;
    alloc a(&mr);

    void *ptrs[32];
    for (int i = 0; i < 32; i++) {
        int v = i + 1;

        void *p = a.allocate(100 * v);

        memset(p, v, 100 * v);

        ptrs[i] = p;
    }

    for (int i = 0; i < 32; i++) {
        int v = i + 1;

        uint8_t *p = (uint8_t *)ptrs[i];

        for (int j = 0; j < 100 * v; j++) {
            assert(p[j] == v);
        }

        a.deallocate(ptrs[i]);
    }

    return 0;
}

extern "C" {
    
    int madi_initialized()
    {
        return 1;
    }

    size_t madi_get_pid()
    {
        return 0;
    }

    void madi_exit(int exitcode)
    {
        exit(exitcode);
    }

    int madi_dprint_raw(const char *format, ...)
    {
        va_list arg;

        FILE *out = stderr;

        va_start(arg, format);
        int r = vfprintf(out, format, arg);
        va_end(arg);

        fflush(out);

        return r;
    }

}
