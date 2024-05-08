#ifndef CPPALLOCATOR_SPINLOCK_H
#define CPPALLOCATOR_SPINLOCK_H

#include <atomic>

// Taken from https://rigtorp.se/spinlock/
struct Spinlock {
    std::atomic<bool> lock_ = {false};

    void lock() {
        for (;;) {
            if (try_lock()) {
                break;
            }
            while (lock_.load(std::memory_order_relaxed)) {
                __builtin_ia32_pause();
            }
        }
    }

    bool try_lock() {
        return !lock_.exchange(true, std::memory_order_acquire);
    }

    void unlock() {
        lock_.store(false, std::memory_order_release);
    }
};

#endif //CPPALLOCATOR_SPINLOCK_H
