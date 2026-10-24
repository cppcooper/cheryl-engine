#include <cheryl/core/game-framework/dialogue.h>

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <utility>

// Standalone first-use probe only; never link these operators into GoogleTest.
namespace {
    thread_local bool deny = false;
    thread_local unsigned denied = 0;

    void* allocate(std::size_t size, std::size_t alignment = 0) {
        if (deny) {
            ++denied;
            throw std::bad_alloc{};
        }
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
void operator delete(void* pointer) noexcept {
    std::free(pointer);
}
void operator delete[](void* pointer) noexcept {
    std::free(pointer);
}
void operator delete(void* pointer, std::size_t) noexcept {
    std::free(pointer);
}
void operator delete[](void* pointer, std::size_t) noexcept {
    std::free(pointer);
}
void operator delete(void* pointer, std::align_val_t) noexcept {
    std::free(pointer);
}
void operator delete[](void* pointer, std::align_val_t) noexcept {
    std::free(pointer);
}
void operator delete(void* pointer, std::size_t, std::align_val_t) noexcept {
    std::free(pointer);
}
void operator delete[](void* pointer, std::size_t, std::align_val_t) noexcept {
    std::free(pointer);
}

int main() {
    namespace D = CE::Dialogue;
    const std::string prefix(1024, 'k');
    const std::vector<D::NodeId> nodes{{prefix + "z"}, {prefix + std::string("\0a", 2)}, {prefix}, {prefix + "terminal"}};
    const std::vector<D::ChoiceId> choices{{prefix + "z"}, {prefix + "a"}, {prefix + std::string("\0x", 2)}, {prefix}};
    const D::NodeId missing_node{prefix + std::string("\0b", 2)};
    const D::ChoiceId missing_choice{prefix + std::string("\0y", 2)};
    const auto definition = [&] {
        D::Draft draft{{"allocation-lookup"}, 1, nodes[1], {}};
        for (const auto& id : nodes) {
            D::Node node{id, "", D::NodeKind::Prompt, {}};
            if (id == nodes.back())
                node.kind = D::NodeKind::Terminal;
            else
                for (const auto& key : choices)
                    node.choices.push_back({key, "", nodes[0], {}, {}});
            draft.nodes.push_back(std::move(node));
        }
        return D::compile(draft, {}).value();
    }();

    // Verify ordinary/array/aligned denial before any call to either lookup.
    for (int kind = 0; kind < 3; ++kind) {
        bool observed = false;
        deny = true;
        try {
            void* pointer;
            if (kind == 0)
                pointer = ::operator new(1);
            else if (kind == 1)
                pointer = ::operator new[](1);
            else
                pointer = ::operator new(1, std::align_val_t{64});
            deny = false;
            if (kind == 0)
                ::operator delete(pointer);
            else if (kind == 1)
                ::operator delete[](pointer);
            else
                ::operator delete(pointer, std::align_val_t{64});
        } catch (const std::bad_alloc&) {
            observed = true;
        }
        deny = false;
        if (!observed || denied != static_cast<unsigned>(kind + 1))
            return 1;
    }
    // Confirm that Engine's allocation-bearing path reaches this executable's
    // hook too. No lookup is warmed up; the already-frozen definition survives.
    const D::Draft control{{"control"}, 1, {"end"}, {{{"end"}, "", D::NodeKind::Terminal, {}}}};
    bool engine_observed = false;
    deny = true;
    try {
        static_cast<void>(D::compile(control, {}));
    } catch (const std::bad_alloc&) {
        engine_observed = true;
    }
    deny = false;
    if (!engine_observed || denied != 4)
        return 3;
    bool correct = true;
    deny = true;
    for (int repetition = 0; repetition < 100; ++repetition) {
        for (std::size_t row = 0; row < nodes.size(); ++row) {
            const auto& authored = definition->nodes()[row];
            correct = (definition->find_node(nodes[row]) == &authored) && correct;
            for (std::size_t choice = 0; choice < authored.choices.size(); ++choice)
                correct = (definition->find_choice(nodes[row], choices[choice]) == &authored.choices[choice]) && correct;
            correct = (definition->find_choice(nodes[row], missing_choice) == nullptr) && correct;
        }
        correct = (definition->find_node(missing_node) == nullptr) && correct;
        correct = (definition->find_choice(missing_node, choices[0]) == nullptr) && correct;
        correct = (definition->find_choice(nodes.back(), choices[0]) == nullptr) && correct;
    }
    deny = false;
    if (!correct || denied != 4)
        return 2;
    std::puts(
        "Allocation-denied first-use lookup hits/misses and authored pointers passed (scalar/array/aligned and shared Engine controls verified)."
    );
    return 0;
}
