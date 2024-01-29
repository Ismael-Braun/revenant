#pragma once

#include "hypercalls.h"
#include "vmx.h"
#include "vcpu.h"
#include "mm.h"
#include "ept.h"

typedef struct hypervisor_t
{
	u32 vcpu_count;
	vcpu_t* vcpus;
	cr3 system_cr3;
	ept_t* ept;

	alignas(0x1000) pml4e_64 page_table_pml4[512];
	cr3 page_table_cr3;
};

namespace hv
{
	extern hypervisor_t ghv;

	auto start_hv() -> bool;
}

