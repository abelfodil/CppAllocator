#include <iostream>
#include <list>
#include <chrono>
#include <execution>
#include "Allocators.h"
#include "Spinlock.h"
#include "Fragmenters.h"
#include "Mergers.h"
#include "MemoryHeap.h"

template<typename AllocatorType>
void test(AllocatorType &&allocator) {
    std::cout << "Testing allocator of type: " << typeid(AllocatorType).name() << "\n";

    std::cout << "Last chunk size before alloc: " << allocator.__test_get_last_chunk_size() << ", expected: " << 0
              << "\n";

    char *const elem = static_cast<char *>(allocator.alloc(1));
    std::cout << "Last chunk size after 1 alloc: " << allocator.__test_get_last_chunk_size() << ", expected: "
              << AllocatorType::AllocatorPolicy::MinimalSize - 1 << "\n";

    allocator.free(elem);
    std::cout << "Last chunk size after 1 free: " << allocator.__test_get_last_chunk_size() << ", expected: "
              << AllocatorType::AllocatorPolicy::MinimalSize << "\n";

    constexpr size_t ALLOC_ELEMENTS = 4096;
    std::array<char *, ALLOC_ELEMENTS> elements{};

    using std::chrono::high_resolution_clock;
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    auto t1 = high_resolution_clock::now();
    std::for_each(
            std::execution::par_unseq,
            elements.begin(),
            elements.end(),
            [&allocator](auto &e) {
                e = static_cast<char *>(allocator.alloc(1));
            }
    );
    auto t2 = high_resolution_clock::now();

    std::cout << "Number of chunks after alloc: " << allocator.__test_count_chunks() << ", expected: "
              << std::min(AllocatorType::AllocatorPolicy::MinimalSize, ALLOC_ELEMENTS + 1) << "\n";
    std::cout << "Processing time of alloc: " << duration_cast<milliseconds>(t2 - t1).count() << "ms\n";

    size_t null_elements = 0;
    for (auto &element: elements) {
        null_elements += element == nullptr;
    }
    std::cout << "Number of null elements after alloc: " << null_elements << ", expected: "
              << std::max(0, (int) ALLOC_ELEMENTS - (int) AllocatorType::AllocatorPolicy::MinimalSize) << "\n";

    t1 = high_resolution_clock::now();
    std::for_each(
            std::execution::par_unseq,
            elements.begin(),
            elements.end(),
            [&allocator](auto &e) {
                allocator.free(e);
                e = nullptr;
            }
    );
    t2 = high_resolution_clock::now();
    std::cout << "Number of chunks after free: " << allocator.__test_count_chunks() << ", expected: "
              << 1 << "\n";
    std::cout << "Processing time of free: " << duration_cast<milliseconds>(t2 - t1).count() << "ms\n";

    for (auto &element: elements) {
        element = static_cast<char *>(allocator.alloc(1));
    }
    std::cout << "Number of chunks after alloc: " << allocator.__test_count_chunks() << ", expected: "
              << std::min(AllocatorType::AllocatorPolicy::MinimalSize, ALLOC_ELEMENTS + 1) << "\n";

    null_elements = 0;
    for (auto &element: elements) {
        null_elements += element == nullptr;
    }
    std::cout << "Number of null elements after alloc: " << null_elements << ", expected: "
              << std::max(0, (int) ALLOC_ELEMENTS - (int) AllocatorType::AllocatorPolicy::MinimalSize) << "\n";

    t1 = high_resolution_clock::now();
    for (auto &element: elements) {
        allocator.free(element);
        element = nullptr;
    }
    t2 = high_resolution_clock::now();

    std::cout << "Number of chunks after free backwards: " << allocator.__test_count_chunks() << ", expected: "
              << 1 << "\n";
    std::cout << "Processing time of backwards: " << duration_cast<milliseconds>(t2 - t1).count() << "ms\n";

    std::cout << "\n\n";
}

int main() {
    test(MemoryHeap<std::list, Spinlock, OSAllocator, ForwardFragmenter, AdjacentMerger>{});
    test(MemoryHeap<std::list, Spinlock, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>{});
    test(MemoryHeap<std::list, std::mutex, OSAllocator, ForwardFragmenter, AdjacentMerger>{});
    test(MemoryHeap<std::list, std::mutex, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>{});
    test(MemoryHeap<std::vector, Spinlock, OSAllocator, ForwardFragmenter, AdjacentMerger>{});
    test(MemoryHeap<std::vector, Spinlock, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>{});
    test(MemoryHeap<std::vector, std::mutex, OSAllocator, ForwardFragmenter, AdjacentMerger>{});
    test(MemoryHeap<std::vector, std::mutex, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>{});
}