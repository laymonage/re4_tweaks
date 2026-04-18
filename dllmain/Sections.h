#pragma once
#include <Patterns.h>

namespace re4t::sections {
	extern hook::scan_segments data;
	extern hook::scan_segments rdata;
	extern hook::scan_segments text;
}

namespace re4t {
    inline hook::pattern pattern(std::string_view patternString)
    {
        return hook::pattern(re4t::sections::text, patternString);
    }
	inline hook::pattern pattern(hook::scan_segments segments, std::string_view patternString)
    {
        return hook::pattern(segments, patternString);
    }
}
