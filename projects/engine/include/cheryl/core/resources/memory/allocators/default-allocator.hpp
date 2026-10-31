#pragma once
#include <core/resources/memory/mem-mgr.h>
#include <limits>
#include <memory>
#include <new>
#include <stdexcept>

namespace CE::Memory::Allocators {
    /** Byte-backed standard allocator retaining a live checkout/return facade.
     * Copies and rebinds share authority. All facades use the same byte domain,
     * so storage may be returned through any instance. This does not synchronize
     * container access. Final return follows Engine's report-and-terminate policy
     * on bookkeeping failure; it never throws from container destruction.
     * The primary std::allocator_traits supplies generic object construction.
     */
    template <class T> class DefaultAllocator {
        template <class> friend class DefaultAllocator;
        using MM = ::CE::Memory::Manager<2.0, 4096>;
        std::shared_ptr<MM> manager_;

        static std::shared_ptr<MM> default_manager() {
            static const auto manager = std::allocate_shared<MM>(::CE::Memory::Detail::ControlAllocator<MM>{});
            return manager;
        }

    public:
        using value_type = T;
        using manager_type = MM;
        using is_always_equal = std::true_type;
        using propagate_on_container_swap = std::true_type;
        using propagate_on_container_move_assignment = std::true_type;
        using propagate_on_container_copy_assignment = std::false_type;
        template <class U> struct rebind { using other = DefaultAllocator<U>; };

        DefaultAllocator() : manager_(default_manager()) {}
        explicit DefaultAllocator(std::shared_ptr<MM> manager) : manager_(std::move(manager)) {
            if (!manager_)
                throw std::invalid_argument("DefaultAllocator requires a live manager");
        }
        // Copying on move also keeps moved-from allocators usable for new growth.
        DefaultAllocator(const DefaultAllocator&) noexcept = default;
        DefaultAllocator& operator=(const DefaultAllocator&) noexcept = default;
        template <class U> DefaultAllocator(const DefaultAllocator<U>& other) noexcept : manager_(other.manager_) {}

        [[nodiscard]] T* allocate(std::size_t count) {
            if (count > max_size())
                throw std::bad_array_new_length();
            if (!count)
                return nullptr;
            return static_cast<T*>(manager_->checkout_chunk(sizeof(T) * count, alignof(T), ::CE::Memory::exact).head.get());
        }
        void deallocate(T* pointer, std::size_t) noexcept {
            if (!pointer)
                return;
            try {
                manager_->return_ptr(pointer);
            } catch (...) {
                ::CE::Diagnostics::report_failure("allocator byte return", std::current_exception());
                std::terminate();
            }
        }
        [[nodiscard]] constexpr std::size_t max_size() const noexcept {
            return std::numeric_limits<std::size_t>::max() / sizeof(T);
        }
        template <class U> bool operator==(const DefaultAllocator<U>&) const noexcept { return true; }
    };
}
