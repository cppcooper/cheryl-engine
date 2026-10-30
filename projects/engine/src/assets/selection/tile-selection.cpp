#include <assets/selection/tile-selection.h>
#include <internals/exceptions.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

namespace CE::Assets::Tilesets::Selection {
    namespace {
        constexpr std::array directions{Direction::North, Direction::NorthEast, Direction::East, Direction::SouthEast,
            Direction::South, Direction::SouthWest, Direction::West, Direction::NorthWest};

        void validate_fallback(const Fallback fallback) {
            if (fallback != Fallback::Empty && fallback != Fallback::Center && fallback != Fallback::Unresolved)
                throw Exceptions::invalid_args(CE_HERE, "Unknown tile terrain fallback policy");
        }

        void validate_options(const Sampler& sampler, const Options& options) {
            if (!sampler)
                throw Exceptions::invalid_args(CE_HERE, "Tile selection needs a terrain sampler");
            validate_fallback(options.outside);
            validate_fallback(options.unknown);
            if (options.diagonals != DiagonalConnectivity::Independent && options.diagonals != DiagonalConnectivity::RequireCardinals)
                throw Exceptions::invalid_args(CE_HERE, "Unknown diagonal connectivity policy");
        }

        bool known_terrain(const WangRule& rule, const std::uint32_t terrain) {
            return terrain == 0 || std::ranges::any_of(rule.terrains, [terrain](const auto& entry) { return entry.id == terrain; });
        }

        std::optional<std::uint32_t> sample_terrain(const Sample sample, const Options& options, const WangRule* wang) {
            if (sample.state == SampleState::Known && (!wang || known_terrain(*wang, sample.terrain)))
                return sample.terrain;

            Fallback fallback;
            switch (sample.state) {
                case SampleState::Outside:
                    fallback = options.outside;
                    break;
                case SampleState::Known: // A label absent from the Wang terrain catalogue is unknown.
                case SampleState::Unknown:
                    fallback = options.unknown;
                    break;
                default:
                    throw Exceptions::invalid_args(CE_HERE, "Unknown terrain sample state");
            }
            switch (fallback) {
                case Fallback::Empty:
                    return 0;
                case Fallback::Center:
                    return options.terrain;
                case Fallback::Unresolved:
                    return std::nullopt;
            }
            throw Exceptions::invalid_args(CE_HERE, "Unknown tile terrain fallback policy");
        }

        std::optional<WangSignature> sample_sites(
            const std::array<bool, 8>& required,
            const SiteKind kind,
            const Sampler& sampler,
            const Options& options,
            const WangRule* wang = nullptr
        ) {
            WangSignature samples{};
            bool complete = true;
            for (std::size_t index = 0; index < directions.size(); ++index) {
                if (!required[index])
                    continue;
                const auto terrain = sample_terrain(sampler(Site{kind, directions[index]}), options, wang);
                if (terrain)
                    samples[index] = *terrain;
                else
                    complete = false;
            }
            if (!complete)
                return std::nullopt;
            return samples;
        }

        std::uint64_t mix_seed(std::uint64_t seed) {
            // Fixed unsigned arithmetic; no library distribution, clock or shared RNG state.
            seed += 0x9e3779b97f4a7c15ULL;
            seed = (seed ^ (seed >> 30)) * 0xbf58476d1ce4e5b9ULL;
            seed = (seed ^ (seed >> 27)) * 0x94d049bb133111ebULL;
            return seed ^ (seed >> 31);
        }

        CE::Assets::Grids::CellIndex weighted_cell(
            const WangRule& rule,
            const WangSignature& signature,
            const std::vector<std::size_t>& candidates,
            const std::uint64_t seed
        ) {
            double maximum = 0.0;
            std::optional<std::size_t> previous;
            for (const auto index : candidates) {
                if (index >= rule.tiles.size() || (previous && index <= *previous))
                    throw Exceptions::invalid_args(CE_HERE, "Wang candidates must index tiles in definition order");
                const auto& tile = rule.tiles[index];
                if (tile.wang != signature || !std::isfinite(tile.weight) || tile.weight <= 0.0)
                    throw Exceptions::invalid_args(CE_HERE, "Wang candidate signature or weight is invalid");
                maximum = std::max(maximum, tile.weight);
                previous = index;
            }

            double total = 0.0;
            for (const auto index : candidates)
                total += rule.tiles[index].weight / maximum;
            const auto fraction = static_cast<double>(mix_seed(seed) >> 11) * 0x1.0p-53;
            const auto target = fraction * total;
            double cumulative = 0.0;
            for (const auto index : candidates) {
                cumulative += rule.tiles[index].weight / maximum;
                if (target < cumulative)
                    return rule.tiles[index].cell;
            }
            return rule.tiles[candidates.back()].cell; // Covers accumulation rounding at the upper boundary.
        }
    }

    Result select(const WangRule& rule, const Sampler& sampler, const Options& options) {
        validate_options(sampler, options);
        if (rule.type != WangType::Corner && rule.type != WangType::Edge)
            throw Exceptions::invalid_args(CE_HERE, "Unknown Wang rule type");
        if (rule.slot_order != directions)
            throw Exceptions::invalid_args(CE_HERE, "Wang slots must use canonical direction order");
        if ((options.outside == Fallback::Center || options.unknown == Fallback::Center) && !known_terrain(rule, options.terrain))
            throw Exceptions::invalid_args(CE_HERE, "Wang center fallback needs a declared terrain ID or zero");

        std::array<bool, 8> required{};
        for (std::size_t index = 0; index < required.size(); ++index)
            required[index] = (index % 2 == 0) == (rule.type == WangType::Edge);
        const auto signature =
            sample_sites(required, rule.type == WangType::Edge ? SiteKind::Edge : SiteKind::Corner, sampler, options, &rule);
        if (!signature)
            return Failure::IncompleteNeighborhood;
        const auto candidates = rule.variants.find(*signature);
        if (candidates == rule.variants.end() || candidates->second.empty())
            return Failure::MissingRule;
        return weighted_cell(rule, *signature, candidates->second, options.seed);
    }

    Result select(const BitmaskRule& rule, const Sampler& sampler, const Options& options) {
        validate_options(sampler, options);
        if (rule.type != BitmaskType::FourNeighbor && rule.type != BitmaskType::EightNeighbor)
            throw Exceptions::invalid_args(CE_HERE, "Unknown bitmask rule type");
        const auto maximum = rule.type == BitmaskType::FourNeighbor ? 4u : 8u;
        if (rule.bit_order.empty() || rule.bit_order.size() > maximum)
            throw Exceptions::invalid_args(CE_HERE, "Bitmask direction count is invalid");
        std::array<bool, 8> declared{};
        std::array<bool, 8> required{};
        for (const auto direction : rule.bit_order) {
            const auto index = static_cast<std::size_t>(direction);
            if (index >= directions.size() || declared[index] || (rule.type == BitmaskType::FourNeighbor && index % 2 != 0))
                throw Exceptions::invalid_args(CE_HERE, "Bitmask directions must be unique and compatible with their type");
            declared[index] = true;
            required[index] = true;
            if (index % 2 != 0 && options.diagonals == DiagonalConnectivity::RequireCardinals) {
                required[(index + 7) % 8] = true;
                required[(index + 1) % 8] = true;
            }
        }
        const auto samples = sample_sites(required, SiteKind::Cell, sampler, options);
        if (!samples)
            return Failure::IncompleteNeighborhood;

        const auto connects = [&](const std::size_t index) { return options.terrain != 0 && (*samples)[index] == options.terrain; };
        std::uint32_t mask = 0;
        for (std::size_t bit = 0; bit < rule.bit_order.size(); ++bit) {
            const auto index = static_cast<std::size_t>(rule.bit_order[bit]);
            const auto joined = connects(index) &&
                (index % 2 == 0 || options.diagonals == DiagonalConnectivity::Independent ||
                    (connects((index + 7) % 8) && connects((index + 1) % 8)));
            if (joined)
                mask |= std::uint32_t{1} << bit;
        }
        const auto cell = rule.cases.find(mask);
        if (cell == rule.cases.end())
            return Failure::MissingRule;
        return cell->second;
    }

    Result select(const Rule& rule, const Sampler& sampler, const Options& options) {
        return std::visit([&](const auto& definition) { return select(definition, sampler, options); }, rule);
    }
}
