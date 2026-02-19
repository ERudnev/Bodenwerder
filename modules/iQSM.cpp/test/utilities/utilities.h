#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include <iQSM/schema.h>

namespace tests::utilities {
    inline std::string type_names(const iqsm::SchemaObject& schema) {
        std::vector<const std::string*> names;
        names.reserve(schema.aspects.size());
        for (const auto& [_, e] : schema.aspects) { names.push_back(&e.name); }
        std::sort(names.begin(), names.end(), [](const auto* a, const auto* b) { return *a < *b; });

        std::string out;
        out.push_back('{');
        bool first = true;
        for (const auto* n : names) {
            if (!first) { out.append(", "); }
            first = false;
            out.append(*n);
        }
        out.push_back('}');
        return out;
    }
}

