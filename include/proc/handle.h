#pragma once
#include <Windows.h>

#ifdef _PROC_HANDLE
namespace proc {
	class handle {
		HANDLE h_ = INVALID_HANDLE_VALUE;

	public:
		handle() noexcept = default;

		explicit handle(HANDLE h) noexcept : h_(h) {}

		handle(const handle&) = delete;
		handle& operator=(const handle&) = delete;

		handle(handle&& other) noexcept : h_(other.h_) {
			other.h_ = INVALID_HANDLE_VALUE;
		}

		handle& operator=(handle&& other) noexcept {
			if (this != &other) {
				close();
				h_ = other.h_;
				other.h_ = INVALID_HANDLE_VALUE;
			}
			return *this;
		}

		~handle() {
			close();
		}

		bool valid() const noexcept {
			return h_ && h_ != INVALID_HANDLE_VALUE;
		}

		HANDLE get() const noexcept {
			return h_;
		}

		HANDLE release() noexcept {
			HANDLE temp = h_;
			h_ = INVALID_HANDLE_VALUE;
			return temp;
		}

		void reset(HANDLE h = INVALID_HANDLE_VALUE) noexcept {
			if (h_ != h) {
				close();
				h_ = h;
			}
		}

	private:
		void close() noexcept {
			if (valid()) {
				::CloseHandle(h_);
				h_ = INVALID_HANDLE_VALUE;
			}
		}
	};
}
#endif