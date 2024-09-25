#pragma once
#ifndef OPOOLHPP
#define OPOOLHPP

#include <resources/allocators.h>
#include <memory>
#include <resources/memory/allocators/object-pool-allocator.hpp>
#include "object-construction.hpp"
#include <assets/primitives.h>

namespace CE::Obj {
    template<typename T>
    template<typename ... Args>
    std::vector<std::shared_ptr<T>> Pool<T>::retrieve_objects(std::size_t N, Args... args) {
        auto block = retrieve_block(N);
        ObjCtor<T>::construct(block.head.get(), N, std::forward<Args>(args)...);
        return block.vector([](T* p) {
            ObjCtor<T>::destroy(p);
            Pool<T>::get().return_objects(p,1);
        });
    }

    template<typename T>
    Block<T> Pool<T>::retrieve_block(std::size_t N) {
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
    void Pool<T>::return_objects(T* p, std::size_t length) {
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
            ret_chunk(sec, p, length);
        } else if (auto owner = this->find_owner(p); owner.has_value() && owner->contains(p)) {
            ret_chunk(owner, p, length);
        } else {
            throw Exceptions::bad_request(CE_HERE,"No matching block found for the given pointer.");
        }
    }

    template<typename T>
    void Pool<T>::return_block(const Block<T> &returned) {
        if (!this->contains(returned, this->sections) && !this->contains(returned, this->registry)) {
            CELog::error("Cannot return Block. No such block exists. Block: {}", returned);
            return;
        }
        this->merge_into_pool(returned);
    }

    template<typename T>
    template<typename ... Args>
    Block<T> Pool<T>::allocate(size_t length, Args... args) {
        // we will allocate an ObjBlock to be recorded, it will clean up memory when we cull it
        Mem::HeapBlock b = Mem::ObjMMgr<T>::get().checkout_chunk(length*sizeof(T), alignof(T), Enum::greedy);
        auto raw = static_cast<T*>(b.head.get());

        // calculate number of objects = bytes / size
        static_assert(!std::is_same_v<T,void>);
        uint32_t len = b.length / sizeof(T);
        // we need to make a more useful pointer
        std::shared_ptr<T> block_root(raw, [b,len](auto p) {
            // ensure any pre-constructed objects (or something) get destroyed
            ObjCtor<T>::destroy(p,len);
            ObjCtor<T>::erase(p,p+len);
            // when our HeapBlock is stale, we'll need to return it
            Mem::ObjMMgr<T>::get().return_chunk(b);
        });

        // construct objects if legal
        if constexpr (std::is_constructible_v<T, Args...>) {
            ObjCtor<T>::construct(block_root.get(), len, std::forward<Args>(args)...);
        }
        return {block_root, block_root, b.alignment, len};;
    }
}


#endif
