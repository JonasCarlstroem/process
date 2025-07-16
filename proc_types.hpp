#pragma once
#include <functional>
#include <string>

namespace proc {

using proc_handler = std::function<void(const std::string &)>;

}