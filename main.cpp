#include <iostream>
#include <sys/mman.h>
#include <cstring>
#define CODEGEN_PERF
#include "Perf.h"

class allocator {
    public:
    virtual void* alloc(size_t size) = 0;
    virtual void* realloc(void* ptr, size_t oldSize, size_t newSize) = 0;
    virtual void free(void* ptr, size_t size) = 0;
};

class allocator1: public allocator {
    public:
    void* alloc(size_t size) {
        void* ptr = mmap((void*)0x700000000000, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) {
            perror("mmap");
            return NULL;
        }
        return ptr;
    }
    void* realloc(void* ptr, size_t oldSize, size_t newSize) {
        void* new_ptr = mremap(ptr, oldSize, newSize, 0);
        bool remap_succ = true;
        if (new_ptr == MAP_FAILED) {
            remap_succ = false;
            new_ptr =  mmap(NULL, newSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (new_ptr == MAP_FAILED) {
                perror("mremap");
                return NULL;
            }
        }
        ::memcpy(new_ptr, ptr, oldSize);
        if (!remap_succ) {
            munmap(ptr, oldSize);
        }
        return new_ptr;
    }
    void free(void* ptr, size_t size) {
        munmap(ptr, size);
    }
};

class allocator2: public allocator {
    public:
    void* alloc(size_t size) {
        void* ptr = mmap((void*)0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) {
            perror("mmap");
            return NULL;
        }
        return ptr;
    }
    void* realloc(void* ptr, size_t oldSize, size_t newSize) {
        void* newptr = mmap((void*)0, newSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (newptr == MAP_FAILED) {
            perror("mmap");
            return NULL;
        }
        ::memcpy(newptr, ptr, oldSize);
        munmap(ptr, oldSize);
        return newptr;
    }
    void free(void* ptr, size_t size) {
        munmap(ptr, size);
    }
};

class allocator3: public allocator {
    public:
    void* alloc(size_t size) {
        return malloc(size);
    }
    void* realloc(void* ptr, size_t oldSize, size_t newSize) {
        void* newptr = malloc(newSize);
        ::memcpy(newptr, ptr, oldSize);
        ::free(ptr);
        return newptr;
    }
    void free(void* ptr, size_t size) {
        ::free(ptr);
    }
};

//MADV_POPULATE_WRITE
class allocator4: public allocator {
    public:
    void* alloc(size_t size) {
        void* ptr = mmap((void*)0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED) {
            perror("mmap");
            return NULL;
        }
        return ptr;
    }
    void* realloc(void* ptr, size_t oldSize, size_t newSize) {
        void* newptr = mmap((void*)0, newSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (newptr == MAP_FAILED) {
            perror("mmap");
            return NULL;
        }
        ::memcpy(newptr, ptr, oldSize);
        madvise(newptr, newSize, MADV_POPULATE_WRITE);
        munmap(ptr, oldSize);
        return newptr;
    }
    void free(void* ptr, size_t size) {
        munmap(ptr, size);
    }
};

void f1(allocator &a) {
    for (auto j = 0; j < 2; ++ j) {
        auto i = 10;
        auto size = 1 << i;
        void* ptr = a.alloc(size);
        auto dst_size = 0;
        for (; i < 25; ++ i) {
            dst_size = 1 << (i + 1);
            memset(ptr, 0, size);
            ptr = a.realloc(ptr, size, dst_size);
            memset(ptr, 0, dst_size);
            size = dst_size;
        }
        a.free(ptr, dst_size);
    }
}
int main(int argc, char** argv) {
    if (argc < 2) {   
        return -1;
    }
    int use1 = atoi(argv[1]);
    std::vector<PerfEvent> myEvents = {
        {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS},
        {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MIN},
        {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_PAGE_FAULTS_MAJ},
        {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_ALIGNMENT_FAULTS},
        {PERF_TYPE_SOFTWARE, PERF_COUNT_SW_EMULATION_FAULTS},
    };
    facebook::velox::codegen::Perf p(myEvents);
    allocator1 a;
    allocator2 b;
    allocator3 c;
    allocator4 d;
    char *x;
    switch (use1) {
        case 0:
            std::cout << "remap" << std::endl;
            f1(a);
            break;
        case 1:
            std::cout << "mmap" << std::endl;
            f1(b);
            break;
        case 2:
            std::cout << "jemalloc" << std::endl;
            f1(c);
            break;
        case 3:
            std::cout << "madvise" << std::endl;
            f1(d);
            break;
        default:
            std::cout << "invalid" << std::endl;
            break;
    }
  return 0;
}
