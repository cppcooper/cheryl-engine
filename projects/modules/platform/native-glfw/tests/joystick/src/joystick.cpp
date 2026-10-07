#include <gtest/gtest.h>
#include <core/controls/input-mapper.h>
#include <gainput/GainputInputDeltaState.h>

#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <deque>
#include <vector>
#include <fcntl.h>
#include <linux/joystick.h>
#include <unistd.h>

namespace {
    struct Joystick;
    Joystick* active_joystick = nullptr;

    struct Joystick {
        static constexpr int descriptor = 0x6000;
        const char* name = "DualSense Wireless Controller";
        std::vector<__u16> buttons{
            BTN_SOUTH, BTN_EAST, BTN_NORTH, BTN_WEST, BTN_TL, BTN_TR, BTN_TL2, BTN_TR2,
            BTN_SELECT, BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR
        };
        std::vector<__u8> axes{ABS_X, ABS_Y, ABS_Z, ABS_RX, ABS_RY, ABS_RZ, ABS_HAT0X, ABS_HAT0Y};
        std::deque<js_event> events;
        bool connected = true;
        bool maps_available = true;
        int opens = 0;
        int closes = 0;

        Joystick() { active_joystick = this; }
        ~Joystick() { active_joystick = nullptr; }

        void push(__u8 type, __u8 number, __s16 value) { events.push_back({0, value, type, number}); }
    };

    struct PadSession {
        gainput::InputManager manager;
        CE::Input::InputMapper bindings{manager};
        gainput::InputDevicePad* pad = nullptr;
        gainput::InputDeltaState delta{manager.GetAllocator()};

        PadSession() {
            // This runner requires HID disabled. Device creation and retained state
            // follow the demo's path without opening a window or physical joystick.
            manager.Init(nullptr);
            static_cast<void>(manager.CreateDevice<gainput::InputDeviceKeyboard>());
            static_cast<void>(manager.CreateDevice<gainput::InputDeviceMouse>());
            pad = manager.CreateAndGetDevice<gainput::InputDevicePad>();
        }

        ~PadSession() { manager.Exit(); }

        void poll() {
            delta.Clear();
            pad->Update(&delta);
            gainput::Array<gainput::InputListener*> listeners(manager.GetAllocator());
            listeners.push_back(&bindings);
            delta.NotifyListeners(0.016f, listeners);
        }
    };
}

// GNU link wrapping replaces calls from the owned static Gainput library only in
// this runner. Real filesystem calls still work outside the synthetic joystick.
extern "C" {
    int __real_open(const char* path, int flags, ...);
    ssize_t __real_read(int descriptor, void* buffer, size_t size);
    int __real_ioctl(int descriptor, unsigned long request, ...);
    int __real_close(int descriptor);

    int __wrap_open(const char* path, int flags, ...) {
        if (active_joystick && std::strcmp(path, "/dev/input/js0") == 0) {
            if (!active_joystick->connected) {
                errno = ENODEV;
                return -1;
            }
            ++active_joystick->opens;
            return Joystick::descriptor;
        }
        if ((flags & O_CREAT) || (flags & O_TMPFILE) == O_TMPFILE) {
            va_list arguments;
            va_start(arguments, flags);
            const auto mode = va_arg(arguments, mode_t);
            va_end(arguments);
            return __real_open(path, flags, mode);
        }
        return __real_open(path, flags);
    }

    ssize_t __wrap_read(int descriptor, void* buffer, size_t size) {
        if (!active_joystick || descriptor != Joystick::descriptor) {
            return __real_read(descriptor, buffer, size);
        }
        if (!active_joystick->connected) {
            errno = ENODEV;
            return -1;
        }
        if (active_joystick->events.empty()) {
            errno = EAGAIN;
            return -1;
        }
        if (size != sizeof(js_event)) {
            errno = EINVAL;
            return -1;
        }
        std::memcpy(buffer, &active_joystick->events.front(), sizeof(js_event));
        active_joystick->events.pop_front();
        return sizeof(js_event);
    }

    int __wrap_ioctl(int descriptor, unsigned long request, ...) {
        va_list arguments;
        va_start(arguments, request);
        void* output = va_arg(arguments, void*);
        va_end(arguments);
        if (!active_joystick || descriptor != Joystick::descriptor) {
            return __real_ioctl(descriptor, request, output);
        }
        auto& joystick = *active_joystick;
        if (_IOC_TYPE(request) == 'j' && _IOC_NR(request) == 0x13) {
            const auto size = _IOC_SIZE(request);
            std::memset(output, 0, size);
            if (size > 0) {
                std::strncpy(static_cast<char*>(output), joystick.name, size - 1);
            }
        } else if (request == JSIOCGBUTTONS) {
            *static_cast<__u8*>(output) = static_cast<__u8>(joystick.buttons.size());
        } else if (request == JSIOCGAXES) {
            *static_cast<__u8*>(output) = static_cast<__u8>(joystick.axes.size());
        } else if (request == JSIOCGVERSION) {
            *static_cast<__u32*>(output) = JS_VERSION;
        } else if (joystick.maps_available && request == JSIOCGBTNMAP) {
            std::memset(output, 0, _IOC_SIZE(request));
            std::memcpy(output, joystick.buttons.data(), joystick.buttons.size() * sizeof(__u16));
        } else if (joystick.maps_available && request == JSIOCGAXMAP) {
            std::memset(output, 0, _IOC_SIZE(request));
            std::memcpy(output, joystick.axes.data(), joystick.axes.size() * sizeof(__u8));
        } else {
            errno = EINVAL;
            return -1;
        }
        return 0;
    }

    int __wrap_close(int descriptor) {
        if (!active_joystick || descriptor != Joystick::descriptor) {
            return __real_close(descriptor);
        }
        ++active_joystick->closes;
        return 0;
    }
}

TEST(linux_joystick, cross) {
    Joystick joystick;
    PadSession session;
    const CE::Input::ActionId action{1};
    static_cast<void>(session.bindings.bind_button({session.pad->GetDeviceId(), gainput::PadButtonA}, action));
    ASSERT_EQ(session.pad->GetDeviceId(), 2u);
    ASSERT_TRUE(session.pad->IsAvailable());
    ASSERT_TRUE(session.pad->IsValidButtonId(gainput::PadButtonA));

    joystick.push(JS_EVENT_BUTTON | JS_EVENT_INIT, 0, 0);
    joystick.push(JS_EVENT_BUTTON, 0, 1);
    session.poll();
    EXPECT_TRUE(session.pad->GetBool(gainput::PadButtonA));
    auto actions = session.bindings.publish_actions();
    EXPECT_EQ(actions->button(action).press_count, 1u);
    EXPECT_TRUE(actions->button(action).held());

    session.poll();
    actions = session.bindings.publish_actions();
    EXPECT_EQ(actions->button(action).press_count, 0u);
    EXPECT_TRUE(actions->button(action).held());

    joystick.push(JS_EVENT_BUTTON, 0, 0);
    joystick.push(JS_EVENT_BUTTON, 0, 1);
    joystick.push(JS_EVENT_BUTTON, 0, 0);
    session.poll();
    EXPECT_FALSE(session.pad->GetBool(gainput::PadButtonA));
    actions = session.bindings.publish_actions();
    EXPECT_EQ(actions->button(action).press_count, 1u);
    EXPECT_EQ(actions->button(action).release_count, 2u);
    EXPECT_FALSE(actions->button(action).held());
}

TEST(linux_joystick, controls) {
    Joystick joystick;
    PadSession session;
    const gainput::DeviceButtonId buttons[]{
        gainput::PadButtonA, gainput::PadButtonB, gainput::PadButtonY, gainput::PadButtonX,
        gainput::PadButtonL1, gainput::PadButtonR1, gainput::PadButtonL2, gainput::PadButtonR2,
        gainput::PadButtonSelect, gainput::PadButtonStart, gainput::PadButtonHome, gainput::PadButtonL3, gainput::PadButtonR3
    };
    for (__u8 i = 0; i < joystick.buttons.size(); ++i) {
        joystick.push(JS_EVENT_BUTTON, i, 1);
    }
    joystick.push(JS_EVENT_AXIS, 2, -32767); // ABS_Z is L2, not right stick X.
    joystick.push(JS_EVENT_AXIS, 3, 32767);
    joystick.push(JS_EVENT_AXIS, 4, -32767);
    joystick.push(JS_EVENT_AXIS, 5, 32767);
    session.poll();
    for (const auto button : buttons) {
        EXPECT_TRUE(session.pad->IsValidButtonId(button));
        EXPECT_TRUE(session.pad->GetBool(button));
    }
    EXPECT_FLOAT_EQ(session.pad->GetFloat(gainput::PadButtonAxis4), -1.0f);
    EXPECT_FLOAT_EQ(session.pad->GetFloat(gainput::PadButtonRightStickX), 1.0f);
    EXPECT_FLOAT_EQ(session.pad->GetFloat(gainput::PadButtonRightStickY), -1.0f);
    EXPECT_FLOAT_EQ(session.pad->GetFloat(gainput::PadButtonAxis5), 1.0f);
}

TEST(linux_joystick, dpad) {
    Joystick joystick;
    PadSession session;
    for (const auto button : {gainput::PadButtonLeft, gainput::PadButtonRight, gainput::PadButtonUp, gainput::PadButtonDown}) {
        ASSERT_TRUE(session.pad->IsValidButtonId(button));
    }
    joystick.push(JS_EVENT_AXIS, 6, -32767);
    joystick.push(JS_EVENT_AXIS, 7, -32767);
    session.poll();
    EXPECT_TRUE(session.pad->GetBool(gainput::PadButtonLeft));
    EXPECT_TRUE(session.pad->GetBool(gainput::PadButtonUp));
    EXPECT_FALSE(session.pad->GetBool(gainput::PadButtonRight));
    EXPECT_FALSE(session.pad->GetBool(gainput::PadButtonDown));
    joystick.push(JS_EVENT_AXIS, 6, 32767);
    joystick.push(JS_EVENT_AXIS, 7, 32767);
    session.poll();
    EXPECT_FALSE(session.pad->GetBool(gainput::PadButtonLeft));
    EXPECT_FALSE(session.pad->GetBool(gainput::PadButtonUp));
    EXPECT_TRUE(session.pad->GetBool(gainput::PadButtonRight));
    EXPECT_TRUE(session.pad->GetBool(gainput::PadButtonDown));
    joystick.push(JS_EVENT_AXIS, 6, 0);
    joystick.push(JS_EVENT_AXIS, 7, 0);
    session.poll();
    EXPECT_FALSE(session.pad->GetBool(gainput::PadButtonRight));
    EXPECT_FALSE(session.pad->GetBool(gainput::PadButtonDown));
}

TEST(linux_joystick, remap) {
    Joystick joystick;
    joystick.name = "Renamed controller";
    joystick.buttons.assign(33, BTN_TRIGGER);
    joystick.buttons[32] = BTN_SOUTH;
    joystick.axes = {ABS_RX, ABS_RY, ABS_X, ABS_Y};
    PadSession session;
    joystick.push(JS_EVENT_BUTTON, 0, 1); // Unsupported code must not become A.
    joystick.push(JS_EVENT_BUTTON, 32, 1); // Kernel slots can exceed Gainput's button count.
    joystick.push(JS_EVENT_AXIS, 0, 32767);
    joystick.push(JS_EVENT_AXIS, 2, -32767);
    session.poll();
    EXPECT_TRUE(session.pad->GetBool(gainput::PadButtonA));
    EXPECT_FALSE(session.pad->GetBool(gainput::PadButtonB));
    EXPECT_FLOAT_EQ(session.pad->GetFloat(gainput::PadButtonRightStickX), 1.0f);
    EXPECT_FLOAT_EQ(session.pad->GetFloat(gainput::PadButtonLeftStickX), -1.0f);
}

TEST(linux_joystick, reconnect) {
    Joystick joystick;
    {
        PadSession session;
        const CE::Input::ActionId action{1};
        static_cast<void>(session.bindings.bind_button({session.pad->GetDeviceId(), gainput::PadButtonA}, action));
        joystick.push(JS_EVENT_BUTTON, 0, 1);
        joystick.push(JS_EVENT_AXIS, 0, 32767);
        session.poll();
        static_cast<void>(session.bindings.publish_actions());
        joystick.connected = false;
        session.poll();
        EXPECT_FALSE(session.pad->IsAvailable());
        EXPECT_FALSE(session.pad->GetInputState()->GetBool(gainput::PadButtonA));
        EXPECT_FLOAT_EQ(session.pad->GetInputState()->GetFloat(gainput::PadButtonLeftStickX), 0.0f);
        EXPECT_EQ(joystick.closes, 1);
        auto actions = session.bindings.publish_actions();
        EXPECT_EQ(actions->button(action).release_count, 1u);
        EXPECT_FALSE(actions->button(action).held());

        joystick.connected = true;
        joystick.buttons = {BTN_EAST};
        joystick.axes = {ABS_RY};
        joystick.push(JS_EVENT_BUTTON | JS_EVENT_INIT, 0, 1);
        session.poll();
        EXPECT_EQ(joystick.opens, 2);
        EXPECT_TRUE(session.pad->IsAvailable());
        EXPECT_FALSE(session.pad->IsValidButtonId(gainput::PadButtonA));
        EXPECT_FALSE(session.pad->IsValidButtonId(gainput::PadButtonLeftStickX));
        EXPECT_TRUE(session.pad->GetBool(gainput::PadButtonB));
        EXPECT_FALSE(session.pad->GetBool(gainput::PadButtonA));
        actions = session.bindings.publish_actions();
        EXPECT_EQ(actions->button(action).press_count, 0u);
        EXPECT_FALSE(actions->button(action).held());
    }
    EXPECT_EQ(joystick.closes, 2);
}

TEST(linux_joystick, legacy) {
    for (const bool ps3 : {false, true}) {
        Joystick joystick;
        joystick.name = ps3 ? "Sony PLAYSTATION(R)3 Controller" : "Microsoft X-Box 360 pad";
        joystick.maps_available = false;
        PadSession session;
        joystick.push(JS_EVENT_BUTTON, ps3 ? 14 : 0, 1);
        session.poll();
        EXPECT_TRUE(session.pad->GetBool(gainput::PadButtonA));
    }
}

TEST(linux_joystick, unavailable_maps) {
    Joystick joystick;
    joystick.maps_available = false;
    PadSession session;
    joystick.push(JS_EVENT_BUTTON, 0, 1);
    joystick.push(JS_EVENT_AXIS, 0, 32767);
    session.poll();
    EXPECT_FALSE(session.pad->GetBool(gainput::PadButtonA));
    EXPECT_FLOAT_EQ(session.pad->GetFloat(gainput::PadButtonLeftStickX), 1.0f);
}
