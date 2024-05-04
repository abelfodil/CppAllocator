#include <sys/mman.h>
#include <iostream>
#include <memory>

struct OSAllocator {
    inline static size_t MinimalSize = 1024;
    void *Allocate(size_t size) {
        return mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);
    }
};

template<size_t Size>
struct StackAllocator {
    inline static size_t MinimalSize = Size;

    inline static char buffer[Size];

    void *Allocate(size_t size) {
        if(size > Size) {
            return nullptr;
        }
        return buffer;
    }
};

template<typename LowLevelAllocatorPolicy>
struct MemoryHeapChunk : LowLevelAllocatorPolicy {
    void *ptr = nullptr;
    size_t size = 0;
    bool is_used = false;
    MemoryHeapChunk *prev = nullptr;
    std::shared_ptr<MemoryHeapChunk> next = nullptr;

    void free(void *ptr) {
        for (auto p = this; p != nullptr;) {
            if (p->ptr == ptr) {
                p->is_used = false;
                auto const next = p->next.get();
                Merge(p);
                Merge(next);
                break;
            }
            p = p->next.get();
        }
    }

    void *alloc(size_t size) {
        if (this->size == 0) {
            // TODO take alignment into account
            const size_t actual_size = std::max(LowLevelAllocatorPolicy::MinimalSize, size);
            this->ptr = Allocate(actual_size);
            this->size = actual_size;
        }

        if (this->ptr == nullptr) {
            this->size = 0;
            next.reset();
            return nullptr;
        }

        if(is_used || size > this->size) {
            if(next) {
                return next->alloc(size);
            } else {
                return nullptr;
            }
        }

        is_used = true;
        this->next = Fragment(this, size);
        return ptr;
    }

    size_t __test_count_chunks() {
        size_t count = 0;
        auto p = this;
        while (p) {
            ++count;
            p = p->next.get();
        }
        return count;
    }

    MemoryHeapChunk *__test_get_last_chunk() {
        auto p = this;
        while (true) {
            if (!p->next) {
                return p;
            }
            p = p->next.get();
        }
    }

private:
    std::shared_ptr<MemoryHeapChunk> Fragment(MemoryHeapChunk *to_fragment, size_t fragment_at) {
        const size_t original_size = to_fragment->size;
        if (fragment_at >= original_size) {
            return nullptr;
        }

        // TODO take alignment into account
        const size_t new_size = to_fragment->size - fragment_at;
        to_fragment->size = fragment_at;

        auto const old_next = to_fragment->next;

        to_fragment->next = std::make_shared<MemoryHeapChunk>(MemoryHeapChunk{
                .ptr = static_cast<char *>(to_fragment->ptr) + new_size,
                .size = new_size,
                .prev = to_fragment
        });

        if (old_next) {
            old_next->prev = to_fragment->next.get();
        }

        return to_fragment->next;
    }

    void Merge(MemoryHeapChunk *to_merge) {
        if (!to_merge) {
            return;
        }

        auto const previous = to_merge->prev;
        // TODO decouple from is_used
        if (!previous || previous->is_used || to_merge->is_used) {
            return;
        }

        previous->size += to_merge->size;
        if (to_merge->next) {
            previous->next = std::move(to_merge->next);
            previous->next->prev = previous;
        } else {
            previous->next.reset();
        }
    }

    using LowLevelAllocatorPolicy::Allocate;
    using LowLevelAllocatorPolicy::MinimalSize;
};

template<typename AllocatorType>
void test(AllocatorType&& allocator) {
    std::cout << "Testing allocator of type: " << typeid(AllocatorType).name() << "\n";

    std::cout << "Last chunk size before alloc: " << allocator.__test_get_last_chunk()->size << "\n";

    char* const elem = static_cast<char *>(allocator.alloc(1));
    std::cout << "Last chunk size after 1 alloc: " << allocator.__test_get_last_chunk()->size << "\n";

    allocator.free(elem);
    auto last_chunk = allocator.__test_get_last_chunk();
    std::cout << "Last chunk size after 1 free: " << last_chunk->size << "\n";

    constexpr auto ALLOC_ELEMENTS = 4096;
    char *elements[ALLOC_ELEMENTS];

    for (int i = 0; i < ALLOC_ELEMENTS; ++i) {
        elements[i] = static_cast<char *>(allocator.alloc(1));
    }
    std::cout << "Number of chunks after alloc: " << allocator.__test_count_chunks() << "\n";

    size_t null_elements = 0;
    for (int i = 0; i < ALLOC_ELEMENTS; ++i) {
        null_elements += elements[i] == nullptr;
    }
    std::cout << "Number of null elements after alloc: " << null_elements << "\n";

    for (int i = 0; i < ALLOC_ELEMENTS; ++i) {
        allocator.free(elements[i]);
    }
    std::cout << "Number of chunks after free: " << allocator.__test_count_chunks() << "\n";

    std::cout << "\n\n";
}

int main() {
    test(MemoryHeapChunk<OSAllocator>{});
    test(MemoryHeapChunk<StackAllocator<2048>>{});
}