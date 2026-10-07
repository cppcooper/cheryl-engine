#include <gtest/gtest.h>
#include <GainputHID.h>
#include <GainputHIDWhitelist.h>
#include <timer/GainputTimer.h>
#include <hidapi.h>
#include <libudev.h>

#include <algorithm>
#include <cstring>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <poll.h>

struct hid_device_ {
    std::string path;
    int generation;
};

struct udev {};
struct udev_monitor {};
struct udev_device {};

namespace {
    enum class Notification { ready, context_failure, monitor_failure, enable_failure, descriptor_failure };

    struct Backend {
        static constexpr int notification_descriptor = 0x7000;
        std::vector<std::string> paths{"/dev/hidraw0"};
        std::set<std::string> unreadable;
        std::set<std::string> read_failures;
        std::vector<std::pair<bool, int>> changes;
        std::vector<std::string> reports;
        std::vector<std::pair<std::string, int>> writes;
        Notification notification = Notification::context_failure;
        unsigned short product = pidSonyDS5;
        int64_t clock_us = 0;
        int init_result = 0;
        int initializations = 0;
        int exits = 0;
        int enumerations = 0;
        int freed_entries = 0;
        int attempts = 0;
        int opens = 0;
        int closes = 0;
        int contexts = 0;
        int monitors = 0;
        int pending_notifications = 0;
        int drained_notifications = 0;
        bool null_path = false;
    };

    Backend* active_backend = nullptr;

    void changed(const char*, bool added, int index) {
        EXPECT_EQ(HIDControllerConnected(static_cast<uint8_t>(index)), added);
        active_backend->changes.emplace_back(added, index);
    }

    class HidLifecycle : public testing::Test {
    protected:
        Backend backend_;

        void SetUp() override {
            active_backend = &backend_;
            ASSERT_EQ(HIDInit(nullptr), 0);
            HIDSetDeviceChangeCallback(changed);
        }

        void TearDown() override {
            EXPECT_EQ(HIDExit(), 0);
            EXPECT_EQ(backend_.opens, backend_.closes);
            EXPECT_EQ(backend_.contexts, 0);
            EXPECT_EQ(backend_.monitors, 0);
            active_backend = nullptr;
        }

        void poll() { HIDPromptForDeviceStateReports(nullptr); }
        void recheck() {
            backend_.clock_us += 4000000;
            poll();
        }

        void use_notifications(Notification mode) {
            ASSERT_EQ(HIDExit(), 0);
            backend_.notification = mode;
            ASSERT_EQ(HIDInit(nullptr), 0);
            HIDSetDeviceChangeCallback(changed);
        }
    };
}

// Only the isolated runner defines these transport functions. Production targets
// and owner aggregates never link the fixtures or the poll wrapper.
extern "C" {
    int hid_init() {
        ++active_backend->initializations;
        return active_backend->init_result;
    }

    int hid_exit() {
        ++active_backend->exits;
        return 0;
    }

    hid_device_info* hid_enumerate(unsigned short, unsigned short) {
        auto& backend = *active_backend;
        ++backend.enumerations;
        hid_device_info* result = nullptr;
        hid_device_info** tail = &result;
        const auto add = [&](const char* path) {
            auto* device = new hid_device_info{};
            if (path) {
                device->path = new char[std::strlen(path) + 1];
                std::strcpy(device->path, path);
            }
            device->vendor_id = vSony;
            device->product_id = backend.product;
            device->usage_page = upDesktop;
            device->usage = uGamepad;
            *tail = device;
            tail = &device->next;
        };
        for (const auto& path : backend.paths) {
            add(path.c_str());
        }
        if (backend.null_path) {
            add(nullptr);
        }
        return result;
    }

    void hid_free_enumeration(hid_device_info* devices) {
        while (devices) {
            auto* next = devices->next;
            delete[] devices->path;
            delete devices;
            ++active_backend->freed_entries;
            devices = next;
        }
    }

    hid_device* hid_open_path(const char* path) {
        auto& backend = *active_backend;
        ++backend.attempts;
        if (backend.unreadable.count(path)) {
            return nullptr;
        }
        return new hid_device_{path, ++backend.opens};
    }

    int hid_read_timeout(hid_device* device, unsigned char*, size_t, int milliseconds) {
        auto& backend = *active_backend;
        if (!device) {
            ADD_FAILURE() << "Read with no open handle";
            return -1;
        }
        if (milliseconds == 0) {
            backend.reports.push_back(device->path);
        }
        return backend.read_failures.count(device->path) ? -1 : 0;
    }

    int hid_get_feature_report(hid_device* device, unsigned char*, size_t) {
        EXPECT_NE(device, nullptr);
        return 0;
    }

    int hid_write(hid_device* device, const unsigned char*, size_t length) {
        if (!device) {
            ADD_FAILURE() << "Write with no open handle";
            return -1;
        }
        active_backend->writes.emplace_back(device->path, device->generation);
        return static_cast<int>(length);
    }

    void hid_close(hid_device* device) {
        EXPECT_NE(device, nullptr);
        delete device;
        ++active_backend->closes;
    }

    udev* udev_new() {
        if (active_backend->notification == Notification::context_failure) {
            return nullptr;
        }
        ++active_backend->contexts;
        return new udev{};
    }

    udev* udev_unref(udev* context) {
        delete context;
        --active_backend->contexts;
        return nullptr;
    }

    udev_monitor* udev_monitor_new_from_netlink(udev*, const char*) {
        if (active_backend->notification == Notification::monitor_failure) {
            return nullptr;
        }
        ++active_backend->monitors;
        return new udev_monitor{};
    }

    int udev_monitor_enable_receiving(udev_monitor*) {
        return active_backend->notification == Notification::enable_failure ? -1 : 0;
    }

    int udev_monitor_get_fd(udev_monitor*) {
        return active_backend->notification == Notification::descriptor_failure ? -1 : Backend::notification_descriptor;
    }

    udev_monitor* udev_monitor_unref(udev_monitor* monitor) {
        delete monitor;
        --active_backend->monitors;
        return nullptr;
    }

    udev_device* udev_monitor_receive_device(udev_monitor*) {
        if (active_backend->pending_notifications == 0) {
            return nullptr;
        }
        --active_backend->pending_notifications;
        return new udev_device{};
    }

    udev_device* udev_device_unref(udev_device* device) {
        delete device;
        ++active_backend->drained_notifications;
        return nullptr;
    }

    int __real_poll(pollfd* descriptors, nfds_t count, int timeout);
    int __wrap_poll(pollfd* descriptors, nfds_t count, int timeout) {
        if (!active_backend || count != 1 || descriptors[0].fd != Backend::notification_descriptor) {
            return __real_poll(descriptors, count, timeout);
        }
        descriptors[0].revents = active_backend->pending_notifications ? POLLIN : 0;
        return active_backend->pending_notifications ? 1 : 0;
    }

    void initHiresTimer(HiresTimer* timer) { timer->mStartTime = active_backend->clock_us; }
    int64_t getHiresTimerUSec(HiresTimer* timer, bool reset) {
        const auto elapsed = active_backend->clock_us - timer->mStartTime;
        if (reset) {
            timer->mStartTime = active_backend->clock_us;
        }
        return elapsed;
    }
}

TEST_F(HidLifecycle, same_path) {
    poll();
    recheck();
    HIDLoadController(1, 0);
    EXPECT_EQ(backend_.attempts, 1);
    EXPECT_EQ(backend_.closes, 0);
    ASSERT_EQ(backend_.changes.size(), 1);
    EXPECT_EQ(backend_.changes.front(), std::make_pair(true, 0));
    EXPECT_TRUE(HIDControllerConnected(0));
}

TEST_F(HidLifecycle, distinct_paths) {
    backend_.paths = {"/dev/hidraw0", "/dev/hidraw1"};
    poll();
    backend_.paths = {"/dev/hidraw1"};
    recheck();
    EXPECT_EQ(backend_.opens, 2);
    EXPECT_EQ(backend_.closes, 1);
    EXPECT_FALSE(HIDControllerConnected(0));
    EXPECT_TRUE(HIDControllerConnected(1));
    ASSERT_EQ(backend_.changes.size(), 3);
    EXPECT_EQ(backend_.changes.back(), std::make_pair(false, 0));
}

TEST_F(HidLifecycle, empty_list) {
    poll();
    backend_.paths.clear();
    recheck();
    recheck();
    EXPECT_FALSE(HIDControllerConnected(0));
    EXPECT_EQ(backend_.closes, 1);
    ASSERT_EQ(backend_.changes.size(), 2);
    EXPECT_EQ(backend_.changes.back(), std::make_pair(false, 0));
    backend_.paths = {"/dev/hidraw2"};
    recheck();
    EXPECT_TRUE(HIDControllerConnected(0));
    EXPECT_EQ(backend_.opens, 2);
}

TEST_F(HidLifecycle, retry_open) {
    backend_.unreadable.insert(backend_.paths.front());
    poll();
    EXPECT_FALSE(HIDControllerConnected(0));
    EXPECT_TRUE(backend_.changes.empty());
    backend_.unreadable.clear();
    backend_.clock_us += 3999999;
    poll();
    EXPECT_EQ(backend_.attempts, 1);
    ++backend_.clock_us;
    poll();
    EXPECT_EQ(backend_.attempts, 2);
    EXPECT_TRUE(HIDControllerConnected(0));
    ASSERT_EQ(backend_.changes.size(), 1);
    EXPECT_EQ(backend_.changes.front(), std::make_pair(true, 0));
}

TEST_F(HidLifecycle, unopened_removal) {
    backend_.unreadable.insert(backend_.paths.front());
    poll();
    backend_.paths.clear();
    recheck();
    EXPECT_EQ(backend_.opens, 0);
    EXPECT_TRUE(backend_.changes.empty());
}

TEST_F(HidLifecycle, ps4_open_failure) {
    backend_.product = pidSonyDS4;
    backend_.unreadable.insert(backend_.paths.front());
    poll();
    EXPECT_EQ(backend_.opens, 0);
    EXPECT_TRUE(backend_.writes.empty());
    EXPECT_TRUE(backend_.changes.empty());
    EXPECT_FALSE(HIDControllerConnected(0));
}

TEST_F(HidLifecycle, initial_read_failure) {
    backend_.read_failures.insert(backend_.paths.front());
    poll();
    EXPECT_EQ(backend_.opens, 1);
    EXPECT_EQ(backend_.closes, 1);
    EXPECT_FALSE(HIDControllerConnected(0));
    EXPECT_TRUE(backend_.changes.empty());
    backend_.read_failures.clear();
    recheck();
    EXPECT_TRUE(HIDControllerConnected(0));
    EXPECT_EQ(backend_.opens, 2);
}

TEST_F(HidLifecycle, read_failure) {
    poll();
    backend_.read_failures.insert(backend_.paths.front());
    poll();
    EXPECT_EQ(backend_.closes, 1);
    EXPECT_EQ(backend_.opens, 1);
    EXPECT_FALSE(HIDControllerConnected(0));
    ASSERT_EQ(backend_.changes.size(), 2);
    EXPECT_EQ(backend_.changes.back(), std::make_pair(false, 0));
    backend_.read_failures.clear();
    poll();
    EXPECT_TRUE(HIDControllerConnected(0));
    EXPECT_EQ(backend_.opens, 2);
}

TEST_F(HidLifecycle, capacity) {
    backend_.paths.clear();
    for (int i = 0; i < 21; ++i) {
        backend_.paths.push_back("/dev/hidraw" + std::to_string(i));
    }
    poll();
    EXPECT_EQ(backend_.opens, 20);
    EXPECT_TRUE(HIDControllerConnected(19));
    HIDDoRumble(20, 0.25f, 0.5f, 0);
    ASSERT_FALSE(backend_.writes.empty());
    EXPECT_EQ(backend_.writes.back().first, "/dev/hidraw19");
    backend_.paths.erase(backend_.paths.begin() + 5);
    recheck();
    EXPECT_EQ(backend_.opens, 21);
    EXPECT_EQ(backend_.closes, 1);
    EXPECT_TRUE(HIDControllerConnected(5));
    EXPECT_TRUE(HIDControllerConnected(19));
    EXPECT_EQ(backend_.reports.back(), "/dev/hidraw19");
    EXPECT_NE(std::find(backend_.reports.begin(), backend_.reports.end(), "/dev/hidraw20"), backend_.reports.end());
}

TEST_F(HidLifecycle, invalid_paths) {
    backend_.paths = {"/dev/hidraw0", "/dev/hidraw0", std::string(247, 'x')};
    backend_.null_path = true;
    poll();
    EXPECT_EQ(backend_.opens, 1);
    EXPECT_EQ(backend_.freed_entries, 4);
}

TEST_F(HidLifecycle, restart) {
    poll();
    ASSERT_EQ(HIDExit(), 0);
    EXPECT_FALSE(HIDControllerConnected(0));
    EXPECT_EQ(HIDGetNextNewControllerID(nullptr, nullptr, nullptr), INVALID_DEV_ID);
    EXPECT_FALSE(HIDHandleSystemMessage(nullptr));
    backend_.paths = {"/dev/hidraw1"};
    const auto changes = backend_.changes.size();
    ASSERT_EQ(HIDInit(nullptr), 0);
    poll();
    EXPECT_EQ(backend_.opens, 2);
    EXPECT_EQ(backend_.closes, 1);
    EXPECT_TRUE(HIDControllerConnected(0));
    EXPECT_EQ(backend_.changes.size(), changes);
}

TEST_F(HidLifecycle, init_failure) {
    ASSERT_EQ(HIDExit(), 0);
    backend_.init_result = -1;
    EXPECT_EQ(HIDInit(nullptr), -1);
    poll();
    EXPECT_EQ(backend_.enumerations, 0);
    EXPECT_FALSE(HIDHandleSystemMessage(nullptr));
    backend_.init_result = 0;
    ASSERT_EQ(HIDInit(nullptr), 0);
    poll();
    EXPECT_TRUE(HIDControllerConnected(0));
}

TEST_F(HidLifecycle, notifier_failures) {
    for (const auto mode : {Notification::context_failure, Notification::monitor_failure,
                          Notification::enable_failure, Notification::descriptor_failure}) {
        use_notifications(mode);
        EXPECT_EQ(backend_.contexts, 0);
        EXPECT_EQ(backend_.monitors, 0);
        const auto enumerations = backend_.enumerations;
        poll();
        recheck();
        EXPECT_EQ(backend_.enumerations, enumerations + 2);
    }
}

TEST_F(HidLifecycle, notifications) {
    use_notifications(Notification::ready);
    poll();
    backend_.paths.clear();
    backend_.pending_notifications = 2;
    poll();
    EXPECT_EQ(backend_.closes, 1);
    EXPECT_EQ(backend_.drained_notifications, 2);
    EXPECT_FALSE(HIDControllerConnected(0));
}

TEST_F(HidLifecycle, periodic_recheck) {
    use_notifications(Notification::ready);
    poll();
    backend_.paths.clear();
    recheck();
    EXPECT_EQ(backend_.closes, 1);
    EXPECT_FALSE(HIDControllerConnected(0));
}
