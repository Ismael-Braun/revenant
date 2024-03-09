#include "hypercalls.h"
#include "vmx.h"
#include "hv.h"
#include "mm.h"

using namespace vmx;

extern "C" u8 __ImageBase;

namespace hypercalls
{
	auto ping(vcpu_t* vcpu) -> void
	{
		vcpu->ctx->rax = hv_signature;

		skip_instruction();
	}

	auto hv_base(vcpu_t* vcpu) -> void
	{
		vcpu->ctx->rax = reinterpret_cast<u64>(&__ImageBase);

		skip_instruction();
	}

	auto hide_physical_page(vcpu_t* vcpu) -> void
	{
		auto vmroot_cr3 = __readcr3();

		auto phys_addr = vcpu->ctx->rcx;

		__writecr3(ghv.system_cr3.flags);

		if (!ghv.ept->split_large_page(phys_addr))
		{
			__writecr3(vmroot_cr3);
			vcpu->ctx->rax = 0;
			skip_instruction();
			return;
		}

		auto pte = ghv.ept->get_pte(phys_addr);

		if (!pte)
		{
			__writecr3(vmroot_cr3);
			vcpu->ctx->rax = 0;
			skip_instruction();
			return;
		}

		pte->page_frame_number = ghv.ept->dummy_page_pfn;

		__writecr3(vmroot_cr3);

		ghv.ept->invalidate();

		vcpu->ctx->rax = 1;
		skip_instruction();
	}

	auto install_ept_hook(vcpu_t* vcpu) -> void
	{
		auto hint = reinterpret_cast<ept_hint*>(vcpu->ctx->rcx);

		vcpu->ctx->rax = ghv.ept->install_page_hook(hint);

		ghv.ept->invalidate();

		skip_instruction();
	}

	auto remove_ept_hook(vcpu_t* vcpu) -> void
	{
		auto virt = reinterpret_cast<void*>(vcpu->ctx->rcx);

		vcpu->ctx->rax = ghv.ept->remove_ept_hook(virt);

		skip_instruction();
	}

	auto current_dirbase(vcpu_t* vcpu) -> void
	{
		cr3 dirbase;
		dirbase.flags = vm_read(VMCS_GUEST_CR3);

		vcpu->ctx->rax = dirbase.address_of_page_directory << 12;

		skip_instruction();
	}

	auto copy_memory(vcpu_t* vcpu) -> void
	{
		auto dirbase_src = vcpu->ctx->rcx;
		auto virt_src = vcpu->ctx->rdx;
		auto dirbase_dest = vcpu->ctx->r8;
		auto virt_dest = vcpu->ctx->r9;
		auto size = vcpu->ctx->r10;

		vcpu->ctx->rax = hv::copy_virt(dirbase_src, virt_src, dirbase_dest, virt_dest, size);

		skip_instruction();
	}
}

