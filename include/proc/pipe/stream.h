#pragma once
#include <io/streambuf.h>
#include <iostream>

namespace proc {

class pipe_stream : public std::iostream {
    io::h_streambuf read_buf_;
    io::h_streambuf write_buf_;

  public:
    explicit pipe_stream(
        const win_handle &read_handle, const win_handle &write_handle
    )
        : std::iostream(nullptr), read_buf_(read_handle.get()),
          write_buf_(write_handle.get()) {
        rdbuf(&read_buf_);
    }

    std::streambuf *outbuf() { return &write_buf_; }
};

} // namespace proc