#pragma once
#include <io/streambuf.h>
#include <istream>

namespace proc {

class pipe_istream : public std::istream {
    io::h_streambuf buf_;

  public:
    pipe_istream() : std::istream(nullptr), buf_(nullptr) {}

    explicit pipe_istream(const win_handle &handle)
        : pipe_istream(handle.get()) {}

    explicit pipe_istream(HANDLE h) : std::istream(nullptr), buf_(h) {
        rdbuf(&buf_);
    }

    pipe_istream(pipe_istream &&other) noexcept
        : std::istream(nullptr), buf_(std::move(other.buf_)) {
        rdbuf(&buf_);
    }

    pipe_istream &operator=(pipe_istream &&other) noexcept {
        if (this != &other) {
            buf_ = std::move(other.buf_);
            this->rdbuf(&buf_);
        }

        return *this;
    }
};

} // namespace proc