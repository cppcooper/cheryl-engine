#include <host/host.h>

#include <limits>

namespace CE::LivingWorld {
    std::expected<ActionDelivery, Error> Host::delivery(const Access& access, const Action& action) const noexcept {
        const auto receipt = lookup(access, action);
        if (!receipt)
            return std::unexpected(receipt.error());
        if (image_->work_count > image_->work.size() || image_->acquired_count > image_->acquired.size())
            return std::unexpected(Error::Preparation);
        const Detail::Record* retained = nullptr;
        for (std::size_t index = 0; index < image_->record_count; ++index)
            if (image_->records[index].action.key == action.key) {
                retained = &image_->records[index];
                break;
            }
        if (!retained || retained->witness_count > retained->witnesses.size() ||
            ((receipt->state != Disposition::Applied || !receipt->gain) && retained->witness_count))
            return std::unexpected(Error::Preparation);
        ActionDelivery result{*receipt, retained->witness_count};
        std::size_t work_count = 0, acquired_count = 0;
        for (std::size_t index = 0; index < image_->work_count; ++index)
            work_count += image_->work[index].cause == action.key;
        for (std::size_t index = 0; index < image_->acquired_count; ++index)
            acquired_count += image_->acquired[index].cause == action.key;
        if (work_count != result.required)
            return std::unexpected(Error::Preparation);
        for (std::size_t witness = 0; witness < retained->witness_count; ++witness) {
            const auto ref = retained->witnesses[witness];
            for (std::size_t previous = 0; previous < witness; ++previous)
                if (retained->witnesses[previous] == ref)
                    return std::unexpected(Error::Preparation);
            const Detail::ObservationWork* required = nullptr;
            for (std::size_t index = 0; index < image_->work_count; ++index) {
                const auto& work = image_->work[index];
                if (work.cause != action.key || work.witness != ref)
                    continue;
                if (required || work.subject != action.body.recipient || work.observed_at != retained->observed_at)
                    return std::unexpected(Error::Preparation);
                required = &work;
            }
            if (!required)
                return std::unexpected(Error::Preparation);
            std::size_t acquired = 0;
            for (std::size_t index = 0; index < image_->acquired_count; ++index) {
                const auto& entry = image_->acquired[index];
                if (entry.cause != action.key || entry.witness != ref)
                    continue;
                if (entry.subject != required->subject || entry.observed_at != required->observed_at ||
                    !entry.local_id || entry.acquired_at < entry.observed_at)
                    return std::unexpected(Error::Preparation);
                ++acquired;
            }
            if (acquired != static_cast<std::size_t>(required->consumed))
                return std::unexpected(Error::Preparation);
            result.acquired += acquired;
        }
        if (acquired_count != result.acquired)
            return std::unexpected(Error::Preparation);
        result.outstanding = result.required - result.acquired;
        return result;
    }

    Delivery Host::deliver() noexcept {
        Delivery result;
        constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
        if (reserved_ || image_->work_count == 0)
            return result;
        if (image_->revision > maximum - 2 || image_->tick == maximum) {
            result.backpressure = true;
            return result;
        }
        *staging_ = *image_;
        bool changed = false;
        for (std::size_t index = 0; index < staging_->work_count; ++index) {
            auto& work = staging_->work[index];
            if (work.consumed)
                continue;
            const auto witness = actor_index(work.witness);
            if (!witness || !image_->actors[*witness].access) {
                result.withheld = true;
                continue;
            }
            bool duplicate = false;
            std::uint64_t next_id = 1;
            for (std::size_t acquired = 0; acquired < staging_->acquired_count; ++acquired) {
                const auto& entry = staging_->acquired[acquired];
                if (entry.witness == work.witness) {
                    ++next_id;
                    duplicate = duplicate || entry.cause == work.cause;
                }
            }
            if (duplicate) {
                work.consumed = true;
                changed = true;
                continue;
            }
            if (staging_->acquired_count == staging_->acquired.size()) {
                result.backpressure = true;
                continue;
            }
            staging_->acquired[staging_->acquired_count++] = {
                work.cause, work.witness, work.subject, next_id, work.observed_at, image_->tick + 1
            };
            work.consumed = true;
            changed = true;
            ++result.adopted;
        }
        if (changed) {
            ++staging_->revision;
            ++staging_->tick;
            publish();
        }
        return result;
    }

    std::expected<Page, Error> Host::read(const Access& access, std::optional<Cursor> cursor) const noexcept {
        if (!authorized(access))
            return std::unexpected(Error::Unauthorized);
        if (cursor && (cursor->recipient != access.principal_ || cursor->incarnation != image_->fence))
            return std::unexpected(Error::Stale);
        Page page;
        page.next = {access.principal_, image_->fence, cursor ? cursor->after : 0};
        for (std::size_t index = 0; index < image_->acquired_count; ++index) {
            const auto& entry = image_->acquired[index];
            if (entry.witness == access.principal_ && entry.local_id > page.next.after) {
                page.wound_closures[page.size++] = {
                    entry.local_id, entry.subject, Source::Eyewitness, entry.observed_at, entry.acquired_at
                };
                page.next.after = entry.local_id;
            }
        }
        return page;
    }
}
