#include <cheryl/core/game-framework/dialogue.h>

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <utility>

// Isolated native probe, never linked into GoogleTest. Fail only bounded local
// C++ allocation attempts, without exhaustion or application failure machinery.
namespace {
    thread_local bool deny = false;
    thread_local int remaining = -1;
    thread_local unsigned rejected = 0;
    thread_local unsigned allocations = 0;

    void* allocate(std::size_t size, std::size_t alignment = 0) {
        if (deny || remaining == 0) {
            ++rejected;
            throw std::bad_alloc{};
        }
        if (remaining > 0)
            --remaining;
        ++allocations;
        size = size ? size : 1;
        void* pointer;
        if (alignment) {
            if (size > std::numeric_limits<std::size_t>::max() - (alignment - 1))
                throw std::bad_alloc{};
            pointer = std::aligned_alloc(alignment, ((size + alignment - 1) / alignment) * alignment);
        } else
            pointer = std::malloc(size);
        if (!pointer)
            throw std::bad_alloc{};
        return pointer;
    }
    int fail(const char* message) {
        deny = false;
        remaining = -1;
        std::fprintf(stderr, "%s\n", message);
        return 1;
    }
}

void* operator new(std::size_t size) {
    return allocate(size);
}
void* operator new[](std::size_t size) {
    return allocate(size);
}
void* operator new(std::size_t size, std::align_val_t alignment) {
    return allocate(size, static_cast<std::size_t>(alignment));
}
void* operator new[](std::size_t size, std::align_val_t alignment) {
    return allocate(size, static_cast<std::size_t>(alignment));
}
void operator delete(void* p) noexcept {
    std::free(p);
}
void operator delete[](void* p) noexcept {
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}
void operator delete[](void* p, std::size_t) noexcept {
    std::free(p);
}
void operator delete(void* p, std::align_val_t) noexcept {
    std::free(p);
}
void operator delete[](void* p, std::align_val_t) noexcept {
    std::free(p);
}
void operator delete(void* p, std::size_t, std::align_val_t) noexcept {
    std::free(p);
}
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept {
    std::free(p);
}

int main() {
    namespace D = CE::Dialogue;
    const std::string name(256, 'n'), target(256, 't'), operation(256, 'e');
    const D::Draft draft{
        {name},
        19,
        {name},
        {{{name}, "", D::NodeKind::Prompt, {{{name}, "", {target}, D::ConditionKey{operation}, D::EffectKey{operation}}}},
         {{target}, "", D::NodeKind::Terminal, {}}}
    };
    const D::OperationKeys keys{{{operation}}, {{operation}}};
    const auto definition = D::compile(draft, keys).value();
    const auto start = [&] { return D::Session::start(definition, {name}).value(); };
    const D::ConditionHandler enabled = [](const auto&) { return true; };
    const D::ConditionHandler disabled = [](const auto&) { return false; };

    // Confirm all hook forms plus actual shared-Engine interposition, not just
    // allocations made directly in this executable.
    for (int kind = 0; kind < 4; ++kind) {
        bool observed = false;
        const auto prior = rejected;
        deny = true;
        try {
            if (kind == 0) {
                auto* p = ::operator new(1);
                ::operator delete(p);
            } else if (kind == 1) {
                auto* p = ::operator new[](1);
                ::operator delete[](p);
            } else if (kind == 2) {
                auto* p = ::operator new(1, std::align_val_t{64});
                ::operator delete(p, std::align_val_t{64});
            } else
                static_cast<void>(D::compile(draft, keys));
        } catch (const std::bad_alloc&) {
            observed = true;
        }
        deny = false;
        if (!observed || rejected != prior + 1)
            return fail("Allocation/shared Engine rejection control failed");
    }

    // Sweep a fixed small upper bound over preparatory string allocations. Each
    // failure must precede the effect, preserve progress and clear the busy guard.
    unsigned failures = 0;
    bool complete = false;
    for (int allowance = 0; allowance < 32; ++allowance) {
        auto session = start();
        const auto before = session.snapshot().value();
        const auto token = session.view(enabled).value().choices[0].selection;
        int effects = 0;
        const D::EffectHandler apply = [&](const auto&) { ++effects; };
        bool observed = false;
        remaining = allowance;
        try {
            const auto result = session.choose_with_transition(token, enabled, apply);
            remaining = -1;
            if (!result || effects != 1 || result->new_step != 1)
                return fail("Preparatory sweep success was incoherent");
            complete = true;
        } catch (const std::bad_alloc&) {
            observed = true;
        }
        remaining = -1;
        if (complete)
            break;
        if (!observed || effects != 0 || session.snapshot().value() != before)
            return fail("Preparatory failure invoked effect, faulted or advanced session");
        ++failures;
        if (!session.choose_with_transition(token, enabled, apply) || effects != 1)
            return fail("Preparatory failure did not leave original selection healthy");
    }
    if (!complete || failures < 5)
        return fail("Preparatory allocation sweep did not reach every owning field");

    // Enable denial INSIDE the successful effect, after every owned field and
    // expected success result must be complete. Move the actual result too.
    {
        auto session = start();
        const auto token = session.view(enabled).value().choices[0].selection;
        int effects = 0;
        bool key_borrowed = false;
        unsigned at_effect = 0, prior_rejected = rejected;
        const D::EffectHandler apply = [&](const auto& key) {
            key_borrowed = &key == &*definition->entry().choices[0].effect;
            ++effects;
            at_effect = allocations;
            deny = true;
        };
        try {
            auto result = session.choose_with_transition(token, enabled, apply);
            auto moved = std::move(result);
            const bool valid = moved && moved->new_step == 1 && moved->old_step == 0;
            deny = false;
            if (!valid || !key_borrowed || effects != 1 || allocations != at_effect || rejected != prior_rejected)
                return fail("Post-effect receipt return/movement allocated or lost observation");
            if (moved->definition.value != name || moved->definition_revision != 19 || moved->session.value != name ||
                moved->source.value != name || moved->target.value != target || moved->choice.value != name)
                return fail("Owned observation fields differ from committed choice");
        } catch (...) {
            return fail("Post-effect receipt return threw");
        }
        const auto saved = session.snapshot().value();
        if (saved.node.value != target || saved.step != 1)
            return fail("Post-effect receipt cursor commit failed");
    }

    // Legacy choose must not allocate unused observations even BEFORE the effect.
    // Rejection paths likewise require no receipt preparation.
    {
        auto session = start();
        const auto token = session.view(enabled).value().choices[0].selection;
        const auto before = session.snapshot().value();
        auto foreign = token;
        foreign.session.value = "foreign";
        int effects = 0;
        const D::EffectHandler apply = [&](const auto&) {
            ++effects;
            deny = true;
        };
        const auto prior = rejected;
        deny = true;
        try {
            const auto unavailable = session.choose_with_transition(token, disabled, apply);
            const auto wrong = session.choose_with_transition(foreign, enabled, apply);
            if (unavailable || unavailable.error() != D::SessionError::Unavailable || wrong ||
                wrong.error() != D::SessionError::WrongSession || effects != 0)
                return fail("Rejected choice entered effect or lost rejection order");
            const auto result = session.choose(token, enabled, apply);
            deny = false;
            if (!result || effects != 1 || rejected != prior)
                return fail("Legacy choose paid for unused receipt or allocated after effect");
        } catch (...) {
            return fail("Legacy choose or early rejection allocated");
        }
        if (before.step != 0 || session.snapshot().value().step != 1)
            return fail("Legacy choose cursor commit failed");
    }
    std::printf(
        "Transition allocation probe passed: %u preparatory failures before effects; shared Engine hooks verified; post-effect receipt return/move and legacy choose allocate nothing.\n",
        failures
    );
    return 0;
}
