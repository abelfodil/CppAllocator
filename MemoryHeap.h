#ifndef CPPALLOCATOR_MEMORYHEAP_H
#define CPPALLOCATOR_MEMORYHEAP_H

#include <iterator>
#include <algorithm>
#include <mutex>
#include "MemoryChunk.h"

template<template<typename> typename Container,
        typename MutexType,
        typename LowLevelAllocatorPolicy,
        template<template<typename> typename> typename FragmenterPolicy,
        template<template<typename> typename> typename MergerPolicy>
struct MemoryHeap : LowLevelAllocatorPolicy, FragmenterPolicy<Container>, MergerPolicy<Container> {
    using AllocatorPolicy = LowLevelAllocatorPolicy;
    using ContainerType = Container<MemoryChunk>;

    ContainerType storage{};
    MutexType mutex;

    void free(void *ptr) {
        if (!ptr) {
            return;
        }

        std::lock_guard lock{mutex};
        auto const p = std::find_if(std::begin(storage), std::end(storage), [ptr](auto const &e) { return e == ptr; });
        if (p != std::cend(storage)) {
            p->is_used = false;
            Merge(storage, p);
        }
    }

    void *alloc(size_t size) {
        std::lock_guard lock{mutex};
        if (std::empty(storage)) {
            storage.emplace_back(MemoryChunk::from(Allocate(size)));
        }

        auto const chunk = std::find_if(std::begin(storage), std::end(storage),
                                        [size](auto const &e) { return !e.is_used && e.size >= size; });

        if (std::cend(storage) == chunk) {
            return nullptr;
        }

        chunk->is_used = true;
        return Fragment(storage, chunk, size);
    }

    size_t __test_count_chunks() {
        return std::size(storage);
    }

    size_t __test_get_last_chunk_size() {
        auto const last_element = std::rbegin(storage);
        return std::rend(storage) != last_element ? last_element->size : 0U;
    }

private:
    using LowLevelAllocatorPolicy::Allocate;
    using LowLevelAllocatorPolicy::MinimalSize;
    using FragmenterPolicy<Container>::Fragment;
    using MergerPolicy<Container>::Merge;
};

#endif //CPPALLOCATOR_MEMORYHEAP_H
