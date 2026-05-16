#pragma once

#include <algorithm>
#include <string>
#include <vector>

//#include <iQSM/schema.h>

namespace tests::utilities {
    /*
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

    inline std::string type_names_multiline(const iqsm::SchemaObject& schema, std::string_view indent = "    ") {
        std::vector<const std::string*> names;
        names.reserve(schema.aspects.size());
        for (const auto& [_, e] : schema.aspects) { names.push_back(&e.name); }
        std::sort(names.begin(), names.end(), [](const auto* a, const auto* b) { return *a < *b; });

        if (names.empty()) return "{}";
        if (names.size() == 1) return std::string{"{"} + *names.front() + "}";

        std::string out;
        out.append("{\n");
        for (const auto* n : names) {
            out.append(indent);
            out.append(*n);
            out.append(",\n");
        }
        out.resize(out.size() - 2); // ",\n"
        out.push_back('\n');
        out.push_back('}');
        return out;
    }*/
}

