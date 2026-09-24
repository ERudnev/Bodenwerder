#pragma once

#include <string>
#include <string_view>

#include <fQSM/identifier.h>

// Diagnostic texts used inside per-aspect templates. Kept non-template so the
// std::format machinery is instantiated once, in messages.cpp, not once per aspect.
namespace fqsm::utility::messages {

    // '<operation> "<aspect>" #<id>: not present'
    [[noreturn]] void throw_not_present(std::string_view operation, std::string_view aspect, RawId id);

    // 'structural: <missing> missing for new <forNew> #<id>'
    std::string structural_missing(std::string_view missing, std::string_view forNew, RawId id);

    // 'structural: <parent> must appear in the same patch as new <parasitic> #<id>'
    std::string structural_same_patch(std::string_view parent, std::string_view parasitic, RawId id);

}
