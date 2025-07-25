#pragma once
#include <stdexcept>
#include <io/reader.h>
#include <io/writer.h>
#include "handle.h"

#ifdef _PROC_IO
namespace proc {
	class pipe_reader : public io::reader {
		handle read_;

	public:
		explicit pipe_reader(HANDLE h) : read_(h) {}

		std::string read(size_t max_bytes = 4096) override {
			std::string buffer(max_bytes, '\0');
			DWORD bytes_read = 0;
			if (!::ReadFile(native_handle(), buffer.data(), static_cast<DWORD>(max_bytes), &bytes_read, nullptr)) {
				if (GetLastError() == ERROR_BROKEN_PIPE) {
					return "";
				}
				throw std::runtime_error("ReadFile from pipe failed: " + winapi::get_last_error());
			}
			buffer.resize(bytes_read);
			return buffer;
		}

		std::string read_line() override {
			std::string line;
			char ch;
			DWORD bytes_read;
			while (true) {
				if (!::ReadFile(native_handle(), &ch, 1, &bytes_read, nullptr) || bytes_read == 0) {
					break;
				}
				if (ch == '\n') break;
				line += ch;
			}
			return line;
		}

		HANDLE native_handle() const { return read_.get(); }
	};

	class pipe_writer : public io::writer {
		handle write_;

	public:
		explicit pipe_writer(HANDLE h) : write_(h) {}

		void write(const std::string& data) override {
			DWORD bytes_written = 0;
			if (!::WriteFile(native_handle(), data.data(), static_cast<DWORD>(data.size()), &bytes_written, nullptr)) {
				throw std::runtime_error("WriteFile to pipe failed: " + winapi::get_last_error());
			}
		}

		void flush() override {
			if (!::FlushFileBuffers(write_.get())) {
				throw std::runtime_error("Error flushing pipe: " + winapi::get_last_error());
			}
		}

		HANDLE native_handle() const { return write_.get(); }
	};
}
#endif