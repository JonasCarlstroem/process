#pragma once
#include <io/streambuf.h>
#include <ostream>

namespace proc {

class pipe_ostream : public std::ostream {
    io::h_streambuf buf_;

  public:
    pipe_ostream() : std::ostream(nullptr), buf_(nullptr) {}

    explicit pipe_ostream(const win_handle &handle)
        : pipe_ostream(handle.get()) {}

    explicit pipe_ostream(HANDLE h) : std::ostream(nullptr), buf_(h) {
        rdbuf(&buf_);
    }

    pipe_ostream(pipe_ostream &&other) noexcept
        : std::ostream(nullptr), buf_(std::move(other.buf_)) {
        rdbuf(&buf_);
    }

    pipe_ostream &operator=(pipe_ostream &&other) noexcept {
        if (this != &other) {
            buf_ = std::move(other.buf_);
            this->rdbuf(&buf_);
        }

        return *this;
    }

    pipe_ostream &operator<<(const std::string &value) {
        static_cast<std::ostream &>(*this) << value;
        this->flush();
        return *this;
    }

    template <typename T> pipe_ostream &operator<<(const T &val) {
        static_cast<std::ostream &>(*this) << val;
        this->flush();
        return *this;
    }
};

} // namespace proc