#pragma once

#if defined(SDK_STATE_STATIC)
#define SDK_STATE_API
#elif defined(_WIN32)
#if defined(SDK_STATE_BUILDING)
#define SDK_STATE_API __declspec(dllexport)
#else
#define SDK_STATE_API __declspec(dllimport)
#endif
#else
#define SDK_STATE_API __attribute__((visibility("default")))
#endif

namespace CE::Rendering::Resources {
    struct Provider;
}

SDK_STATE_API const void* sdk_event_system();
SDK_STATE_API const void* sdk_logging_mutex();
SDK_STATE_API bool sdk_provider_bound(const CE::Rendering::Resources::Provider& provider);
SDK_STATE_API void sdk_throw_exception();
