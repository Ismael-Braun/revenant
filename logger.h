#pragma once

#include "types.h"

#define LOG_MAX_LEN 256

enum log_level
{
    log_warning,
    log_info,
    log_error
};

namespace logger
{
    auto dbg_log(log_level level, const char* format, ...) -> void;
}


