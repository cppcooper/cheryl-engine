#include <backends/opengl/resource-lifetime.h>

#include <internals/exceptions.h>

#include <utility>

namespace CE::RenderAPIs {
    OpenGLResourceLifetime::OpenGLResourceLifetime(const std::thread::id owner, std::function<bool()> is_current)
    : owner_(owner), is_current_(std::move(is_current)) {
        if (owner == std::thread::id{} || !is_current_)
            throw Exceptions::invalid_args(CE_HERE, "OpenGL lifetime needs an owner thread and a current-context predicate");
    }

    void OpenGLResourceLifetime::require_owner_locked() const {
        if (!active_ || std::this_thread::get_id() != owner_)
            throw Exceptions::failed_operation(CE_HERE, "OpenGL resource requires its live context thread");
    }

    void OpenGLResourceLifetime::require_current_locked() const {
        // Never query a borrowed context after shutdown or from a foreign thread.
        require_owner_locked();
        if (!is_current_())
            throw Exceptions::failed_operation(CE_HERE, "OpenGL resource requires its own current context");
    }

    void OpenGLResourceLifetime::require_owner() const {
        const std::lock_guard lock(mutex_);
        require_owner_locked();
    }

    void OpenGLResourceLifetime::require_current() const {
        const std::lock_guard lock(mutex_);
        require_current_locked();
    }

    void OpenGLResourceLifetime::delete_handle(const GLResourceKind kind, const GLuint id) noexcept {
        if (!id)
            return;
        switch (kind) {
            case GLResourceKind::Texture:
                glDeleteTextures(1, &id);
                break;
            case GLResourceKind::Buffer:
                glDeleteBuffers(1, &id);
                break;
            case GLResourceKind::VertexArray:
                glDeleteVertexArrays(1, &id);
                break;
            case GLResourceKind::Program:
                glDeleteProgram(id);
                break;
            case GLResourceKind::ShaderStage:
                glDeleteShader(id);
                break;
        }
    }

    std::size_t OpenGLResourceLifetime::track(const GLResourceKind kind, const GLuint id) {
        const std::lock_guard lock(mutex_);
        require_current_locked();
        if (kind != GLResourceKind::Texture && kind != GLResourceKind::Buffer && kind != GLResourceKind::VertexArray &&
            kind != GLResourceKind::Program && kind != GLResourceKind::ShaderStage)
            throw Exceptions::invalid_args(CE_HERE, "Unknown OpenGL resource kind");
        if (!id)
            throw Exceptions::failed_operation(CE_HERE, "OpenGL failed to create a resource");
        if (free_ != none) {
            const auto slot = free_;
            free_ = entries_[slot].next;
            entries_[slot] = {kind, id};
            return slot;
        }
        entries_.push_back({kind, id});
        return entries_.size() - 1;
    }

    void OpenGLResourceLifetime::retire(const std::size_t slot) noexcept {
        try {
            const std::lock_guard lock(mutex_);
            if (!active_ || slot >= entries_.size() || entries_[slot].pending || !entries_[slot].id)
                return;
            entries_[slot].pending = true;
            entries_[slot].next = pending_;
            pending_ = slot;
        } catch (...) {
            // The shutdown sweep still owns this handle if a mutex operation fails.
        }
    }

    void OpenGLResourceLifetime::collect() {
        const std::lock_guard lock(mutex_);
        require_current_locked();
        while (pending_ != none) {
            const auto slot = pending_;
            auto& entry = entries_[slot];
            pending_ = entry.next;
            delete_handle(entry.kind, entry.id);
            entry.id = 0;
            entry.pending = false;
            entry.next = free_;
            free_ = slot;
        }
    }

    void OpenGLResourceLifetime::discard_untracked(const GLResourceKind kind, const GLuint id) noexcept {
        if (!id)
            return;
        try {
            const std::lock_guard lock(mutex_);
            require_current_locked();
            delete_handle(kind, id);
        } catch (...) {
            // No tracking allocation succeeded. If the context cannot be used,
            // native context destruction owns the remaining cleanup.
        }
    }

    void OpenGLResourceLifetime::shutdown() {
        const std::lock_guard lock(mutex_);
        require_current_locked();
        for (auto& entry : entries_) {
            delete_handle(entry.kind, entry.id);
            entry.id = 0;
        }
        active_ = false;
        entries_.clear();
        pending_ = free_ = none;
    }

    void OpenGLResourceLifetime::abandon() noexcept {
        try {
            const std::lock_guard lock(mutex_);
            active_ = false;
            entries_.clear();
            pending_ = free_ = none;
        } catch (...) {
            // No OpenGL call is permitted from this failure fallback.
        }
    }

    OpenGLHandle::OpenGLHandle(std::shared_ptr<OpenGLResourceLifetime> lifetime, const GLResourceKind kind, const GLuint id)
    : lifetime_(std::move(lifetime)), id_(id), kind_(kind) {
        if (!lifetime_)
            throw Exceptions::invalid_args(CE_HERE, "OpenGL handle needs a resource lifetime");
        slot_ = lifetime_->track(kind, id);
    }

    OpenGLHandle::OpenGLHandle(OpenGLHandle&& other) noexcept
    : lifetime_(std::move(other.lifetime_)),
      slot_(std::exchange(other.slot_, std::numeric_limits<std::size_t>::max())),
      id_(std::exchange(other.id_, 0)), kind_(other.kind_) {}

    OpenGLHandle& OpenGLHandle::operator=(OpenGLHandle&& other) noexcept {
        if (this != &other) {
            reset();
            lifetime_ = std::move(other.lifetime_);
            slot_ = std::exchange(other.slot_, std::numeric_limits<std::size_t>::max());
            id_ = std::exchange(other.id_, 0);
            kind_ = other.kind_;
        }
        return *this;
    }

    void OpenGLHandle::reset() noexcept {
        if (lifetime_)
            lifetime_->retire(slot_);
        lifetime_.reset();
        id_ = 0;
    }

    GLuint OpenGLHandle::id() const {
        if (!lifetime_)
            throw Exceptions::failed_operation(CE_HERE, "OpenGL handle is empty");
        lifetime_->require_current();
        return id_;
    }
}
