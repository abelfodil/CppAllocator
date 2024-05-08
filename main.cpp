#include <sys/mman.h>
#include <iostream>
#include <list>
#include <algorithm>
#include <chrono>
#include <unistd.h>

struct AllocatedMemory {
    void *ptr;
    size_t size;
};

// TODO See if allocated memory is page aligned
struct OSAllocator {
    inline static size_t MinimalSize = getpagesize();

    AllocatedMemory Allocate(size_t size) {
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

    AllocatedMemory Allocate(size_t size) {
        return size > Size
               ? AllocatedMemory{
                        .ptr = nullptr,
                        .size = 0,}
               : AllocatedMemory{
                        .ptr = buffer,
                        .size = Size,
                };
    }
};

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

template<template<typename> typename Container>
struct ForwardFragmenter {
    static void *
    Fragment(Container<MemoryChunk> &storage, Container<MemoryChunk>::iterator const &to_fragment, size_t fragment_at) {
        size_t const original_size = to_fragment->size;
        if (fragment_at > original_size) {
            return nullptr;
        } else if (fragment_at == original_size) {
            return to_fragment->ptr;
        }

        // TODO take alignment into account
        const size_t new_size = to_fragment->size - fragment_at;
        to_fragment->size = fragment_at;

        storage.emplace(std::next(to_fragment),
                        MemoryChunk{
                                .ptr = static_cast<char *>(to_fragment->ptr) + new_size,
                                .size = new_size,
                        });

        return to_fragment->ptr;
    }
};

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

        auto current = to_merge;

        auto const previous = std::prev(current);
        // TODO decouple from is_used
        if (std::cbegin(storage) != current && !previous->is_used) {
            merge_with_previous(storage, current, previous);
            current = previous;
        }

        auto const next = std::next(current);
        // TODO decouple from is_used
        if (std::cend(storage) != next && !next->is_used) {
            merge_with_previous(storage, next, current);
        }
    }
};

template<template<typename> typename Container, typename LowLevelAllocatorPolicy,
        template<template<typename> typename> typename FragmenterPolicy,
        template<template<typename> typename> typename MergerPolicy>
struct MemoryHeap : LowLevelAllocatorPolicy, FragmenterPolicy<Container>, MergerPolicy<Container> {
    using AllocatorPolicy = LowLevelAllocatorPolicy;
    using ContainerType = Container<MemoryChunk>;

    ContainerType storage{};

    void free(void *ptr) {
        auto const p = std::find_if(std::begin(storage), std::end(storage), [ptr](auto const &e) { return e == ptr; });
        if (p != std::cend(storage)) {
            p->is_used = false;
            Merge(storage, p);
        }
    }

    void *alloc(size_t size) {
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

    constexpr size_t ALLOC_ELEMENTS = 4090;
    char *elements[ALLOC_ELEMENTS];

    using std::chrono::high_resolution_clock;
    using std::chrono::duration_cast;
    using std::chrono::milliseconds;

    auto t1 = high_resolution_clock::now();
    for (auto &element: elements) {
        element = static_cast<char *>(allocator.alloc(1));
    }
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
    for (auto &element: elements) {
        allocator.free(element);
        element = nullptr;
    }
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
    test(MemoryHeap<std::list, OSAllocator, ForwardFragmenter, AdjacentMerger>{});
    test(MemoryHeap<std::list, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>{});
    test(MemoryHeap<std::vector, OSAllocator, ForwardFragmenter, AdjacentMerger>{});
    test(MemoryHeap<std::vector, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>{});
}