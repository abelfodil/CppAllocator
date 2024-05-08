#ifndef CPPALLOCATOR_ALLOCATORS_H
#define CPPALLOCATOR_ALLOCATORS_H

#include <unistd.h>
#include <sys/mman.h>

struct AllocatedMemory {
    void *ptr;
    size_t size;
};

// TODO See if allocated memory is page aligned
struct OSAllocator {
    inline static size_t MinimalSize = getpagesize();

    static AllocatedMemory Allocate(size_t size) {
        auto const actual_size = MinimalSize * (size / MinimalSize + 1);
        auto const ptr = mmap(nullptr, actual_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);
        return {
                .ptr = ptr,
                .size = ptr == nullptr ? 0 : actual_size,
        };
    }
};

template<size_t Size>
struct StackAllocator {
    inline static size_t MinimalSize = Size;

    inline static char buffer[Size];

    static AllocatedMemory Allocate(size_t) {
        return AllocatedMemory{
                .ptr = buffer,
                .size = Size,
        };
    }
};

#endif //CPPALLOCATOR_ALLOCATORS_H
