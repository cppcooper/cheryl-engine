#include <core/resources/asset-management/asset-catalogue.h>
#include <internals/compile-time-logging.hpp>

namespace CE::Assets::Cache::Detail {
    void report_catalogue_attempt(
        std::uint64_t source,
        std::uint64_t revision,
        CatalogueChangeKind kind,
        std::uint64_t lost,
        std::size_t pending
    ) noexcept {
        if (revision == 1 || revision % 120 == 0 || kind != CatalogueChangeKind::Published || lost)
            CE_LOG_DEBUG(
                CE::Logging::Categories::assets,
                "subsystem=assets event=asset-catalogue-changed operation=notification_attempt source={} revision={} kind={} lost={} pending={} sample_period=120",
                source, revision,
                kind == CatalogueChangeKind::Published  ? "published"
                : kind == CatalogueChangeKind::Replaced ? "replaced"
                                                        : "reset",
                lost, pending
            );
    }
}
