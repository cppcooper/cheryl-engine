#include <host/host.h>

#include <limits>

namespace CE::LivingWorld {
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
