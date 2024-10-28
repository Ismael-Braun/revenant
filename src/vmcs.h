#pragma once

#include "vcpu.h"

namespace hv
{
	auto setup_vmcs_ctrl(vcpu_t* cpu) -> void;
	auto setup_vmcs_host(vcpu_t const* cpu) -> void;
	auto setup_vmcs_guest() -> void;
}

