#pragma once
#include <Windows.h>
// #include "proc_handle.hpp"

namespace proc {

struct startup_info {
    STARTUPINFOW si{};
    win_handle stdin_handle;
    win_handle stdout_handle;
    win_handle stderr_handle;

    startup_info() {
        ZeroMemory(&si, sizeof(STARTUPINFOW));
        si.cb = sizeof(STARTUPINFOW);
        si.dwFlags |= STARTF_USESTDHANDLES;
    }

    void set_redirected_handles(HANDLE in, HANDLE out, HANDLE err) {
        si.hStdInput  = in;
        si.hStdOutput = out;
        si.hStdError  = err;
    }

    LPSTARTUPINFOW data() { return &si; }
};

} // namespace proc