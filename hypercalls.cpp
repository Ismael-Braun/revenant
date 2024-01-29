#include "hypercalls.h"
#include "vmx.h"
#include "hv.h"

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
		}

		auto pte = ghv.ept->get_pte(phys_addr);

		if (!pte)
		{
			__writecr3(vmroot_cr3);
			vcpu->ctx->rax = 0;
			skip_instruction();
		}

		pte->page_frame_number = ghv.ept->dummy_page_pfn;

		__writecr3(vmroot_cr3);

		ghv.ept->invalidate();

		vcpu->ctx->rax = 1;
		skip_instruction();
	}

	auto install_ept_hook(vcpu_t* vcpu) -> void
	{
		auto addr = reinterpret_cast<void*>(vcpu->ctx->rcx);
		auto patch = reinterpret_cast<u8*>(vcpu->ctx->rdx);
		auto patch_size = vcpu->ctx->r8;
		auto hint = reinterpret_cast<ept_hint*>(vcpu->ctx->r9);
		auto mdl = reinterpret_cast<PMDL>(vcpu->ctx->r10);

		vcpu->ctx->rax = ghv.ept->install_page_hook(addr, patch, patch_size, hint, mdl);

		ghv.ept->invalidate();

		skip_instruction();
	}

	auto remove_ept_hook(vcpu_t* vcpu) -> void
	{
		auto virt = reinterpret_cast<void*>(vcpu->ctx->rcx);

		vcpu->ctx->rax = (u64)ghv.ept->remove_ept_hook(virt);

		skip_instruction();
	}
}

