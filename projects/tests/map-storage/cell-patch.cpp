#include "cell-patch.h"

#include <algorithm>
#include <set>
#include <utility>

namespace CellPatches {
    namespace {
        namespace Maps = CE::Maps;

        [[noreturn]] void reject(Code code, const std::string& operation, const std::string& location,
                                 const std::string& detail) {
            throw Error(code, operation, location, detail);
        }

        void admit_shape(const Maps::Snapshot& base, const std::string& operation) {
            if (base.width == 0 || base.width > Maps::max_dimension)
                reject(Code::InvalidBase, operation, "$.base.width", "invalid finite width");
            if (base.height == 0 || base.height > Maps::max_dimension)
                reject(Code::InvalidBase, operation, "$.base.height", "invalid finite height");
            if (base.layers.empty() || base.layers.size() > Maps::max_layers)
                reject(Code::InvalidBase, operation, "$.base.layers", "invalid layer count");
            if (base.spawns.size() > Maps::max_spawns)
                reject(Code::InvalidBase, operation, "$.base.spawns", "invalid spawn count");
            // Divide before multiplying; no candidate allocation occurs here.
            if (base.width > max_base_cells / base.height)
                reject(Code::LimitExceeded, operation, "$.base", "private total-cell budget exceeded");
            const auto area = std::size_t{base.width} * base.height;
            if (base.layers.size() > max_base_cells / area)
                reject(Code::LimitExceeded, operation, "$.base.layers", "private total-cell budget exceeded");
            for (std::size_t i = 0; i < base.layers.size(); ++i) {
                if (base.layers[i].cells.size() != area)
                    reject(Code::InvalidBase, operation, "$.base.layers[" + std::to_string(i) + "].cells",
                           "cell count differs from dimensions");
            }
        }

        std::string base_bytes(const Maps::Snapshot& base, const std::string& operation) {
            admit_shape(base, operation);
            try {
                return Maps::encode(base);
            } catch (const Maps::Error& error) {
                throw Error(error.code == Maps::Code::LimitExceeded ? Code::LimitExceeded : Code::InvalidBase,
                            operation, "$.base", "base failed public codec validation", error);
            }
        }

        std::string edit_location(std::size_t index) {
            return "$.edits[" + std::to_string(index) + "]";
        }
    }

    Error::Error(Code code_value, std::string operation_value, std::string location_value,
                 const std::string& detail, std::optional<CE::Maps::Error> cause_value)
        : std::runtime_error(operation_value + " " + location_value + ": " + detail),
          code(code_value), operation(std::move(operation_value)), location(std::move(location_value)),
          cause(std::move(cause_value)) {}

    Witness::Witness(std::string bytes) : bytes_(std::move(bytes)), valid_(true) {}

    Witness::Witness(Witness&& other) noexcept : bytes_(std::move(other.bytes_)), valid_(other.valid_) {
        other.bytes_.clear();
        other.valid_ = false;
    }

    Witness& Witness::operator=(Witness&& other) noexcept {
        if (this != &other) {
            bytes_ = std::move(other.bytes_);
            valid_ = other.valid_;
            other.bytes_.clear();
            other.valid_ = false;
        }
        return *this;
    }

    std::string_view Witness::canonical_bytes() const noexcept { return bytes_; }

    Witness capture(const CE::Maps::Snapshot& base) {
        return Witness(base_bytes(base, "capture"));
    }

    CE::Maps::Snapshot apply(const CE::Maps::Snapshot& base, const Patch& patch) {
        if (patch.edits.size() > max_edits)
            reject(Code::LimitExceeded, "apply", "$.edits", "private edit-count budget exceeded");
        if (!patch.witness.valid_ || patch.witness.bytes_.empty() ||
            patch.witness.bytes_.size() > max_witness_bytes)
            reject(Code::InvalidWitness, "apply", "$.witness", "capture a validated base witness first");
        // Release transient encoding before making the candidate. No hash match.
        if (base_bytes(base, "apply") != patch.witness.bytes_)
            reject(Code::BaseMismatch, "apply", "$.base", "canonical base bytes differ from the witness");

        using Address = std::pair<std::size_t, std::size_t>;
        std::set<Address> addresses;
        std::vector<Address> resolved;
        resolved.reserve(patch.edits.size());
        for (std::size_t i = 0; i < patch.edits.size(); ++i) {
            const auto& edit = patch.edits[i];
            const auto location = edit_location(i);
            if (edit.layer.size() > Maps::max_key_bytes)
                reject(Code::InvalidEdit, "apply", location + ".layer", "layer name exceeds key budget");
            const auto layer = std::find_if(base.layers.begin(), base.layers.end(), [&](const auto& item) {
                return item.id == edit.layer;
            });
            if (layer == base.layers.end())
                reject(Code::InvalidEdit, "apply", location + ".layer", "unknown stable layer identity");
            if (edit.x >= base.width)
                reject(Code::InvalidEdit, "apply", location + ".x", "column outside the finite base");
            if (edit.y >= base.height)
                reject(Code::InvalidEdit, "apply", location + ".y", "row outside the finite base");
            const Address address{static_cast<std::size_t>(layer - base.layers.begin()),
                                  std::size_t{edit.y} * base.width + edit.x};
            if (!addresses.insert(address).second)
                reject(Code::InvalidEdit, "apply", location, "duplicate edit address");
            if (edit.tile && edit.tile->size() > Maps::max_key_bytes)
                reject(Code::InvalidEdit, "apply", location + ".tile", "tile key exceeds key budget");
            resolved.push_back(address);
        }

        // Admission/address checks precede this copy; semantic key checks follow it.
        auto candidate = base;
        for (std::size_t i = 0; i < resolved.size(); ++i)
            candidate.layers[resolved[i].first].cells[resolved[i].second] = patch.edits[i].tile;
        if (!patch.edits.empty()) {
            try {
                Maps::validate(candidate);
            } catch (const Maps::Error& error) {
                if (error.code == Maps::Code::InvalidValue) {
                    for (std::size_t i = 0; i < resolved.size(); ++i) {
                        const auto location = "$.layers[" + std::to_string(resolved[i].first) +
                                              "].cells[" + std::to_string(resolved[i].second) + "]";
                        if (error.location == location)
                            throw Error(Code::InvalidEdit, "apply", edit_location(i) + ".tile",
                                        "invalid logical tile key", error);
                    }
                }
                throw Error(error.code == Maps::Code::LimitExceeded ? Code::LimitExceeded : Code::InvalidEdit,
                            "apply", "$.candidate", "complete candidate failed public codec validation", error);
            }
        }
        return candidate;
    }
}
