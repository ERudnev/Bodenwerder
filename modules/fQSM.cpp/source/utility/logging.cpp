#include <fQSM/utility/logging.h>

#include <format>
#include <string>
#include <vector>
#include <fQSM/model/intertype/schema.h>

namespace fqsm::utility {

    void log_patch(std::string_view legend, cref<model::complex::Patch> patch) {
        std::vector<std::string> lines;
        for (const auto& [_, node] : patch->schema->nodes) {
            const auto line = node.binding.patch.log(*patch, node.name);
            if (!line.empty()) lines.push_back(line);
        }

        const auto patchlets = patch->quanta();
        const auto summary = patchlets == 0
            ? std::string{"empty"}
            : std::format("{{H:{}:S:{}}}", lines.size(), patchlets);

        base::message(std::format("{}: {}", legend, summary));

        for (const auto& line : lines)
            base::message("    {}", line);
    }

}
