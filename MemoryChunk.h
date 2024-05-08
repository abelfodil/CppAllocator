#ifndef CPPALLOCATOR_MEMORYCHUNK_H
#define CPPALLOCATOR_MEMORYCHUNK_H

#include "Allocators.h"

struct MemoryChunk {
    void *ptr = nullptr;
    size_t size = 0;
    bool is_used = false;

    static MemoryChunk from(AllocatedMemory const &allocatedMemory) {
        return {
                .ptr = allocatedMemory.ptr,
                .size = allocatedMemory.size,
        };
    }

    bool operator==(MemoryChunk const &rhs) const {
        return this->ptr == rhs.ptr;
    }

    bool operator==(void *const rhs) const {
        return this->ptr == rhs;
    }

    bool has_memory() const {
        return ptr != nullptr;
    }
};

#endif //CPPALLOCATOR_MEMORYCHUNK_H
