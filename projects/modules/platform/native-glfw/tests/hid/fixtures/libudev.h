#pragma once

// Opaque test-only notification handles; no system libudev implementation is linked.
struct udev;
struct udev_monitor;
struct udev_device;

extern "C" {
    udev* udev_new();
    udev* udev_unref(udev* context);
    udev_monitor* udev_monitor_new_from_netlink(udev* context, const char* name);
    int udev_monitor_enable_receiving(udev_monitor* monitor);
    int udev_monitor_get_fd(udev_monitor* monitor);
    udev_monitor* udev_monitor_unref(udev_monitor* monitor);
    udev_device* udev_monitor_receive_device(udev_monitor* monitor);
    udev_device* udev_device_unref(udev_device* device);
}
