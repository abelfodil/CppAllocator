#ifndef CPPALLOCATOR_MERGERS_H
#define CPPALLOCATOR_MERGERS_H

#include <iterator>
#include "MemoryChunk.h"

template<template<typename> typename Container>
struct AdjacentMerger {
    static void Merge(Container<MemoryChunk> &storage, Container<MemoryChunk>::iterator const &to_merge) {
        if (std::cend(storage) == to_merge || !to_merge->has_memory() || to_merge->is_used) {
            return;
        }

        constexpr auto merge_with_previous = [](auto &storage, auto const &current, auto const &previous) {
            previous->size += current->size;
            storage.erase(current);
        };

        auto const next = std::next(to_merge);
        // TODO decouple from is_used
        if (std::cend(storage) != next && !next->is_used) {
            merge_with_previous(storage, next, to_merge);
        }

        auto const previous = std::prev(to_merge);
        // TODO decouple from is_used
        if (std::cbegin(storage) != to_merge && !previous->is_used) {
            merge_with_previous(storage, to_merge, previous);
        }
    }
};

#endif //CPPALLOCATOR_MERGERS_H
