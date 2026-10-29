// Applications can replace the backend defaults, including for dependency audits.
extern "C" __attribute__((visibility("default"))) const char* __lsan_default_suppressions() {
    return "";
}
