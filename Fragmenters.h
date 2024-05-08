#ifndef CPPALLOCATOR_FRAGMENTERS_H
#define CPPALLOCATOR_FRAGMENTERS_H

#include <algorithm>
#include <iterator>
#include "MemoryChunk.h"

template<template<typename> typename Container>
struct ForwardFragmenter {
    static void *Fragment(Container<MemoryChunk> &storage,
                          Container<MemoryChunk>::iterator const &to_fragment,
                          size_t fragment_at) {
        size_t const original_size = to_fragment->size;
        if (fragment_at > original_size) {
            return nullptr;
        } else if (fragment_at == original_size) {
            return to_fragment->ptr;
        }

        // TODO take alignment into account
        const size_t new_size = to_fragment->size - fragment_at;
        to_fragment->size = fragment_at;

        auto *const ptr = to_fragment->ptr;
        storage.emplace(std::next(to_fragment),
                        MemoryChunk{
                                .ptr = static_cast<char *>(ptr) + new_size,
                                .size = new_size,
                        });

        return ptr;
    }
};

#endif //CPPALLOCATOR_FRAGMENTERS_H
