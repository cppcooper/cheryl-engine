#include "storage.h"
#include <atomic>
#include <new>
#include <stdexcept>

namespace CE::Story {
    Family::Family(std::unique_ptr<Detail::Storage> storage) : storage_(std::move(storage)) {}
    Family::~Family() = default;

    std::expected<std::unique_ptr<Family>, Error> Family::create(const Seed& input, Limits limits) {
        const auto valid = Detail::validate_limits(limits);
        if (valid != Error::None) {
            return std::unexpected(valid);
        }
        static std::atomic<std::uint64_t> next_fence{1};
        auto fence = next_fence.load();
        do {
            if (fence == std::numeric_limits<std::uint64_t>::max()) {
                return std::unexpected(Error::Capacity);
            }
        } while (!next_fence.compare_exchange_weak(fence, fence + 1));
        try {
            auto storage = std::make_unique<Detail::Storage>(input.timeline, input.now, limits, fence);
            const auto error = Detail::seed(*storage, input);
            if (error != Error::None) {
                return std::unexpected(error);
            }
            return std::unique_ptr<Family>(new Family(std::move(storage)));
        } catch (const std::bad_alloc&) {
            return std::unexpected(Error::Capacity);
        } catch (const std::length_error&) {
            return std::unexpected(Error::Capacity);
        }
    }
    Capability Family::actor(PersonId id) const {
        Capability result;
        if (storage_->scoped(id) && storage_->people.find(id)) {
            result.owner_ = this;
            result.fence_ = storage_->fence;
            result.actor_ = id;
        }
        return result;
    }
    Error Family::retire_through(const Capability& capability, std::uint64_t attempt) {
        if (capability.owner_ != this || capability.fence_ != storage_->fence) {
            return Error::Authority;
        }
        auto* person = storage_->people.find(capability.actor_);
        if (!person || attempt < person->retired_through) {
            return Error::Expired;
        }
        person->retired_through = attempt;
        return Error::None;
    }
    std::optional<Person> Family::person(PersonId id) const {
        const auto* record = storage_->people.find(id);
        return storage_->scoped(id) && record ? std::optional<Person>{*record} : std::nullopt;
    }
    std::optional<ObjectState> Family::object(ObjectId id) const {
        const auto* record = storage_->objects.find(id);
        return storage_->scoped(id) && record ? std::optional<ObjectState>{*record} : std::nullopt;
    }
    std::optional<Commitment> Family::commitment(CommitmentId id) const {
        const auto* record = storage_->promises.find(id);
        return storage_->scoped(id) && record ? std::optional<Commitment>{*record} : std::nullopt;
    }
    std::optional<Episode> Family::episode(EpisodeId id) const {
        const auto* record = storage_->episodes.find(id);
        return storage_->scoped(id) && record ? std::optional<Episode>{*record} : std::nullopt;
    }
    std::expected<std::size_t, Error> Family::knowledge(const Capability& capability, std::span<Knowledge> output) const {
        if (capability.owner_ != this || capability.fence_ != storage_->fence) {
            return std::unexpected(Error::Authority);
        }
        const auto* person = storage_->people.find(capability.actor_);
        if (!person) {
            return std::unexpected(Error::Authority);
        }
        if (person->account_count > output.size() || person->account_count > storage_->limits.output_bytes / sizeof(Knowledge)) {
            return std::unexpected(Error::Capacity);
        }
        auto link = person->accounts;
        auto remaining = static_cast<std::size_t>(person->account_count);
        while (link != Detail::absent) {
            output[--remaining] = storage_->accounts.rows[link].value;
            link = storage_->accounts.rows[link].next;
        }
        return static_cast<std::size_t>(person->account_count);
    }
    Counters Family::counters() const { return storage_->counters; }
    std::size_t Family::episode_count() const { return storage_->episodes.rows.size(); }
    std::size_t Family::acquisition_count() const { return storage_->accounts.rows.size(); }
}
