#include "dllmain.h"

// To speed up pattern scans in specific sections.
namespace re4t::sections {
	hook::scan_segments data;
	hook::scan_segments rdata;
	hook::scan_segments text;
}

void re4t::init::Sections() {
	void* module = GetModuleHandle(nullptr);
	re4t::sections::data = hook::get_section_by_name(module, ".data");
	re4t::sections::rdata = hook::get_section_by_name(module, ".rdata");
	re4t::sections::text = hook::get_section_by_name(module, ".text");
}
