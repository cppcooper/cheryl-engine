#pragma once

#include <cheryl/export.h>

#include <backends/opengl/gl.h>
#include <core/diagnostics.h>
#include <core/rendering/graphics-domain.h>

#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <memory_resource>
#include <mutex>
#include <thread>
#include <vector>

namespace CE::Rendering::OpenGL {
    namespace Detail {
        struct LifetimeAccess;
    }

    /** Native object family used to select the matching OpenGL deletion operation. */
    enum class ResourceKind { Texture, ///< Texture object name.
        Buffer, ///< Vertex/index buffer name.
        VertexArray, ///< Vertex-array state name.
        Program, ///< Executable program name.
        ShaderStage, ///< Compiled shader-stage name.
        Sampler ///< Independent sampling-state name.
    };

    /** Synchronized cumulative counters for one native lifetime, not a residency budget. */
    struct ResourceStats {
        ::CE::Diagnostics::DomainId domain = 0; ///< Unique lifetime diagnostic identity.
        std::uint64_t tracked = 0; ///< Successfully adopted native objects over this lifetime.
        std::uint64_t live = 0; ///< Adopted objects not yet deleted/abandoned, including pending retirement.
        std::uint64_t pending = 0; ///< Live objects queued for owner-context collection.
        std::uint64_t deleted = 0; ///< Objects for which matching native deletion was issued.
        std::uint64_t abandoned = 0; ///< Objects left to native context destruction after cleanup recovery failed.
        bool active = true; ///< False after shutdown/abandon invalidates native use.
    };

    /** One retained native-resource domain with thread-safe retirement and owner-context deletion.
     * The current-context predicate borrows a context that outlives the active domain;
     * it runs only on the owner and must not reenter this lifetime. Retaining the
     * lifetime after shutdown preserves identity/statistics, not native usability.
     * @see [Resource residency](cheryl-guide:resources/resource-residency)
     */
    // All GL calls happen on the context thread. Asset destructors may run on another
    // thread, so they only mark their handles for deletion here.
    class ResourceLifetime final {
        static constexpr std::size_t none = std::numeric_limits<std::size_t>::max();
        struct Entry {
            ::CE::Rendering::OpenGL::ResourceKind kind;
            NativeHandle id;
            std::size_t next = none;
            bool pending = false;
        };

        mutable std::mutex mutex_;
        std::shared_ptr<::CE::Rendering::GraphicsDomainOwner> domain_;
        // Retain an injected allocator until the entry vector is destroyed.
        std::shared_ptr<std::pmr::memory_resource> entry_memory_;
        std::pmr::vector<Entry> entries_;
        std::size_t pending_ = none;
        std::size_t free_ = none;
        std::thread::id owner_;
        std::function<bool()> is_current_;
        bool active_ = true;
        ::CE::Rendering::OpenGL::ResourceStats diagnostics_{::CE::Diagnostics::next_domain_id()};

    public:
        // The predicate borrows a context that must outlive this active lifetime.
        // It is queried only on the owner thread, and must not reenter the lifetime.
        CE_OPENGL_API ResourceLifetime(std::thread::id owner, std::function<bool()> is_current);
        CE_OPENGL_API ~ResourceLifetime();
        // Renderer-owned identity is reserved before initialization touches native state.
        CE_OPENGL_API ResourceLifetime(
            std::thread::id owner,
            std::function<bool()> is_current,
            std::shared_ptr<::CE::Rendering::GraphicsDomainOwner> domain
        );
        [[nodiscard]] ::CE::Rendering::GraphicsDomain graphics_domain() const noexcept { return domain_->token(); }

        [[nodiscard]] CE_OPENGL_API std::size_t track(::CE::Rendering::OpenGL::ResourceKind kind, NativeHandle id);
        CE_OPENGL_API void retire(std::size_t slot) noexcept;
        CE_OPENGL_API void collect();
        // Only for a native ID not adopted by track(). Failure cleanup must not
        // delete on a foreign/missing context or replace the original exception.
        CE_OPENGL_API void discard_untracked(::CE::Rendering::OpenGL::ResourceKind kind, NativeHandle id) noexcept;
        CE_OPENGL_API void shutdown();
        // Failure fallback: invalidate handles without issuing calls to an unavailable context.
        // The platform's context destruction releases any remaining native resources.
        CE_OPENGL_API void abandon() noexcept;
        CE_OPENGL_API void require_owner() const;
        CE_OPENGL_API void require_current() const;
        // Snapshot under the lifetime lock; no logging or native calls.
        [[nodiscard]] CE_OPENGL_API ::CE::Rendering::OpenGL::ResourceStats diagnostics() const;

    private:
        friend struct ::CE::Rendering::OpenGL::Detail::LifetimeAccess;
        CE_OPENGL_API ResourceLifetime(
            std::thread::id owner,
            std::function<bool()> is_current,
            std::shared_ptr<std::pmr::memory_resource> entry_memory,
            std::shared_ptr<::CE::Rendering::GraphicsDomainOwner> domain
        );
        CE_OPENGL_API static void delete_handle(::CE::Rendering::OpenGL::ResourceKind kind, NativeHandle id) noexcept;
        CE_OPENGL_API void require_owner_locked() const;
        CE_OPENGL_API void require_current_locked() const;
    };

    /** Move-only native-object registration; destruction queues retirement without OpenGL calls. */
    class Handle final {
        std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> lifetime_;
        std::size_t slot_ = std::numeric_limits<std::size_t>::max();
        NativeHandle id_ = 0;
        ::CE::Rendering::OpenGL::ResourceKind kind_ = ::CE::Rendering::OpenGL::ResourceKind::Texture;

    public:
        Handle() = default;
        CE_OPENGL_API Handle(
            std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> lifetime,
            ::CE::Rendering::OpenGL::ResourceKind kind,
            NativeHandle id
        );
        ~Handle() { reset(); }

        Handle(const Handle&) = delete;
        Handle& operator=(const Handle&) = delete;
        CE_OPENGL_API Handle(Handle&& other) noexcept;
        CE_OPENGL_API Handle& operator=(Handle&& other) noexcept;

        [[nodiscard]] CE_OPENGL_API NativeHandle id() const;
        [[nodiscard]] ::CE::Rendering::OpenGL::ResourceKind kind() const noexcept { return kind_; }
        // Identity only: this does not query a context or authorize native use.
        [[nodiscard]] const ::CE::Rendering::OpenGL::ResourceLifetime* resource_domain() const noexcept { return lifetime_.get(); }

    private:
        CE_OPENGL_API void reset() noexcept;
    };
}

/**
 * @fn CE::Rendering::OpenGL::ResourceLifetime::ResourceLifetime(std::thread::id owner, std::function<bool()> is_current)
 * @brief Create active tracking for one context owner.
 * @param owner Nondefault thread identity permitted to issue native operations.
 * @param is_current Nonempty predicate borrowing the context; it must outlive active tracking and not reenter this object.
 * @throws CE::Exceptions::invalid_args If owner/predicate is absent.
 * Construction allocates tracking state but adopts no native objects.
 */

/**
 * @fn CE::Rendering::OpenGL::ResourceLifetime::~ResourceLifetime()
 * @brief Abandon any still-active domain without native calls or listener invocation.
 * Prefer explicit shutdown() while the context is usable. Retained portable tokens
 * become unavailable even when a separate backend authority remains retained.
 */

/**
 * @fn std::size_t CE::Rendering::OpenGL::ResourceLifetime::track(ResourceKind kind, NativeHandle id)
 * @brief Adopt an untracked nonzero native name on the live owner/current context.
 * @param kind Matching deletion family; unknown values reject.
 * @param id Native name whose successful registration transfers cleanup responsibility to this lifetime.
 * @return Internal registration slot passed to retire(), not a native name or permanent public identifier.
 * Validation/allocation failure leaves adoption incomplete: the caller must discard_untracked() without replacing the original exception. Do not adopt the same native object twice.
 */

/**
 * @fn void CE::Rendering::OpenGL::ResourceLifetime::retire(std::size_t slot) noexcept
 * @brief Queue a tracked slot for deletion from any thread without native calls.
 * @param slot Registration from track(); inactive, absent or already-pending entries are ignored.
 * Requires live tracking storage, not a current context. Collection/shutdown later deletes the name; mutex failures are reported and defer cleanup to shutdown. Do not reuse a stale slot after collection.
 */

/**
 * @fn void CE::Rendering::OpenGL::ResourceLifetime::collect()
 * @brief Delete queued native objects on the live owner/current context.
 * After deletion tracking slots become reusable. Owner/context failure throws failed_operation; this does not stop the domain or delete still-retained objects.
 */

/**
 * @fn void CE::Rendering::OpenGL::ResourceLifetime::discard_untracked(ResourceKind kind, NativeHandle id) noexcept
 * @brief Best-effort constructor-failure cleanup for a name not adopted by track().
 * @param kind Correct native deletion family.
 * @param id Untracked object name; zero is ignored.
 * Deletes only on the live owner/current context. Failure is reported without replacing the original exception; context destruction owns remaining cleanup. Never pass an adopted name.
 */

/**
 * @fn void CE::Rendering::OpenGL::ResourceLifetime::shutdown()
 * @brief Delete every tracked object and permanently deactivate the domain.
 * Requires the live owner/current context; otherwise failed_operation is thrown. Retained logical handles become unusable for native operations, including handles not yet retired. The caller must finish rendering/loading before shutdown.
 */

/**
 * @fn void CE::Rendering::OpenGL::ResourceLifetime::abandon() noexcept
 * @brief Deactivate tracking without any OpenGL calls when native cleanup cannot run.
 * Retained handles stop querying the borrowed context. Remaining native names are left to context destruction; diagnostics record abandoned objects. This is a failure fallback, not ordinary collection.
 */

/**
 * @fn void CE::Rendering::OpenGL::ResourceLifetime::require_owner() const
 * @brief Check active domain and calling-thread identity under the lifetime lock.
 * @throws CE::Exceptions::failed_operation For a stopped domain or foreign thread. Does not query the context.
 */

/**
 * @fn void CE::Rendering::OpenGL::ResourceLifetime::require_current() const
 * @brief Check active owner first, then the borrowed current-context predicate.
 * @throws CE::Exceptions::failed_operation For stopped/foreign/noncurrent use. The predicate is never called from a foreign thread or after deactivation.
 */

/**
 * @fn CE::Rendering::OpenGL::ResourceStats CE::Rendering::OpenGL::ResourceLifetime::diagnostics() const
 * @return Copied counters under the lifetime mutex, with no logging or native queries; safe to observe independently of the native owner while retaining this object.
 */

/**
 * @fn CE::Rendering::OpenGL::Handle::Handle()
 * @brief Create an empty logical registration; id() rejects it, resource_domain() returns null.
 */

/**
 * @fn CE::Rendering::OpenGL::Handle::Handle(std::shared_ptr<::CE::Rendering::OpenGL::ResourceLifetime> lifetime, ::CE::Rendering::OpenGL::ResourceKind kind, NativeHandle id)
 * @brief Adopt a native object through a retained live lifetime.
 * @param lifetime Required domain whose owner/current context is active.
 * @param kind Matching native object family.
 * @param id Nonzero untracked native name.
 * Null lifetime throws invalid_args; track() validation/allocation errors propagate. If construction fails before adoption, the creator still owns untracked cleanup.
 */

/**
 * @fn CE::Rendering::OpenGL::Handle::~Handle()
 * @brief Queue this registration for retirement and release the lifetime reference; performs no native calls and may run off the context owner.
 */

/**
 * @fn CE::Rendering::OpenGL::Handle::Handle(Handle&& other) noexcept
 * @brief Transfer the registration without native calls; other becomes empty and no longer retires the transferred object.
 */

/**
 * @fn CE::Rendering::OpenGL::Handle& CE::Rendering::OpenGL::Handle::operator=(Handle&& other) noexcept
 * @brief Retire the destination's previous registration and take the source registration.
 * @return This handle; self-move preserves it. No OpenGL calls occur, and a moved source becomes empty.
 */

/**
 * @fn CE::Rendering::OpenGL::NativeHandle CE::Rendering::OpenGL::Handle::id() const
 * @return Native name after requiring this registration's live owner/current context.
 * @throws CE::Exceptions::failed_operation For empty, stopped, foreign-thread or noncurrent use. A logical retained handle alone does not authorize native access.
 */

/**
 * @fn CE::Rendering::OpenGL::ResourceKind CE::Rendering::OpenGL::Handle::kind() const noexcept
 * @return Stored deletion family without native validation; for an empty/moved-from handle this is metadata only.
 */

/**
 * @fn const CE::Rendering::OpenGL::ResourceLifetime* CE::Rendering::OpenGL::Handle::resource_domain() const noexcept
 * @return Borrowed domain identity, or null for an empty/moved-from handle. This does not check activity, thread or current context; keep the handle/lifetime retained while comparing it.
 */
