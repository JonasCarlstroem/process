#pragma once
#include <condition_variable>
#include <future>
#include <io/async_reader.h>
#include <mutex>
#include <queue>
#include <sstream>
#include <thread>
#include <winapi/console.h>
#include <winapi/handle.h>
#include <winapi/utils.h>
// #include "proc_handle.hpp"
#include "pipe/pipe.h"
#include "process_options.h"
#include "startup_info.h"

// using namespace winapi;

namespace proc {

class process {
    process_options opts_;
    bool started_ = false;

    win_handle process_handle_;
    win_handle thread_handle_;
    DWORD process_id_ = 0;

    pipe stdin_pipe_;
    pipe stdout_pipe_;
    pipe stderr_pipe_;

    std::thread stdout_thread_;
    std::thread stderr_thread_;

    std::mutex stdin_mutex_;
    std::condition_variable stdin_cv_;
    std::queue<std::string> stdin_queue_;
    bool stdin_closed_             = false;

    BOOL inherit_handles_override_ = FALSE;

    const wchar_t* working_dir() const noexcept {
        return opts_.working_directory.has_value()
                   ? opts_.working_directory.value().wstring().c_str()
                   : nullptr;
    }

    BOOL inherit_handles() const noexcept {
        if (inherit_handles_override_)
            return TRUE;

        return (opts_.redirect_stdin_ || opts_.redirect_stdout_ ||
                opts_.redirect_stderr_)
                   ? TRUE
                   : FALSE;
    }

  public:
    process() = default;

    process(const process_options& opts) : opts_(std::move(opts)) {}

    process(process&& other) noexcept
        : process_handle_(std::move(other.process_handle_)),
          thread_handle_(std::move(other.thread_handle_)),
          stdin_pipe_(std::move(other.stdin_pipe_)),
          stdout_pipe_(std::move(other.stdout_pipe_)),
          stderr_pipe_(std::move(other.stderr_pipe_)),
          stdout_thread_(std::move(other.stdout_thread_)),
          stderr_thread_(std::move(other.stderr_thread_)),
          started_(other.started_), opts_(std::move(other.opts_)),
          stdin_closed_(other.stdin_closed_) {}

    process& operator=(process&& other) noexcept {
        if (this != &other) {
            wait();
            process_handle_ = std::move(other.process_handle_);
            thread_handle_  = std::move(other.thread_handle_);
            stdin_pipe_     = pipe(std::move(other.stdin_pipe_));
            stdout_pipe_    = pipe(std::move(other.stdout_pipe_));
            stderr_pipe_    = pipe(std::move(other.stderr_pipe_));
            stdout_thread_  = std::move(other.stdout_thread_);
            stderr_thread_  = std::move(other.stderr_thread_);
            started_        = other.started_;
            opts_           = std::move(other.opts_);
            stdin_closed_   = other.stdin_closed_;
        }
        return *this;
    }

    process(const process&)            = delete;
    process& operator=(const process&) = delete;

    process& with_working_directory(const fs::path dir) {
        opts_.with_working_directory(dir);
        return *this;
    }

    process& redirect_stdout_to(proc_handler handler) {
        opts_.redirect_stdout_to(std::move(handler));
        return *this;
    }

    process& redirect_stderr_to(proc_handler handler) {
        opts_.redirect_stderr_to(std::move(handler));
        return *this;
    }

    process& redirect_stderr_to_stdout() {
        opts_.redirect_stderr_to_stdout();
        return *this;
    }

    process& redirect_stdin() {
        opts_.redirect_stdin_ = true;
        return *this;
    }

    void start() {
        if (started_) {
            throw std::runtime_error("Process already started.");
        }

        start_impl(opts_.application, opts_.command_line);
        started_ = true;
    }

    void
    start(const fs::path application, const std::string& command_line = "") {
        if (started_) {
            throw std::runtime_error("Process already started.");
        }

        start(application, winapi::string_to_wstring(command_line));
    }

    void
    start(const fs::path application, const std::wstring& command_line = L"") {
        if (started_) {
            throw std::runtime_error("Process already started.");
        }

        start_impl(application, command_line);
        started_ = true;
    }

    bool kill(UINT exit_code = 1) {
        if (!process_handle_.valid())
            return false;

        BOOL result = TerminateProcess(process_handle_.get(), exit_code);
        return result != 0;
    }

    static process
    launch(const std::string& cmd_line, process_options opts = {}) {
        process proc;
        proc.opts_ = std::move(opts);
        proc.start("", cmd_line);
        return proc;
    }

    static process launch(
        const std::string& application, const std::string& command_line = "",
        process_options opts = {}
    ) {
        process proc;
        proc.opts_ = std::move(opts);
        proc.start(application, command_line);
        return proc;
    }

    static process launch(
        const fs::path& application, const std::string& cmd_line = "",
        process_options opts = {}
    ) {
        process proc;
        proc.opts_ = std::move(opts);
        proc.start(application.string(), cmd_line);
        return proc;
    }

    void wait() {
        if (process_handle_.valid()) {
            ::WaitForSingleObject(process_handle_.get(), INFINITE);
        }

        // if (async_stdin_writer_) async_stdin_writer_->stop();
        // if (async_stdout_) async_stdout_->stop();
        // if (async_stderr_) async_stderr_->stop();
    }

    DWORD exit_code() const {
        DWORD code = 0;
        if (process_handle_.valid()) {
            ::GetExitCodeProcess(process_handle_.get(), &code);
        }
        return code;
    }

    bool is_running() const { return exit_code() == STILL_ACTIVE; }

    HANDLE native_handle() const { return process_handle_.get(); }

    void write_stdin(const std::string& data) {
        if (!started_ || !opts_.redirect_stdin_) {
            throw std::runtime_error(
                "Process not started or stdin not redirected"
            );
        }

        // async_stdin_writer_->write(data);
    }

    void close_stdin() {
        if (!started_ || !opts_.redirect_stdin_)
            return;

        stdin_pipe_.close_read();
        stdin_pipe_.close_write();
    }

    void close_handles() {
        CloseHandle(process_handle_.get());
        CloseHandle(thread_handle_.get());
    }

    pipe& standard_out() { return stdout_pipe_; }
    pipe& standard_error() { return stderr_pipe_; }
    pipe& standard_in() { return stdin_pipe_; }

    void begin_read_stdout(proc_handler handler = nullptr) {
        if (!handler) {
            if (!opts_.stdout_handler) {
                throw std::runtime_error(
                    "No (std) output handler was supplied."
                );
            }
            handler = opts_.stdout_handler;
        }

        stdout_pipe_.begin_read(std::move(handler));
    }

    void begin_read_stderr(proc_handler handler = nullptr) {
        if (!handler) {
            if (!opts_.stderr_handler) {
                throw std::runtime_error(
                    "No (std) error handler was supplied."
                );
            }
            handler = opts_.stderr_handler;
        }

        stderr_pipe_.begin_read(std::move(handler));
    }

    void end_read_stdout() { stdout_pipe_.end_read(); }

    void end_read_stderr() { stderr_pipe_.end_read(); }

  private:
    void start_impl(const fs::path application, const std::wstring& cmdline) {
        startup_info si;

        if (opts_.redirect_stdin_) {
            stdin_pipe_     = pipe::create(true, false);
            si.stdin_handle = win_handle(stdin_pipe_.read_handle());
        }

        if (opts_.redirect_stdout_) {
            stdout_pipe_    = pipe::create(false, true);
            si.stdin_handle = win_handle(stdout_pipe_.write_handle());
        }

        if (opts_.redirect_stderr_) {
            if (opts_.stderr_to_stdout_ && opts_.redirect_stdout_) {
                si.stderr_handle = win_handle(stdout_pipe_.write_handle());
            } else {
                stderr_pipe_     = pipe::create(false, true);
                si.stderr_handle = win_handle(stderr_pipe_.write_handle());
            }
        }

        si.set_redirected_handles(
            si.stdin_handle.get(), si.stdout_handle.get(),
            si.stderr_handle.get()
        );

        STARTUPINFOW siw = *si.data();
        bool inherit_    = inherit_handles();

        PROCESS_INFORMATION pi{};
        BOOL success = CreateProcessW(
            application.c_str(),
            !cmdline.empty() ? const_cast<wchar_t*>(cmdline.c_str()) : nullptr,
            nullptr, nullptr, inherit_handles(), opts_.creation_flags, nullptr,
            working_dir(), si.data(), &pi
        );

        if (!success) {
            std::string error_msg = winapi::get_last_error();
            throw std::runtime_error(error_msg);
        }

#ifdef _DEBUG
        std::cout << "stdin read handle: " << stdin_pipe_.read_handle()
                  << std::endl;
        std::cout << "stdin write handle: " << stdin_pipe_.write_handle()
                  << std::endl;
#endif

        process_handle_ = win_handle(pi.hProcess);
        thread_handle_  = win_handle(pi.hThread);
        process_id_     = pi.dwProcessId;
    }

    void join_threads() {
        if (stdout_thread_.joinable())
            stdout_thread_.join();
        if (stderr_thread_.joinable())
            stderr_thread_.join();
    }
};

} // namespace proc