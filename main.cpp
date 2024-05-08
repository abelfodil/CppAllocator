#include <sys/mman.h>
#include <iostream>
#include <memory>
#include <list>
#include <algorithm>

struct OSAllocator {
    inline static size_t MinimalSize = 4096;

    void *Allocate(size_t size) {
        return mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);
    }
};

template<size_t Size>
struct StackAllocator {
    inline static size_t MinimalSize = Size;

    inline static char buffer[Size];

    void *Allocate(size_t size) {
        if (size > Size) {
            return nullptr;
        }
        return buffer;
    }
};

struct Chunk {
    void *ptr = nullptr;
    size_t size = 0;
    bool is_used = false;

    bool operator==(Chunk const &rhs) const {
        return this->ptr == rhs.ptr;
    }

    bool operator==(void *const rhs) const {
        return this->ptr == rhs;
    }

    bool has_memory() const {
        return ptr != nullptr;
    }
};

template<typename LowLevelAllocatorPolicy>
struct MemoryHeap : LowLevelAllocatorPolicy {
    using AllocatorPolicy = LowLevelAllocatorPolicy;

    std::list<Chunk> storage{};

    void free(void *ptr) {
        auto const p = std::find_if(std::begin(storage), std::end(storage), [ptr](auto const &e) { return e == ptr; });
        if (p != std::cend(storage)) {
            p->is_used = false;
            Merge(storage, p);
        }
    }

    void *alloc(size_t size) {
        if (std::empty(storage)) {
            // TODO take alignment into account
            const size_t actual_size = std::max(LowLevelAllocatorPolicy::MinimalSize, size);
            storage.emplace_back(Chunk{
                    .ptr = Allocate(actual_size),
                    .size = actual_size
            });
        }

        if (auto first = std::rbegin(storage); !first->has_memory()) {
            first->size = 0;
            return nullptr;
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
    static void *
    Fragment(std::list<Chunk> &storage, std::list<Chunk>::iterator const &to_fragment, size_t fragment_at) {
        size_t const original_size = to_fragment->size;
        if (fragment_at > original_size) {
            return nullptr;
        } else if(fragment_at == original_size) {
            return to_fragment->ptr;
        }

        // TODO take alignment into account
        const size_t new_size = to_fragment->size - fragment_at;
        to_fragment->size = fragment_at;

        storage.emplace(std::next(to_fragment),
                        Chunk{
                                .ptr = static_cast<char *>(to_fragment->ptr) + new_size,
                                .size = new_size,
                        });

        return to_fragment->ptr;
    }

    static void Merge(std::list<Chunk> &storage, std::list<Chunk>::iterator const &to_merge) {
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
//
        auto const next = std::next(current);
        // TODO decouple from is_used
        if (std::cend(storage) != next && !next->is_used) {
            merge_with_previous(storage, next, current);
        }
    }

    using LowLevelAllocatorPolicy::Allocate;
    using LowLevelAllocatorPolicy::MinimalSize;
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

    constexpr size_t ALLOC_ELEMENTS = 4096;
    char *elements[ALLOC_ELEMENTS];

    for (auto &element: elements) {
        element = static_cast<char *>(allocator.alloc(1));
    }
    std::cout << "Number of chunks after alloc: " << allocator.__test_count_chunks() << ", expected: "
              << std::min(AllocatorType::AllocatorPolicy::MinimalSize, ALLOC_ELEMENTS + 1) << "\n";

    size_t null_elements = 0;
    for (auto &element: elements) {
        null_elements += element == nullptr;
    }
    std::cout << "Number of null elements after alloc: " << null_elements << ", expected: "
              << std::max(0, (int) ALLOC_ELEMENTS - (int) AllocatorType::AllocatorPolicy::MinimalSize) << "\n";

    for (auto &element: elements) {
        allocator.free(element);
        element = nullptr;
    }
    std::cout << "Number of chunks after free: " << allocator.__test_count_chunks() << ", expected: "
              << 1 << "\n";

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

    for (auto &element: elements) {
        allocator.free(element);
        element = nullptr;
    }
    std::cout << "Number of chunks after free backwards: " << allocator.__test_count_chunks() << ", expected: "
              << 1 << "\n";

    std::cout << "\n\n";
}

int main() {
    test(MemoryHeap<OSAllocator>{});
    test(MemoryHeap<StackAllocator<8048>>{});
}