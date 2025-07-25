#pragma once
#include "istream.h"
#include "ostream.h"
#include "stream.h"
#include <atomic>
#include <fcntl.h>
#include <fstream>
#include <io.h>
#include <io/stream.h>
#include <optional>
#include <stdexcept>
#include <thread>
#include <winapi/debugger.h>
#include <winapi/handle.h>

namespace proc {

class pipe {
    win_handle read_;
    win_handle write_;
    pipe_ostream write_stream_;
    pipe_istream read_stream_;

    std::optional<std::thread> async_read_thread_;
    std::atomic<bool> async_read_running_ = false;
    proc_handler async_read_handler_;

  public:
    static pipe
    create(bool inheritable_read = true, bool inheritable_write = false) {
        SECURITY_ATTRIBUTES sa{};
        sa.nLength              = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle       = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        HANDLE read_handle  = nullptr;
        HANDLE write_handle = nullptr;

        if (!CreatePipe(&read_handle, &write_handle, &sa, 0)) {
            throw std::runtime_error("CreatePipe failed");
        }

        if (!inheritable_read)
            SetHandleInformation(read_handle, HANDLE_FLAG_INHERIT, 0);

        if (!inheritable_write)
            SetHandleInformation(write_handle, HANDLE_FLAG_INHERIT, 0);

        return pipe(read_handle, write_handle);
    }

    pipe() = default;

    pipe(HANDLE read, HANDLE write)
        : read_(read), write_(write), read_stream_(read_),
          write_stream_(write_) {}

    pipe(const pipe &)            = delete;
    pipe &operator=(const pipe &) = delete;

    pipe(pipe &&other) noexcept
        : read_(std::move(other.read_)), write_(std::move(other.write_)),
          read_stream_(std::move(other.read_stream_)),
          write_stream_(std::move(other.write_stream_)),
          async_read_thread_(std::move(other.async_read_thread_)),
          async_read_running_(other.async_read_running_.load()) {}

    pipe &operator=(pipe &&other) noexcept {
        if (this != &other) {
            read_               = std::move(other.read_);
            write_              = std::move(other.write_);
            read_stream_        = std::move(other.read_stream_);
            write_stream_       = std::move(other.write_stream_);
            async_read_thread_  = std::move(other.async_read_thread_);
            async_read_running_ = other.async_read_running_.load();
        }
        return *this;
    }

    void write(const std::string &data) {
        DWORD bytes_written = 0;
        if (!::WriteFile(
                write_.get(), data.data(), static_cast<DWORD>(data.size()),
                &bytes_written, nullptr
            )) {
            throw std::runtime_error(
                "WriteFile failed: " + winapi::get_last_error()
            );
        }
    }

    void write_line(const std::string &line) { write(line + "\n"); }

    void flush() {
        if (!FlushFileBuffers(write_.get())) {
            throw std::runtime_error(
                "FlushFileBuffers failed: " + winapi::get_last_error()
            );
        }
    }

    std::string read(size_t max_bytes = 4096) {
        std::string buffer(max_bytes, '\0');
        DWORD bytes_read = 0;
        if (!::ReadFile(
                read_.get(), buffer.data(), static_cast<DWORD>(max_bytes),
                &bytes_read, nullptr
            )) {
            if (GetLastError() == ERROR_BROKEN_PIPE) {
                return "";
            }
            throw std::runtime_error(
                "ReadFile from pipe failed: " + winapi::get_last_error()
            );
        }
        buffer.resize(bytes_read);
        return buffer;
    }

    std::string read_line() {
        std::string line;
        char ch;
        DWORD bytes_read;
        while (true) {
            if (!::ReadFile(read_.get(), &ch, 1, &bytes_read, nullptr) ||
                bytes_read == 0) {
                break;
            }
            if (ch == '\n')
                break;
            line += ch;
        }
        return line;
    }

    void begin_read(proc_handler handler) {
        if (async_read_running_)
            throw std::runtime_error("Async read already running.");

        async_read_handler_ = std::move(handler);
        async_read_running_ = true;
        async_read_thread_.emplace([this]() {});
    }

    void end_read() { stop_async_read(); }

    pipe_ostream &write_stream() { return write_stream_; }
    pipe_istream &read_stream() { return read_stream_; }

    win_handle &read_end() { return read_; }
    win_handle &write_end() { return write_; }

    const win_handle &read_end() const { return read_; }
    const win_handle &write_end() const { return write_; }

    HANDLE read_handle() const { return read_.get(); }
    HANDLE write_handle() const { return write_.get(); }

    void close_read() { read_.reset(); }
    void close_write() { write_.release(); }

    ~pipe() { stop_async_read(); }

    template <typename T> pipe &operator<<(const T &val) {
        write_stream_ << val;
        write_stream_.flush();
        return *this;
    }

    template <typename T> pipe &operator>>(T &val) {
        read_stream_ >> val;
        return *this;
    }

  private:
    void async_read_loop() {
        constexpr size_t buffer_size = 4096;
        char buffer[buffer_size];
        DWORD bytes_read = 0;

        while (async_read_running_) {
            if (!ReadFile(
                    read_.get(), buffer, buffer_size - 1, &bytes_read, nullptr
                ) ||
                bytes_read == 0) {
                break;
            }
            buffer[bytes_read] = '\0';
            async_read_handler_(std::string(buffer, bytes_read));
        }
    }

    void stop_async_read() {
        async_read_running_ = false;
        if (async_read_thread_ && async_read_thread_->joinable()) {
            async_read_thread_->join();
        }
        async_read_thread_.reset();
    }
};

} // namespace proc