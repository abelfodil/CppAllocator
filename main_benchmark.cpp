#include <benchmark/benchmark.h>
#include <list>
#include <chrono>
#include <execution>
#include "Allocators.h"
#include "Spinlock.h"
#include "Fragmenters.h"
#include "Mergers.h"
#include "MemoryHeap.h"

template<typename Heap>
static void BM_TestAlloc(benchmark::State &state) {
    constexpr size_t ALLOC_ELEMENTS = 4096;
    std::array<char *, ALLOC_ELEMENTS> elements{};

    for (auto _: state) {
        state.PauseTiming();
        Heap allocator{};
        state.ResumeTiming();

        std::for_each(
                std::execution::par_unseq,
                elements.begin(),
                elements.end(),
                [&allocator](auto &e) {
                    e = static_cast<char *>(allocator.alloc(1));
                }
        );
    }
}

template<typename Heap>
static void BM_TestFree(benchmark::State &state) {
    constexpr size_t ALLOC_ELEMENTS = 4096;
    std::array<char *, ALLOC_ELEMENTS> elements{};

    for (auto _: state) {
        state.PauseTiming();
        Heap allocator{};

        std::for_each(
                std::execution::par_unseq,
                elements.begin(),
                elements.end(),
                [&allocator](auto &e) {
                    e = static_cast<char *>(allocator.alloc(1));
                }
        );
        state.ResumeTiming();

        std::for_each(
                std::execution::par_unseq,
                elements.begin(),
                elements.end(),
                [&allocator](auto &e) {
                    allocator.free(e);
                    e = nullptr;
                }
        );
    }
}

template<typename Heap>
static void BM_TestFreeBackwards(benchmark::State &state) {
    constexpr size_t ALLOC_ELEMENTS = 4096;
    std::array<char *, ALLOC_ELEMENTS> elements{};

    for (auto _: state) {
        state.PauseTiming();
        Heap allocator{};

        std::for_each(
                std::execution::par_unseq,
                elements.begin(),
                elements.end(),
                [&allocator](auto &e) {
                    e = static_cast<char *>(allocator.alloc(1));
                }
        );
        state.ResumeTiming();

        std::for_each(
                std::execution::par_unseq,
                elements.rbegin(),
                elements.rend(),
                [&allocator](auto &e) {
                    allocator.free(e);
                    e = nullptr;
                }
        );
    }
}

BENCHMARK_TEMPLATE(BM_TestAlloc, MemoryHeap<std::list, Spinlock, OSAllocator, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestAlloc,
                   MemoryHeap<std::list, Spinlock, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestAlloc, MemoryHeap<std::list, std::mutex, OSAllocator, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestAlloc,
                   MemoryHeap<std::list, std::mutex, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestAlloc, MemoryHeap<std::vector, Spinlock, OSAllocator, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestAlloc,
                   MemoryHeap<std::vector, Spinlock, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestAlloc, MemoryHeap<std::vector, std::mutex, OSAllocator, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestAlloc,
                   MemoryHeap<std::vector, std::mutex, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>);

BENCHMARK_TEMPLATE(BM_TestFree, MemoryHeap<std::list, Spinlock, OSAllocator, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestFree,
                   MemoryHeap<std::list, Spinlock, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestFree, MemoryHeap<std::list, std::mutex, OSAllocator, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestFree,
                   MemoryHeap<std::list, std::mutex, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestFree, MemoryHeap<std::vector, Spinlock, OSAllocator, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestFree,
                   MemoryHeap<std::vector, Spinlock, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestFree, MemoryHeap<std::vector, std::mutex, OSAllocator, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestFree,
                   MemoryHeap<std::vector, std::mutex, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>);

BENCHMARK_TEMPLATE(BM_TestFreeBackwards, MemoryHeap<std::list, Spinlock, OSAllocator, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestFreeBackwards,
                   MemoryHeap<std::list, Spinlock, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestFreeBackwards, MemoryHeap<std::list, std::mutex, OSAllocator, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestFreeBackwards,
                   MemoryHeap<std::list, std::mutex, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestFreeBackwards, MemoryHeap<std::vector, Spinlock, OSAllocator, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestFreeBackwards,
                   MemoryHeap<std::vector, Spinlock, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestFreeBackwards, MemoryHeap<std::vector, std::mutex, OSAllocator, ForwardFragmenter, AdjacentMerger>);
BENCHMARK_TEMPLATE(BM_TestFreeBackwards,
                   MemoryHeap<std::vector, std::mutex, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>);

BENCHMARK_MAIN();
