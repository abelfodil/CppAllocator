#include <sys/mman.h>
#include <iostream>
#include <memory>

struct OSAllocator {
    void *Allocate(size_t size) {
        return mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);
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
            constexpr size_t MINIMAL_HEAP_SIZE = 1024;
            // TODO take alignment into account
            const size_t actual_size = std::max(MINIMAL_HEAP_SIZE, size);
            this->ptr = Allocate(actual_size);
            this->size = actual_size;
        }

        if (this->ptr == nullptr) {
            this->size = 0;
            next.reset();
            return nullptr;
        }

        if (!is_used) {
            this->next = Fragment(this, size);
            // TODO check before doing this
            is_used = true;
            return ptr;
        }

        if (next) {
            return next->alloc(size);
        }

        // TODO expand mmap size

        return nullptr;
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
        auto const previous = to_merge->prev;
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
};

static MemoryHeapChunk<OSAllocator> os_heap;

int main() {
    std::cout << "Last chunk size before alloc: " << os_heap.__test_get_last_chunk()->size << "\n";

    char* const elem = static_cast<char *>(os_heap.alloc(1));
    std::cout << "Last chunk size after 1 alloc: " << os_heap.__test_get_last_chunk()->size << "\n";

    os_heap.free(elem);
    auto last_chunk = os_heap.__test_get_last_chunk();
    std::cout << "Last chunk size after 1 free: " << last_chunk->size << "\n";

    constexpr auto ALLOC_ELEMENTS = 512;
    char *elements[ALLOC_ELEMENTS];

    for (int i = 0; i < ALLOC_ELEMENTS; ++i) {
        elements[i] = static_cast<char *>(os_heap.alloc(1));
    }
    std::cout << "Number of chunks after alloc: " << os_heap.__test_count_chunks() << "\n";

    for (int i = 0; i < ALLOC_ELEMENTS; ++i) {
        os_heap.free(elements[i]);
    }
    std::cout << "Number of chunks after free: " << os_heap.__test_count_chunks() << "\n";
}