#pragma once
#include <cstddef>
#include <cstdlib>
#include <new>

template <typename T, std::size_t Alignment = 32>
struct AlignedAllocator {

    using value_type = T;
    AlignedAllocator() noexcept = default;
    template<class U> constexpr AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n) {
        std::size_t bytes = n * sizeof(T);
        void* p = nullptr;
        #if defined(_MSC_VER)
            p = _aligned_malloc(bytes, Alignment);
            if (!p) throw std::bad_alloc();
        #else
            if (posix_memalign(&p, Alignment, bytes) != 0) p = nullptr;
            if (!p) throw std::bad_alloc();
        #endif
        return static_cast<T*>(p);
    }

    void deallocate(T* p, std::size_t) noexcept {
        #if defined(_MSC_VER)
            _aligned_free(p);
        #else
            free(p);
        #endif
    }

    template <class U> struct rebind {
        using other = AlignedAllocator<U, Alignment>;
    };
};

template <class T, class U, std::size_t A>
constexpr bool operator==(const AlignedAllocator<T, A>&, const AlignedAllocator<U, A>&) noexcept {
    return true;
}

template <class T, class U, std::size_t A>
constexpr bool operator!=(const AlignedAllocator<T, A>&, const AlignedAllocator<U, A>&) noexcept {
    return false;
}