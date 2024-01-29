#include "logger.h"
#include <TraceLoggingProvider.h>
#include <stdio.h>

auto logger::dbg_log(log_level level, const char* format, ...) -> void
{
    char buffer[LOG_MAX_LEN] = { 0 };

    va_list args;
    va_start(args, format);
    _vsnprintf(buffer, LOG_MAX_LEN - 1, format, args);
    va_end(args);

    auto current_vcpu = KeGetCurrentProcessorNumber() + 1;

    switch (level)
    {
    case log_warning:
        DbgPrint("[-] [vcpu %d] warning | %s \n", current_vcpu, buffer);
        break;
    case log_info:
        DbgPrint("[+] [vcpu %d] information | %s \n", current_vcpu, buffer);
        break;
    case log_error:
        DbgPrint("[!] [vcpu %d] error | %s \n", current_vcpu, buffer);
        DbgBreakPoint();
        break;
    default:
        DbgPrint("[rvnt] %s \n", buffer);
        break;
    }
}