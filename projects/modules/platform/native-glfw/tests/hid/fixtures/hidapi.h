#pragma once

#include <cstddef>

// Test-only transport declarations. Every caller and implementation in this
// runner uses this fixture; it makes no claim about the installed hidapi ABI.
struct hid_device_;
using hid_device = hid_device_;

struct hid_device_info {
    char* path;
    unsigned short vendor_id;
    unsigned short product_id;
    unsigned short usage_page;
    unsigned short usage;
    hid_device_info* next;
};

extern "C" {
    int hid_init();
    int hid_exit();
    hid_device_info* hid_enumerate(unsigned short vendor_id, unsigned short product_id);
    void hid_free_enumeration(hid_device_info* devices);
    hid_device* hid_open_path(const char* path);
    int hid_read_timeout(hid_device* device, unsigned char* data, size_t length, int milliseconds);
    int hid_get_feature_report(hid_device* device, unsigned char* data, size_t length);
    int hid_write(hid_device* device, const unsigned char* data, size_t length);
    void hid_close(hid_device* device);
}
