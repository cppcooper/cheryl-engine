#include "spill.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace ProcGen {
    namespace {
        struct Stop {
            SpillFailure failure;
        };
        [[noreturn]] void fail(FailureKind kind, SpillReason reason) {
            throw Stop{{kind, reason}};
        }
        Index add(Index a, Index b) {
            if (a > std::numeric_limits<Index>::max() - b) {
                fail(FailureKind::invalid_input, SpillReason::overflow);
            }
            return a + b;
        }
        Index multiply(Index a, Index b) {
            if (b != 0 && a > std::numeric_limits<Index>::max() / b) {
                fail(FailureKind::invalid_input, SpillReason::overflow);
            }
            return a * b;
        }
        void charge(Work& work, Index amount = 1) {
            if (!work.consume(amount)) {
                fail(FailureKind::budget_refused, SpillReason::work);
            }
        }
        SpillRequirements needed_for(const Domain& domain, Index outlets, Index queue_entries) {
            if (domain.dimensions() != 2) {
                fail(FailureKind::invalid_input, SpillReason::dimensions);
            }
            const auto count = domain.cells(), q = std::min(queue_entries, count);
            if (outlets == 0 || outlets > count) {
                fail(FailureKind::invalid_input, SpillReason::outlets);
            }
            if (q < outlets) {
                fail(FailureKind::budget_refused, SpillReason::queue_entries);
            }
            if (count > std::vector<SpillCell>{}.max_size() || q > std::vector<Index>{}.max_size()) {
                fail(FailureKind::invalid_input, SpillReason::overflow);
            }
            const auto bytes = add(drainage_fixed_storage, add(multiply(count, sizeof(SpillCell)), multiply(q, sizeof(Index))));
            const auto minimum = add(1024, add(multiply(12, count), q));
            Index levels = 0;
            for (auto last = q - 1; last != 0; last /= 2) {
                ++levels;
            }
            return {count,
                    q,
                    bytes,
                    minimum,
                    add(minimum, add(count, multiply(multiply(5, count), levels))),
                    multiply(count, UINT64_C(4294967295))};
        }
        // Fixed-capacity binary min heap, no duplicate/stale entries. All heap
        // comparisons/slot replacements/swaps are charged, including failed tests.
        class Frontier {
            std::vector<Index>& slots_;
            const std::vector<SpillCell>& cells_;
            Work& work_;
            SpillUsage& usage_;
            Index push_limit_, size_ = 0;
            void extra() {
                charge(work_);
                ++usage_.heap_extra_work;
            }
            bool less(Index a, Index b) {
                extra();
                const auto ah = cells_[static_cast<std::size_t>(a)].effective, bh = cells_[static_cast<std::size_t>(b)].effective;
                return ah < bh || (ah == bh && a < b);
            }
            void swap(Index a, Index b) {
                extra();
                std::swap(slots_[static_cast<std::size_t>(a)], slots_[static_cast<std::size_t>(b)]);
            }
        public:
            Frontier(std::vector<Index>& slots, const std::vector<SpillCell>& cells, Work& work, SpillUsage& usage, Index push_limit)
            : slots_(slots), cells_(cells), work_(work), usage_(usage), push_limit_(push_limit) {}
            bool empty() const { return size_ == 0; }
            void push(Index cell) {
                charge(work_);
                if (usage_.queue_pushes >= push_limit_) {
                    fail(FailureKind::budget_refused, SpillReason::queue_pushes);
                }
                if (size_ >= slots_.size()) {
                    fail(FailureKind::budget_refused, SpillReason::queue_entries);
                }
                charge(work_); // initial slot write, counted in mandatory base work
                auto position = size_++;
                slots_[static_cast<std::size_t>(position)] = cell;
                ++usage_.queue_pushes;
                usage_.peak_queue = std::max(usage_.peak_queue, size_);
                while (position != 0) {
                    const auto parent = (position - 1) / 2;
                    if (!less(slots_[static_cast<std::size_t>(position)], slots_[static_cast<std::size_t>(parent)])) {
                        break;
                    }
                    swap(position, parent);
                    position = parent;
                }
            }
            Index pop() {
                charge(work_);
                if (empty()) {
                    throw std::logic_error("Spill frontier underflow");
                }
                const auto result = slots_[0];
                --size_;
                ++usage_.queue_pops;
                if (size_ != 0) {
                    extra();
                    slots_[0] = slots_[static_cast<std::size_t>(size_)];
                    Index position = 0;
                    while (position < size_ / 2) {
                        const auto left = 2 * position + 1;
                        if (left >= size_) {
                            break;
                        }
                        const auto right = left + 1;
                        auto selected = left;
                        if (right < size_ && less(slots_[static_cast<std::size_t>(right)], slots_[static_cast<std::size_t>(left)])) {
                            selected = right;
                        }
                        if (!less(slots_[static_cast<std::size_t>(selected)], slots_[static_cast<std::size_t>(position)])) {
                            break;
                        }
                        swap(position, selected);
                        position = selected;
                    }
                }
                return result;
            }
        };
        static_assert(sizeof(SpillCell) == 40 && sizeof(Index) == 8);
        static_assert(2 * sizeof(Spill) + 2 * sizeof(SpillDescriptor) + sizeof(Frontier) + 1024 <= fixed_storage_allowance);
    }
    std::string_view spill_reason_text(SpillReason reason) {
        switch (reason) {
            case SpillReason::dimensions:
                return "spill requires a checked positive two-axis domain";
            case SpillReason::height_count:
                return "spill height count must equal domain cells";
            case SpillReason::outlets:
                return "spill requires nonempty distinct in-domain outlets";
            case SpillReason::overflow:
                return "spill index/payload/work/volume arithmetic or capacity overflow";
            case SpillReason::cells:
                return "spill cell budget refused";
            case SpillReason::storage:
                return "spill accounted payload budget refused";
            case SpillReason::work:
                return "spill work budget refused or exhausted";
            case SpillReason::queue_entries:
                return "spill frontier capacity refused or exhausted";
            case SpillReason::queue_pushes:
                return "spill queue-push budget refused or exhausted";
            case SpillReason::algorithm:
                return "unsupported spill algorithm";
            case SpillReason::version:
                return "unsupported spill version";
            case SpillReason::index:
                return "spill index outside domain";
        }
        return "unknown spill failure";
    }
    SpillResult<SpillRequirements> spill_requirements(const Domain& domain, Index outlet_count, Index queue_entries) {
        try {
            return needed_for(domain, outlet_count, queue_entries);
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }
    Spill::Spill(SpillDescriptor descriptor, std::vector<SpillCell> cells, SpillMeasurements measured, SpillUsage usage)
    : descriptor_(std::move(descriptor)), cells_(std::move(cells)), measured_(measured), usage_(usage) {}
    SpillResult<SpillCell> Spill::cell(Index index) const {
        if (index >= cells_.size()) {
            return std::unexpected(SpillFailure{FailureKind::invalid_input, SpillReason::index});
        }
        return cells_[static_cast<std::size_t>(index)];
    }
    SpillResult<Spill> analyze_spill(DrainageInput input, SpillBudgets budgets, SpillOptions options) {
        try {
            if (options.algorithm != SpillAlgorithm::priority_flood) {
                fail(FailureKind::unsupported, SpillReason::algorithm);
            }
            if (options.version != 1) {
                fail(FailureKind::unsupported, SpillReason::version);
            }
            const auto& d = input.domain;
            const SpillBudgets cap{
                std::min(budgets.cells, spill_hard_limits.cells), std::min(budgets.storage_bytes, spill_hard_limits.storage_bytes),
                std::min(budgets.work, spill_hard_limits.work), std::min(budgets.queue_entries, spill_hard_limits.queue_entries),
                std::min(budgets.queue_pushes, spill_hard_limits.queue_pushes)
            };
            const auto needed = needed_for(d, input.outlets.size(), cap.queue_entries);
            if (needed.cells > cap.cells) {
                fail(FailureKind::budget_refused, SpillReason::cells);
            }
            if (needed.storage_bytes > cap.storage_bytes) {
                fail(FailureKind::budget_refused, SpillReason::storage);
            }
            if (needed.minimum_work > cap.work) {
                fail(FailureKind::budget_refused, SpillReason::work);
            }
            if (needed.cells > cap.queue_pushes) {
                fail(FailureKind::budget_refused, SpillReason::queue_pushes);
            }
            if (input.heights.size() != needed.cells) {
                fail(FailureKind::invalid_input, SpillReason::height_count);
            }
            Work work(cap.work);
            SpillUsage usage{};
            charge(work, 1024);
            std::array<std::uint64_t, 1024> outlets{};
            for (auto index : input.outlets) {
                charge(work);
                if (index >= needed.cells) {
                    fail(FailureKind::invalid_input, SpillReason::outlets);
                }
                auto& word = outlets[static_cast<std::size_t>(index / 64)];
                const auto bit = UINT64_C(1) << (index % 64);
                if ((word & bit) != 0) {
                    fail(FailureKind::invalid_input, SpillReason::outlets);
                }
                word |= bit;
            }
            charge(work, add(needed.cells, needed.queue_capacity));
            std::vector<SpillCell> cells(static_cast<std::size_t>(needed.cells));
            std::vector<Index> slots(static_cast<std::size_t>(needed.queue_capacity), 0);
            for (Index i = 0; i < needed.cells; ++i) {
                charge(work);
                cells[static_cast<std::size_t>(i)].original = cells[static_cast<std::size_t>(i)].effective =
                    input.heights[static_cast<std::size_t>(i)];
            }
            Frontier frontier(slots, cells, work, usage, cap.queue_pushes);
            for (Index i = 0; i < needed.cells; ++i) {
                charge(work);
                if ((outlets[static_cast<std::size_t>(i / 64)] & (UINT64_C(1) << (i % 64))) != 0) {
                    cells[static_cast<std::size_t>(i)].outlet = i;
                    frontier.push(i);
                }
            }
            SpillMeasurements measured{};
            measured.cells = needed.cells;
            measured.outlets = input.outlets.size();
            measured.minimum_effective = std::numeric_limits<Height>::max();
            measured.maximum_effective = std::numeric_limits<Height>::min();
            Index settled = 0;
            while (!frontier.empty()) {
                const auto index = frontier.pop();
                charge(work);
                auto& cell = cells[static_cast<std::size_t>(index)];
                if (cell.rank != no_receiver || cell.effective < cell.original) {
                    throw std::logic_error("Spill settle invariant failed");
                }
                cell.rank = settled++;
                cell.depth = static_cast<Index>(static_cast<std::int64_t>(cell.effective) - cell.original);
                measured.filled_cells += static_cast<Index>(cell.depth != 0);
                measured.maximum_depth = std::max(measured.maximum_depth, cell.depth);
                measured.volume = add(measured.volume, cell.depth);
                measured.minimum_effective = std::min(measured.minimum_effective, cell.effective);
                measured.maximum_effective = std::max(measured.maximum_effective, cell.effective);
                for (std::size_t direction = 0; direction < 4; ++direction) {
                    charge(work); // outside / already discovered attempts count too
                    const auto neighbor = d.neighbor(index, direction / 2, static_cast<Direction>(direction % 2)).value();
                    if (!neighbor) {
                        continue;
                    }
                    auto& next = cells[static_cast<std::size_t>(*neighbor)];
                    if (next.outlet != no_receiver) {
                        continue;
                    }
                    charge(work);
                    next.effective = std::max(next.original, cell.effective);
                    next.parent = index;
                    next.outlet = cell.outlet; // marks discovered before enqueue
                    frontier.push(*neighbor);
                }
            }
            if (settled != needed.cells || usage.queue_pushes != needed.cells || measured.volume > needed.maximum_volume) {
                throw std::logic_error("Spill coverage/volume invariant failed");
            }
            usage.work = work.used();
            return Spill({d, budgets, options}, std::move(cells), measured, usage);
        } catch (const Stop& stop) {
            return std::unexpected(stop.failure);
        }
    }
}
