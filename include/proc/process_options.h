#pragma once
#include <Windows.h>
#include <functional>
#include <optional>
#include <string>
#include <variant>

namespace proc {

struct process_options {
    fs::path application;
    std::wstring command_line;
    optional_path working_directory;

    DWORD creation_flags                   = 0;
    optional_bool inherit_handles_override = false;

    proc_handler stdout_handler;
    proc_handler stderr_handler;

    std::string stdin_input;

    process_options &with_application(const fs::path app) {
        application = app;
        return *this;
    }

    process_options &with_creation_flags(DWORD flags) {
        creation_flags = flags;
        return *this;
    }

    process_options &explicitly_inherit_handles(bool inherit = true) {
        inherit_handles_override = inherit;
        return *this;
    }

    process_options &with_command_line(const std::string &cmd) {
        return with_command_line(winapi::string_to_wstring(cmd));
    }

    process_options &with_command_line(const std::wstring &cmd) {
        command_line = cmd;
        return *this;
    }

    process_options &with_working_directory(const fs::path dir) {
        working_directory = dir;
        return *this;
    }

    process_options &redirect_stdout_to(proc_handler handler) {
        redirect_stdout_ = true;
        stdout_handler   = std::move(handler);
        return *this;
    }

    process_options &redirect_stderr_to(proc_handler handler) {
        redirect_stderr_ = true;
        stderr_handler   = std::move(handler);
        return *this;
    }

    process_options &redirect_stderr_to_stdout() {
        redirect_stderr_  = true;
        stderr_to_stdout_ = true;
        return *this;
    }

    process_options &redirect_stdin() {
        redirect_stdin_ = true;
        return *this;
    }

  private:
    friend class process;

    bool redirect_stdin_   = false;
    bool redirect_stdout_  = false;
    bool redirect_stderr_  = false;
    bool stderr_to_stdout_ = false;
};

} // namespace proc