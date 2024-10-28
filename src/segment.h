#pragma once

#include "types.h"

extern "C"
{
	void _sgdt(segment_descriptor_register_64* gdtr);
	void _lgdt(segment_descriptor_register_64* gdtr);
}

namespace hv
{
	segment_selector read_cs();
	segment_selector read_ss();
	segment_selector read_ds();
	segment_selector read_es();
	segment_selector read_fs();
	segment_selector read_gs();
	segment_selector read_tr();
	segment_selector read_ldtr();

	void write_ds(uint16_t selector);
	void write_es(uint16_t selector);
	void write_fs(uint16_t selector);
	void write_gs(uint16_t selector);
	void write_tr(uint16_t selector);
	void write_ldtr(uint16_t selector);

	//base addr
	uint64_t segment_base(segment_descriptor_register_64 const& gdtr, segment_selector selector);
	uint64_t segment_base(segment_descriptor_register_64 const& gdtr, uint16_t selector);

	//access rights
	vmx_segment_access_rights segment_access(segment_descriptor_register_64 const& gdtr, segment_selector selector);
	vmx_segment_access_rights segment_access(segment_descriptor_register_64 const& gdtr, uint16_t selector);
}