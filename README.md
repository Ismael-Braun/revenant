# Revenant Intel VT-x Hypervisor

This project is an [Intel VT-x hypervisor](https://en.wikipedia.org/wiki/X86_virtualization#Intel_virtualization_(VT-x)), following the [Intel SDM](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)
closely as possible.

# Features
- Ept support with mapping of 2MB pages (splitted to 4KB pages if needed)
- Inline hooking via ept
- VM-exit handled cases (see at [vmexit.cpp](https://github.com/Ismael-Braun/revenant/blob/main/src/vmexit.cpp)): `EXCEPTION/NMI` `GETSEC` `INVD` `NMI WINDOW` `MOV CR` `RDMSR/WRMSR` `XSETBV` `VMXON` `VMCALL` `RDTSC/RDTSCP` `EPT VIOLATION` `EPT MISCONFIGURATION` `INVEPT` `VMCLEAR`

# Compilation
- [Visual Studio](https://visualstudio.microsoft.com/downloads/)
- [WDK](https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk)
- Spectre-mitigated libraries

# Supported platforms
- Windows 10, x64 only
- Windows 11 (not tested!)
  
# References
- [HyperPlatform](https://github.com/tandasat/HyperPlatform)
- [hvpp](https://github.com/wbenny/hvpp)
- [SimpleVisor](https://github.com/ionescu007/SimpleVisor)
- [HyperBone](https://github.com/DarthTon/HyperBone)
- [Hypervisor From Scratch](https://rayanfam.com/topics/hypervisor-from-scratch-part-1/)
- [hv](https://github.com/jonomango/hv)
