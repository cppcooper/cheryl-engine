#include "gl-internal.h"
#include <backends/opengl/resource-lifetime.h>
#include "resource-lifetime-internal.h"

#include <internals/exceptions.h>
#include <internals/failure-reporting.h>

#include <utility>

namespace CE::Rendering::OpenGL {
    ResourceLifetime::ResourceLifetime(const std::thread::id owner, std::function<bool()> is_current)
    : ResourceLifetime(owner, std::move(is_current), {}, {}) {}

    ResourceLifetime::~ResourceLifetime() {
        abandon();
    }

    ResourceLifetime::ResourceLifetime(
        const std::thread::id owner,
        std::function<bool()> is_current,
        std::shared_ptr<::CE::Rendering::GraphicsDomainOwner> domain
    )
    : ResourceLifetime(owner, std::move(is_current), {}, std::move(domain)) {}

    ResourceLifetime::ResourceLifetime(
        const std::thread::id owner,
        std::function<bool()> is_current,
        std::shared_ptr<std::pmr::memory_resource> entry_memory,
        std::shared_ptr<::CE::Rendering::GraphicsDomainOwner> domain
    )
    : domain_(domain ? std::move(domain) : std::make_shared<::CE::Rendering::GraphicsDomainOwner>()),
      entry_memory_(std::move(entry_memory)),
      entries_(entry_memory_ ? entry_memory_.get() : std::pmr::new_delete_resource()),
      owner_(owner),
      is_current_(std::move(is_current)) {
        if (owner == std::thread::id{} || !is_current_)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "OpenGL lifetime needs an owner thread and a current-context predicate");
        if (domain_->token().snapshot().sequence != 0)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "OpenGL lifetime requires a fresh graphics domain");
        domain_->make_available();
    }

    std::shared_ptr<ResourceLifetime> Detail::LifetimeAccess::create(
        const std::thread::id owner,
        std::function<bool()> is_current,
        std::shared_ptr<std::pmr::memory_resource> entry_memory
    ) {
        if (!entry_memory)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Private lifetime construction requires owned entry memory");
        return std::shared_ptr<ResourceLifetime>(new ResourceLifetime(owner, std::move(is_current), std::move(entry_memory), {}));
    }

    std::size_t Detail::LifetimeAccess::capacity(const ResourceLifetime& lifetime) {
        const std::lock_guard lock(lifetime.mutex_);
        return lifetime.entries_.capacity();
    }

    std::size_t Detail::LifetimeAccess::size(const ResourceLifetime& lifetime) {
        const std::lock_guard lock(lifetime.mutex_);
        return lifetime.entries_.size();
    }

    void ResourceLifetime::require_owner_locked() const {
        if (!active_ || std::this_thread::get_id() != owner_)
            throw ::CE::Exceptions::failed_operation(CE_HERE, "OpenGL resource requires its live context thread");
    }

    void ResourceLifetime::require_current_locked() const {
        // Never query a borrowed context after shutdown or from a foreign thread.
        require_owner_locked();
        if (!is_current_())
            throw ::CE::Exceptions::failed_operation(CE_HERE, "OpenGL resource requires its own current context");
    }

    void ResourceLifetime::require_owner() const {
        const std::lock_guard lock(mutex_);
        require_owner_locked();
    }

    void ResourceLifetime::require_current() const {
        const std::lock_guard lock(mutex_);
        require_current_locked();
    }

    ResourceStats ResourceLifetime::diagnostics() const {
        const std::lock_guard lock(mutex_);
        return diagnostics_;
    }

    void ResourceLifetime::delete_handle(const ResourceKind kind, const GLuint id) noexcept {
        if (!id)
            return;
        switch (kind) {
            case ResourceKind::Texture:
                glDeleteTextures(1, &id);
                break;
            case ResourceKind::Buffer:
                glDeleteBuffers(1, &id);
                break;
            case ResourceKind::VertexArray:
                glDeleteVertexArrays(1, &id);
                break;
            case ResourceKind::Program:
                glDeleteProgram(id);
                break;
            case ResourceKind::ShaderStage:
                glDeleteShader(id);
                break;
            case ResourceKind::Sampler:
                glDeleteSamplers(1, &id);
                break;
        }
    }

    std::size_t ResourceLifetime::track(const ResourceKind kind, const GLuint id) {
        const std::lock_guard lock(mutex_);
        require_current_locked();
        if (kind != ResourceKind::Texture && kind != ResourceKind::Buffer && kind != ResourceKind::VertexArray &&
            kind != ResourceKind::Program && kind != ResourceKind::ShaderStage && kind != ResourceKind::Sampler)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "Unknown OpenGL resource kind");
        if (!id)
            throw ::CE::Exceptions::failed_operation(CE_HERE, "OpenGL failed to create a resource");
        if (free_ != none) {
            const auto slot = free_;
            free_ = entries_[slot].next;
            entries_[slot] = {kind, id};
            ++diagnostics_.tracked;
            ++diagnostics_.live;
            return slot;
        }
        entries_.push_back({kind, id});
        ++diagnostics_.tracked;
        ++diagnostics_.live;
        return entries_.size() - 1;
    }

    void ResourceLifetime::retire(const std::size_t slot) noexcept {
        try {
            const std::lock_guard lock(mutex_);
            if (!active_ || slot >= entries_.size() || entries_[slot].pending || !entries_[slot].id)
                return;
            entries_[slot].pending = true;
            entries_[slot].next = pending_;
            pending_ = slot;
            ++diagnostics_.pending;
        } catch (...) {
            // The shutdown sweep still owns this handle if a mutex operation fails.
            ::CE::Diagnostics::report_outcome("native_resources", diagnostics_.domain, "retire", "deferred_to_shutdown");
            ::CE::Diagnostics::report_failure("native resource retirement", std::current_exception());
        }
    }

    void ResourceLifetime::collect() {
        const std::lock_guard lock(mutex_);
        require_current_locked();
        while (pending_ != none) {
            const auto slot = pending_;
            auto& entry = entries_[slot];
            pending_ = entry.next;
            delete_handle(entry.kind, entry.id);
            --diagnostics_.live;
            --diagnostics_.pending;
            ++diagnostics_.deleted;
            entry.id = 0;
            entry.pending = false;
            entry.next = free_;
            free_ = slot;
        }
    }

    void ResourceLifetime::discard_untracked(const ResourceKind kind, const GLuint id) noexcept {
        if (!id)
            return;
        try {
            const std::lock_guard lock(mutex_);
            require_current_locked();
            delete_handle(kind, id);
        } catch (...) {
            // No tracking allocation succeeded. If the context cannot be used,
            // native context destruction owns the remaining cleanup.
            ::CE::Diagnostics::report_outcome("native_resources", diagnostics_.domain, "discard_untracked", "deferred_to_context", 1);
            ::CE::Diagnostics::report_failure("native untracked cleanup", std::current_exception());
        }
    }

    void ResourceLifetime::shutdown() {
        {
            const std::lock_guard lock(mutex_);
            require_current_locked();
            for (auto& entry : entries_) {
                delete_handle(entry.kind, entry.id);
                entry.id = 0;
            }
            diagnostics_.deleted += diagnostics_.live;
            diagnostics_.live = diagnostics_.pending = 0;
            diagnostics_.active = false;
            active_ = false;
            domain_->shutdown();
            entries_.clear();
            pending_ = free_ = none;
        }
    }

    void ResourceLifetime::abandon() noexcept {
        std::uint64_t count = 0;
        try {
            {
                const std::lock_guard lock(mutex_);
                count = diagnostics_.live;
                diagnostics_.abandoned += count;
                diagnostics_.live = diagnostics_.pending = 0;
                diagnostics_.active = false;
                active_ = false;
                domain_->abandon();
                entries_.clear();
                pending_ = free_ = none;
            }
            if (count)
                ::CE::Diagnostics::report_outcome("native_resources", diagnostics_.domain, "abandon", "deferred_to_context", count);
        } catch (...) {
            // No OpenGL call is permitted from this failure fallback.
            ::CE::Diagnostics::report_outcome("native_resources", diagnostics_.domain, "abandon", "failed");
            ::CE::Diagnostics::report_failure("native resource abandonment", std::current_exception());
        }
    }

    Handle::Handle(std::shared_ptr<ResourceLifetime> lifetime, const ResourceKind kind, const GLuint id)
    : lifetime_(std::move(lifetime)), id_(id), kind_(kind) {
        if (!lifetime_)
            throw ::CE::Exceptions::invalid_args(CE_HERE, "OpenGL handle needs a resource lifetime");
        slot_ = lifetime_->track(kind, id);
    }

    Handle::Handle(Handle&& other) noexcept
    : lifetime_(std::move(other.lifetime_)),
      slot_(std::exchange(other.slot_, std::numeric_limits<std::size_t>::max())),
      id_(std::exchange(other.id_, 0)),
      kind_(other.kind_) {}

    Handle& Handle::operator=(Handle&& other) noexcept {
        if (this != &other) {
            reset();
            lifetime_ = std::move(other.lifetime_);
            slot_ = std::exchange(other.slot_, std::numeric_limits<std::size_t>::max());
            id_ = std::exchange(other.id_, 0);
            kind_ = other.kind_;
        }
        return *this;
    }

    void Handle::reset() noexcept {
        if (lifetime_)
            lifetime_->retire(slot_);
        lifetime_.reset();
        id_ = 0;
    }

    GLuint Handle::id() const {
        if (!lifetime_)
            throw ::CE::Exceptions::failed_operation(CE_HERE, "OpenGL handle is empty");
        lifetime_->require_current();
        return id_;
    }
}
