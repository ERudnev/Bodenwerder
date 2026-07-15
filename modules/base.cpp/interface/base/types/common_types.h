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
#include <glm/mat4x4.hpp>
#include <glm/gtc/quaternion.hpp>

#include <base/maybe.h>

namespace base::common_types {
    // Q1 builtins (mapped into the language root)
    using boolean = bool;
    using integer = std::int32_t;
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
    using mat4 = glm::mat4;
    using vec2 = glm::vec2;
    using vec3 = glm::vec3;
    using vec4 = glm::vec4;
    using quat = glm::quat;
}
