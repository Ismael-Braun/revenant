#pragma once

#include <intrin.h>
#include <stdarg.h>
#include <stddef.h>
#include <ia32.hpp>
#include <ntddk.h>
#include <wdm.h>
#include <ia32.hpp>
#include "logger.h"

#define HV_POOL_TAG 'rvnt'

#define log_info(format, ...) logger::dbg_log(log_info, format, __VA_ARGS__)
#define log_warning(format, ...) logger::dbg_log(log_warning, format, __VA_ARGS__)
#define log_error(format, ...) logger::dbg_log(log_error, format, __VA_ARGS__)

using uptr = uintptr_t;
using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;
using u128 = __m128;

using s8 = char;
using s16 = short;
using s32 = int;
using s64 = long long;

using pinbased_ctls_t = ia32_vmx_pinbased_ctls_register;
using procbased_ctls_t = ia32_vmx_procbased_ctls_register;
using procbased2_ctls_t = ia32_vmx_procbased_ctls2_register;
using exit_ctls_t = ia32_vmx_exit_ctls_register;
using entry_ctls_t = ia32_vmx_entry_ctls_register;
using interrupt_state_t = vmx_interruptibility_state;
using feature_control_t = ia32_feature_control_register;

struct vmx_msr_entry
{
	u32 msr_idx;
	u32 _reserved;
	u64 msr_data;
};
