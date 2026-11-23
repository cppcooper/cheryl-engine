#include <cheryl/core/game-framework/dialogue.h>

#include <cstdlib>
#include <new>

// Standalone probe only: never link these replacement operators into tests-engine.
namespace {
    thread_local bool reject_allocation = false;
    thread_local int rejected = 0;
}

void* operator new(std::size_t size) {
    if (reject_allocation) {
        ++rejected;
        throw std::bad_alloc{};
    }
    if (void* pointer = std::malloc(size ? size : 1))
        return pointer;
    throw std::bad_alloc{};
}

void operator delete(void* pointer) noexcept {
    std::free(pointer);
}
void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}

int main() {
    namespace D = CE::Dialogue;
    // Long strings detect accidental owning key/result copies hidden by small-string storage.
    const std::string name(128, 'n');
    const std::string operation(128, 'e');
    const D::Draft draft{
        {name},
        1,
        {name},
        {{{name}, name, D::NodeKind::Prompt, {{{name}, name, {"end"}, {}, D::EffectKey{operation}}}},
         {{"end"}, name, D::NodeKind::Terminal, {}}}
    };
    const auto definition = D::compile(draft, {{}, {{operation}}}).value();
    auto session = D::Session::start(definition, {name}).value();
    const auto before = session.snapshot().value();
    const auto offered = session.view().value();

    bool observed = false;
    reject_allocation = true;
    try {
        void* pointer = ::operator new(1);
        ::operator delete(pointer);
    } catch (const std::bad_alloc&) {
        observed = true;
    }
    reject_allocation = false;
    if (!observed || rejected != 1)
        return 1;

    // Fail owned view/snapshot copies without resource exhaustion; busy must clear.
    for (const bool snapshot : {false, true}) {
        observed = false;
        reject_allocation = true;
        try {
            if (snapshot)
                static_cast<void>(session.snapshot());
            else
                static_cast<void>(session.view());
        } catch (const std::bad_alloc&) {
            observed = true;
        }
        reject_allocation = false;
        if (!observed || session.snapshot().value() != before)
            return 2;
    }

    int effects = 0;
    const D::EffectHandler effect = [&](const D::EffectKey& key) {
        if (&key != &*definition->entry().choices[0].effect)
            std::abort(); // The argument must borrow the stable key, not copy it.
        ++effects;
        reject_allocation = true;
    };
    const int prior_rejections = rejected;
    try {
        const auto result = session.choose(offered.choices[0].selection, {}, effect);
        reject_allocation = false; // Disable before inspecting/copying owned results.
        if (!result || effects != 1 || rejected != prior_rejections)
            return 3;
    } catch (...) {
        reject_allocation = false;
        return 4;
    }
    const auto after = session.snapshot().value();
    return after.node.value == "end" && after.step == 1 ? 0 : 5;
}
