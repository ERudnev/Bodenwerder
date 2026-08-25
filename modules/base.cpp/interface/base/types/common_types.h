#pragma once

#include <cstdint>
#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <glm/ext/vector_double3.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_int3x3.hpp>
#include <glm/ext/vector_int3.hpp>
#include <glm/gtc/quaternion.hpp>

#include <base/maybe.h>

namespace base::common_types {
    // Q1 builtins (mapped into the language root)
    using boolean = bool;
    using integer = std::int32_t;
    // Wide counters / clocks (µs ticks, etc.) — not a drop-in for `integer`.
    using int64 = std::int64_t;
    // Q1 builtin `float` maps to C++ keyword type: `float`
    using string = std::string;
    // Q1 builtin: filesystem path.
    using filepath = std::filesystem::path;
    // Q1 builtin: file name within a kit/complect (not a full path).
    using filename = std::string;

    struct index2 final {
        integer x;
        integer y;
    };

    struct index3 final {
        integer x;
        integer y;
        integer z;
    };

    template<typename T>
    using optional = std::optional<T>; // TODO: replace with "maybe" someday...
    template<typename T>
    using maybe = base::maybe<T>;
    template<typename T>
    using vector = std::vector<T>;
    template<typename K, typename V>
    using map = std::map<K, V>;
    template<typename T>
    using set = std::set<T>;
    template<typename K, typename V>
    using umap = std::unordered_map<K, V>;
    template<typename T>
    using uset = std::unordered_set<T>;

    // Q1 builtin: relative time duration (in seconds).
    using seconds = double;

    // Q1 builtin: timepoint (absolute).
    using timepoint = std::chrono::system_clock::time_point;

    // Q1 builtins: glm-compatible math types.
    // Exposed as aliases to avoid requiring `glm::` at every field site,
    // without importing the whole glm namespace into generated code.
    using mat3 = glm::mat3;
    using mat4 = glm::mat4;
    using imat3 = glm::imat3;
    using vec2 = glm::vec2;
    using vec3 = glm::vec3;
    using dvec3 = glm::dvec3;
    using vec4 = glm::vec4;
    using ivec3 = glm::ivec3;
    using quat = glm::quat;
}

// sweet sugar:
namespace base::common_types {
    inline auto rgb(int r, int g, int b) -> vec3 {
        return {r / 255.f, g / 255.f, b / 255.f};
    }
}
