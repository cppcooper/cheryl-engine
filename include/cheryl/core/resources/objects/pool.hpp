#pragma once
//#include <core/resources/allocators.h>
#include <memory>
//#include <core/resources/memory/allocators/object-pool-allocator.hpp>
#include "object-construction.hpp"

namespace CE::Obj {
    template<typename T>
    template<typename ... Args>
    std::vector<std::shared_ptr<T>> PoolState<T>::retrieve_objects(std::size_t N, Args... args) {
        if (N == 0) return {};
        auto context = this->shared_from_this();
        std::vector<std::shared_ptr<T>> objects;
        objects.reserve(N);
        auto block = retrieve_block(N);
        std::size_t next = 0;
        try {
            for (; next < N; ++next) {
                auto* p = block.head.get() + next;
                auto constructed = std::make_shared<bool>(false);
                auto handle = std::shared_ptr<T>(p, [context, constructed](T* object) noexcept {
                    if (*constructed) {
                        ObjCtor<T>::destroy(object);
                        context->release_owned(object, 1);
                    }
                });
                ObjCtor<T>::construct(p, 1, std::forward<Args>(args)...);
                *constructed = true;
                objects.push_back(std::move(handle));
            }
        } catch (...) {
            // Completed handles return their own slots as the vector unwinds.
            // The unconstructed tail remains one contiguous range.
            context->release_owned(block.head.get() + next, N - next);
            throw;
        }
        return objects;
    }

    template<typename T>
    void PoolState<T>::release_owned(T* p, std::size_t length) noexcept {
        return_objects(p, length);
    }

    template<typename T>
    Block<T> PoolState<T>::retrieve_block(std::size_t N) {
        if (N == 0) {
            throw Exceptions::bad_request(CE_HERE, "Cannot retrieve an empty object block.");
        }
        OBlock<T> ob = this->fill_request(N);
        const bool request_filled = ob.has_value();
        if (!request_filled) {
            ob = allocate(N);
            this->record_new(*ob);
        } else if (this->contains(*ob, this->registry)) {
            // an existing block may still be marked stale
            this->erase(*ob, this->stale, this->release);
        }
        const auto right = ob->split_exactly(N);
        // we only need records if the right portion exists, because sections only deals in sub-blocks
        if (right.has_value()) {
            // record the left portion
            this->emplace(*ob, this->sections);
            // record the right portion
            this->emplace(*right, this->sections, this->pool);
        }
        return *ob;
    }

    template<typename T>
    void PoolState<T>::return_objects(T* p, std::size_t length) {
        if (length == 0) {
            throw Exceptions::bad_request(CE_HERE, "Cannot return an empty object range.");
        }
        auto FUNC = CE_FUNCTION_;
        auto ret_chunk = [this,FUNC](OBlock<T> b, T* p, std::size_t length) {
            if(!b.has_value()) {
                std::unreachable();
            }
            const auto original = *b;
            auto block_end = ptr::offset_address(b->head.get(), b->length * sizeof(T));
            auto end = ptr::offset_address(p, length * sizeof(T));

            if (end <= block_end) {
                bool sec_changed = false;
                auto remainder_end = (block_end - end) / sizeof(T);  // Remaining objects at the end
                auto remainder_front = (reinterpret_cast<uintptr_t>(p) - reinterpret_cast<uintptr_t>(b->head.get())) / sizeof(T);  // Remaining objects at the front

                // Split front part if necessary
                if (remainder_front > 0) {
                    sec_changed = true;
                    auto back_end = b->split_exactly(remainder_front);
                    this->emplace(*b, this->sections);  // Re-add modified front block to sections
                    b = back_end;  // Continue with the remaining block
                }
                // Split the back part if necessary
                if (remainder_end > 0) {
                    sec_changed = true;
                    auto back_end = b->split_exactly(b->length - remainder_end);
                    this->emplace(*back_end, this->sections);  // Re-add the split back portion
                }
                // If any changes occurred, update the sections
                if (sec_changed) {
                    this->erase(original, this->sections);
                    this->emplace(*b, this->sections);
                }
                this->merge_into_pool(*b);
            } else {
                throw Exceptions::bad_request(FUNC, __LINE__, "Invalid portion returned. The length exceeds the available memory block.");
            }
        };

        // Try to find the section or owner of the memory block
        if (auto sec = this->find_section(p); sec.has_value() && sec->contains(p)) {
            if (this->contains(*sec, this->pool)) {
                throw Exceptions::bad_request(CE_HERE, "Object range has already been returned.");
            }
            ret_chunk(sec, p, length);
        } else if (auto owner = this->find_owner(p); owner.has_value() && owner->contains(p)) {
            if (this->contains(*owner, this->pool)) {
                throw Exceptions::bad_request(CE_HERE, "Object range has already been returned.");
            }
            ret_chunk(owner, p, length);
        } else {
            throw Exceptions::bad_request(CE_HERE,"No matching block found for the given pointer.");
        }
    }

    template<typename T>
    void PoolState<T>::return_block(const Block<T> &returned) {
        const bool in_sections = this->contains(returned, this->sections);
        const bool in_registry = this->contains(returned, this->registry);
        bool has_active_sections = false;
        if (in_registry) {
            std::shared_lock lock(std::get<0>(this->sections));
            for (const auto& section : std::get<1>(this->sections)) {
                if (section.owner.get() == returned.owner.get()) {
                    has_active_sections = true;
                    break;
                }
            }
        }
        if ((!in_sections && !in_registry) || this->contains(returned, this->pool) || has_active_sections) {
            throw Exceptions::failed_operation(CE_HERE, "Object pool was returned an unknown or active block.");
        }
        this->merge_into_pool(returned);
    }

    template<typename T>
    Block<T> PoolState<T>::allocate(size_t length) {
        // we will allocate an ObjBlock to be recorded, it will clean up memory when we cull it
        auto& manager = Mem::ObjMMgr<T>::get();
        Mem::HeapBlock b = manager.checkout_chunk(length*sizeof(T), alignof(T), Enum::greedy);
        auto raw = static_cast<T*>(b.head.get());

        // calculate number of objects = bytes / size
        static_assert(!std::is_same_v<T,void>);
        const std::size_t len = b.length / sizeof(T);
        // we need to make a more useful pointer
        const auto manager_lifetime = manager.lifetime_token();
        std::shared_ptr<T> block_root(raw, [b,len,manager_lifetime,manager_ptr = &manager](auto p) {
            // ensure any pre-constructed objects (or something) get destroyed
            ObjCtor<T>::destroy(p,len);
            ObjCtor<T>::erase(p,p+len);
            // when our HeapBlock is stale, we'll need to return it
            if (manager_lifetime.lock()) {
                manager_ptr->return_chunk(b);
            }
        });

        return {block_root, block_root, b.alignment, len};
    }
}
