#pragma once

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace CE::TerminalDetail {
    class Descriptor final {
        int value_ = -1;

    public:
        explicit Descriptor(const int value = -1) noexcept : value_(value) {}
        ~Descriptor() { reset(); }
        Descriptor(const Descriptor&) = delete;
        Descriptor& operator=(const Descriptor&) = delete;
        Descriptor(Descriptor&& other) noexcept : value_(std::exchange(other.value_, -1)) {}
        Descriptor& operator=(Descriptor&& other) noexcept {
            if (this != &other) {
                reset();
                value_ = std::exchange(other.value_, -1);
            }
            return *this;
        }
        [[nodiscard]] int get() const noexcept { return value_; }
        void reset(const int value = -1) noexcept {
            if (value_ >= 0)
                static_cast<void>(::close(value_));
            value_ = value;
        }
    };

    [[noreturn]] inline void native_failure(const char* operation) {
        const int error = errno;
        throw std::runtime_error(std::string(operation) + ": " + std::strerror(error));
    }

    [[nodiscard]] inline sockaddr_un socket_address(const std::string& path) {
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        if (path.empty() || path.size() >= sizeof(address.sun_path))
            throw std::invalid_argument("Native terminal socket path exceeds its platform limit");
        std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
        return address;
    }

    [[nodiscard]] inline bool send_control(const int descriptor, const char value) noexcept {
        ssize_t result;
        do {
            result = ::send(descriptor, &value, 1, MSG_NOSIGNAL | MSG_DONTWAIT);
        } while (result < 0 && errno == EINTR);
        return result == 1;
    }
}
