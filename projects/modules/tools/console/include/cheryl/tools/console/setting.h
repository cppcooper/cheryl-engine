#pragma once

#include <cheryl/tools/console/registry.h>

namespace CE::Tools::Console {
    struct UnsignedSettingOwner {
        std::function<std::uint64_t()> read;
        std::function<bool(std::uint64_t)> write;
    };

    /** Registers name.read and name.set over the same owner callbacks.
     * No setting value is mirrored. Callbacks run only from retained Work::step
     * on the Session owner; captures must retain their owner. write(false) must
     * preserve the current value. Range validation precedes admission. The
     * result exposes default/effective/minimum/maximum and OwnerImmediate timing.
     * Destroy/reset registrations before destroying any borrowed owner capture.
     */
    class CE_TYPE_API UnsignedSetting {
        Registration read_, write_;
    public:
        CE_CONSOLE_API UnsignedSetting(Registry&, std::string name, std::string help,
            std::uint64_t default_value, std::uint64_t maximum, UnsignedSettingOwner,
            Rights read_rights, Rights write_rights);
        UnsignedSetting(const UnsignedSetting&) = delete;
        UnsignedSetting& operator=(const UnsignedSetting&) = delete;
    };
}
