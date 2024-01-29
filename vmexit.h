#pragma once

#include "guest_registers.h"

namespace hv
{
	bool vm_launch();
	void vm_exit();
	bool handle_vm_exit(guest_registers* const ctx);
}
