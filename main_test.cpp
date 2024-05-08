#include <list>
#include <chrono>
#include <execution>
#include "MemoryHeap.h"
#include "Allocators.h"
#include "Spinlock.h"
#include "Fragmenters.h"
#include "Mergers.h"
#include "gtest/gtest.h"

namespace {
    template<typename T>
    class MemoryHeapTest : public testing::Test {
    public:
        using HeapType = T;
    protected:
        MemoryHeapTest() : table_(new T{}) {}

        ~MemoryHeapTest() override { delete table_; }

        T *const table_;
    };

    using testing::Types;

    typedef Types<
            MemoryHeap<std::list, Spinlock, OSAllocator, ForwardFragmenter, AdjacentMerger>,
            MemoryHeap<std::list, Spinlock, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>,
            MemoryHeap<std::list, std::mutex, OSAllocator, ForwardFragmenter, AdjacentMerger>,
            MemoryHeap<std::list, std::mutex, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>,
            MemoryHeap<std::vector, Spinlock, OSAllocator, ForwardFragmenter, AdjacentMerger>,
            MemoryHeap<std::vector, Spinlock, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>,
            MemoryHeap<std::vector, std::mutex, OSAllocator, ForwardFragmenter, AdjacentMerger>,
            MemoryHeap<std::vector, std::mutex, StackAllocator<8192>, ForwardFragmenter, AdjacentMerger>
    > Implementations;

    TYPED_TEST_SUITE(MemoryHeapTest, Implementations);

    TYPED_TEST(MemoryHeapTest, NoAllocations) {
        EXPECT_EQ(0, this->table_->storage.size());
    }

    TYPED_TEST(MemoryHeapTest, OneAllocation) {
        char *const elem = static_cast<char *>(this->table_->alloc(1));
        EXPECT_EQ(2, this->table_->storage.size());
        EXPECT_EQ(TestFixture::HeapType::AllocatorPolicy::MinimalSize - 1, this->table_->storage.rbegin()->size);

        this->table_->free(elem);
        EXPECT_EQ(1, this->table_->storage.size());
        EXPECT_EQ(TestFixture::HeapType::AllocatorPolicy::MinimalSize, this->table_->storage.rbegin()->size);
    }

    TYPED_TEST(MemoryHeapTest, LotsOfAllocationsWithForwardFree) {
        constexpr size_t ALLOC_ELEMENTS = 4097;
        std::array<char *, ALLOC_ELEMENTS> elements{};

        std::for_each(
                std::execution::par_unseq,
                elements.begin(),
                elements.end(),
                [this](auto &e) {
                    e = static_cast<char *>(this->table_->alloc(1));
                }
        );

        EXPECT_EQ(std::min(TestFixture::HeapType::AllocatorPolicy::MinimalSize, ALLOC_ELEMENTS + 1),
                  this->table_->storage.size());

        size_t null_elements = 0;
        for (auto &element: elements) {
            null_elements += element == nullptr;
        }
        EXPECT_EQ(std::max(0, (int) ALLOC_ELEMENTS - (int) TestFixture::HeapType::AllocatorPolicy::MinimalSize),
                  null_elements);

        std::for_each(
                std::execution::par_unseq,
                elements.begin(),
                elements.end(),
                [this](auto &e) {
                    this->table_->free(e);
                    e = nullptr;
                }
        );
        EXPECT_EQ(1, this->table_->storage.size());
    }

    TYPED_TEST(MemoryHeapTest, LotsOfAllocationsWithBackwardFree) {
        constexpr size_t ALLOC_ELEMENTS = 4096;
        std::array<char *, ALLOC_ELEMENTS> elements{};

        std::for_each(
                std::execution::par_unseq,
                elements.begin(),
                elements.end(),
                [this](auto &e) {
                    e = static_cast<char *>(this->table_->alloc(1));
                }
        );

        std::for_each(
                std::execution::par_unseq,
                elements.rbegin(),
                elements.rend(),
                [this](auto &e) {
                    this->table_->free(e);
                    e = nullptr;
                }
        );
        EXPECT_EQ(1, this->table_->storage.size());
    }
}  // namespace
