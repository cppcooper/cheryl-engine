#include "generated-document.h"

#include <algorithm>
#include <limits>

namespace WorldProof {
    namespace {
        namespace Patch = Storage::CellPatches;
        [[noreturn]] void refuse(const std::string& detail) {
            throw Error(Refusal::Generation, "Generated document: " + detail);
        }
        std::size_t add(std::size_t a, std::size_t b) {
            if (b > std::numeric_limits<std::size_t>::max() - a)
                refuse("byte/work product overflows");
            return a + b;
        }
        std::size_t multiply(std::size_t a, std::size_t b) {
            if (a && b > std::numeric_limits<std::size_t>::max() / a)
                refuse("byte/work product overflows");
            return a * b;
        }
        void limits(const GeneratedLimits& cap) {
            if (cap.edits > Patch::max_edits || cap.witness_bytes > Patch::max_witness_bytes || cap.retained_bytes > 32 * 1024 * 1024 ||
                cap.work > 4 * 1024 * 1024)
                refuse("limits exceed private hard ceilings");
        }
        void meaning(const std::string& key) {
            const auto known = editor_semantics();
            if (std::ranges::find(known, key, &SemanticKey::key) == known.end())
                refuse("unknown semantic key " + key);
        }
        std::size_t admit(const CaveRecipe& recipe, const GeneratedLimits& cap) {
            limits(cap);
            const auto& domain = recipe.cave.domain;
            if (recipe.family != "cave" || recipe.adapter.version != 1 || recipe.adapter.axes != CaveAxes::XRightYDown)
                refuse("unsupported family/adapter version/axes");
            if (domain.dimensions() != 2 || domain.extents()[0] > editor_max_axis || domain.extents()[1] > editor_max_axis)
                refuse("requires finite two-dimensional axes1..64");
            if (recipe.adapter.spawns.size() > Storage::max_spawns)
                refuse("too many adapter spawns");
            const auto& budget = recipe.cave.budgets;
            if (budget.cells > ProcGen::cave_hard_limits.cells || budget.storage_bytes > ProcGen::cave_hard_limits.storage_bytes ||
                budget.work > ProcGen::cave_hard_limits.work)
                refuse("CaveBudgets exceed hard ceilings; no silent normalization");
            for (const auto& item : recipe.adapter.occupancy) {
                const auto known = editor_semantics();
                auto found = std::ranges::find(known, item.key, &SemanticKey::key);
                if (found == known.end() || found->label != item.label)
                    refuse("unknown/inconsistent occupancy key/label policy");
            }
            if (recipe.adapter.occupancy[0] == recipe.adapter.occupancy[1])
                refuse("floor/solid meanings must differ");
            std::size_t bytes = sizeof(CaveRecipe) + sizeof(GeneratedLimits) + sizeof(ProcGen::CaveConfiguration);
            const auto text = [&](const std::string& value) {
                if (value.size() > Storage::max_key_bytes)
                    refuse("adapter text exceeds key limit");
                bytes = add(bytes, value.size());
            };
            text(recipe.family);
            text(recipe.adapter.id);
            for (const auto& layer : recipe.adapter.layers)
                text(layer);
            for (const auto& item : recipe.adapter.occupancy)
                text(item.key);
            bytes = add(bytes, multiply(recipe.adapter.spawns.size(), sizeof(Storage::Spawn)));
            for (const auto& spawn : recipe.adapter.spawns) {
                text(spawn.name);
                text(spawn.layer);
            }
            if (!recipe.adapter.initial_detail.empty() && recipe.adapter.initial_detail.size() != domain.cells())
                refuse("initial detail must be absent or contain exactly domain.cells semantic keys/nulls");
            bytes = add(bytes, multiply(recipe.adapter.initial_detail.size(), sizeof(std::optional<std::string>)));
            for (const auto& key : recipe.adapter.initial_detail)
                if (key) {
                    text(*key);
                    meaning(*key);
                }
            if (bytes > 64 * 1024)
                refuse("recipe payload exceeds64 KiB");
            const auto cells = static_cast<std::size_t>(domain.cells()); // <=4096 before narrowing
            const auto work = add(add(multiply(96, multiply(2, cells)), multiply(8, cap.edits)), recipe.adapter.spawns.size());
            if (work > cap.work)
                refuse("adapter scan/copy work reservation exceeds cap");
            return bytes;
        }
        void target(const Storage::Snapshot& snapshot) {
            admit_editor_document(snapshot);
            if (!snapshot.id.starts_with("terrain:"))
                refuse("semantic map ID must use terrain: prefix");
            for (const auto& layer : snapshot.layers)
                for (const auto& key : layer.cells)
                    if (key)
                        meaning(*key);
        }
        GeneratedDocument account(GeneratedDocument result, const Storage::Snapshot& final) {
            result.bytes.scan_passes = 64;
            result.bytes.copy_passes = 32;
            result.bytes.recipe = admit(result.recipe, result.limits);
            result.bytes.base = Storage::encode(result.base).size();
            result.bytes.witness = result.overlay.witness.canonical_bytes().size();
            if (result.bytes.witness > result.limits.witness_bytes)
                refuse("canonical witness exceeds byte cap");
            if (result.overlay.edits.size() > result.limits.edits)
                refuse("final changed-address cap exceeded");
            result.bytes.edits = 0;
            for (const auto& edit : result.overlay.edits) {
                // Logical fields: layer bytes, two u32 coordinates, presence byte, optional key bytes.
                result.bytes.edits = add(result.bytes.edits, add(edit.layer.size(), 9));
                if (edit.tile)
                    result.bytes.edits = add(result.bytes.edits, edit.tile->size());
            }
            result.bytes.retained = add(add(result.bytes.recipe, result.bytes.base), add(result.bytes.witness, result.bytes.edits));
            if (result.bytes.retained > result.limits.retained_bytes)
                refuse("retained recipe/base/witness/edit bytes exceed cap");
            result.bytes.full_snapshot = Storage::encode(final).size();
            result.bytes.reserved_work =
                add(add(multiply(96, multiply(2, static_cast<std::size_t>(result.recipe.cave.domain.cells()))),
                        multiply(8, result.limits.edits)),
                    result.recipe.adapter.spawns.size());
            return result;
        }
        void witness(const GeneratedDocument& held) {
            static_cast<void>(admit(held.recipe, held.limits));
            if (held.effective != held.recipe.cave.requested)
                refuse("retained requested/effective configuration disagreement");
            target(held.base);
            if (Patch::capture(held.base) != held.overlay.witness)
                refuse("retained semantic base differs from complete witness");
            if (held.overlay.edits.size() > held.limits.edits || held.overlay.witness.canonical_bytes().size() > held.limits.witness_bytes)
                refuse("retained overlay/witness exceeds cap");
            // Validate edit addresses/keys even for copies presented to this private adapter.
            const auto final = Patch::apply(held.base, held.overlay);
            target(final);
            static_cast<void>(account(held, final));
        }
    }
    GeneratedDocument generate_document(const CaveRecipe& recipe, const GeneratedLimits cap) {
        static_cast<void>(admit(recipe, cap));
        auto cave = ProcGen::replay_cave(recipe.cave);
        if (!cave)
            refuse("cave replay refused: " + std::string(ProcGen::reason_text(cave.error().reason)));
        const auto width = static_cast<std::uint32_t>(recipe.cave.domain.extents()[0]);
        const auto height = static_cast<std::uint32_t>(recipe.cave.domain.extents()[1]);
        Storage::Snapshot base{
            recipe.adapter.id, width, height, {{recipe.adapter.layers[0], {}}, {recipe.adapter.layers[1], {}}}, recipe.adapter.spawns
        };
        base.layers[0].cells.reserve(cave->occupancy().size());
        if (recipe.adapter.initial_detail.empty())
            base.layers[1].cells.resize(cave->occupancy().size());
        else
            base.layers[1].cells = recipe.adapter.initial_detail;
        for (const auto value : cave->occupancy()) {
            if (value > 1)
                refuse("cave occupancy outside floor/solid domain");
            base.layers[0].cells.emplace_back(recipe.adapter.occupancy[value].key);
        }
        target(base);
        auto patch = Patch::diff(base, base);
        if (patch.witness != Patch::capture(base))
            refuse("diff/capture witness disagreement");
        GeneratedDocument result{recipe, cave->effective(), cap, base, std::move(patch), {}};
        result.bytes.cave_storage = ProcGen::cave_requirements(recipe.cave.domain, recipe.cave.requested)->storage_bytes;
        result.bytes.cave_work = cave->usage().work;
        return account(std::move(result), base);
    }
    GeneratedDocument stage_generated_edits(const GeneratedDocument& previous, const Storage::Snapshot& final) {
        witness(previous);
        target(final);
        auto patch = Patch::diff(previous.base, final); // Full metadata check, final unique row-major changes.
        if (patch.edits.size() > previous.limits.edits)
            refuse("final changed-address cap exceeded");
        if (Patch::apply(previous.base, patch) != final)
            refuse("final patch application differs from full semantic target");
        auto result = previous;
        result.overlay = std::move(patch);
        return account(std::move(result), final);
    }
    Storage::Snapshot replay_generated(const GeneratedDocument& held, const CaveRecipe& proposal) {
        witness(held);
        auto generated = generate_document(proposal, held.limits);
        auto old = held.recipe.cave;
        old.budgets = proposal.cave.budgets;
        if (old != proposal.cave || held.recipe.family != proposal.family || held.recipe.adapter != proposal.adapter)
            refuse("preserve-edits requires unchanged recipe/adapter inputs; choose explicit discard for a new epoch");
        if (generated.base != held.base || generated.overlay.witness != held.overlay.witness)
            refuse("replayed semantic base differs from complete retained witness");
        auto final = Patch::apply(generated.base, held.overlay);
        target(final);
        return final;
    }
}
