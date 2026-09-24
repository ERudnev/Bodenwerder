# Precompiled headers for project targets. Vendored code under 3party/ is not touched.
# Usage: daql_enable_pch(<target> <header> ...) ; unknown target names are skipped.
# Measured 2026-09-24 on MSVC: about 20 percent off a clean library build; the parse of
# the standard headers (82 percent of a preprocessed TU) is what it removes.

set(DAQL_PCH_STD
    <algorithm> <array> <cmath> <cstdint> <format> <functional> <map> <memory>
    <optional> <set> <string> <string_view> <unordered_map> <unordered_set> <utility> <vector>
)
set(DAQL_PCH_BASE <base/logging.h> <base/maybe.h>)
set(DAQL_PCH_FQSM <fQSM/api/interface.h>)
set(DAQL_PCH_GLM <glm/glm.hpp> <glm/gtc/quaternion.hpp>)

function(daql_enable_pch DAQL_TARGET)
    if(NOT TARGET ${DAQL_TARGET})
        message(STATUS "PCH: target '${DAQL_TARGET}' not found, skipped")
        return()
    endif()
    get_target_property(DAQL_TARGET_TYPE ${DAQL_TARGET} TYPE)
    if(DAQL_TARGET_TYPE STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()
    target_precompile_headers(${DAQL_TARGET} PRIVATE ${ARGN})
endfunction()
