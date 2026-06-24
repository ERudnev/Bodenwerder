#include <fQSM/utility/logging.h>

#include <fQSM/model/analysis.h>
#include <fQSM/model/complex/patch.h>
#include <fQSM/model/structure/schema.h>

namespace fqsm::utility {

    void log_patch(std::string_view legend, cref<model::complex::Patch> patch) {
        const analysis::Patch stats{*patch};
        const auto& overall = stats.overall;

        const auto summary = overall.patchlets == 0
            ? std::string{"empty"}
            : std::format("{{H:{}:S:{}}}", overall.nonEmptyLines, overall.patchlets);

        base::message(std::format("{}: {}", legend, summary));

        for (const auto& [rtid, node] : patch->schema->nodes) {
            const auto line = node.binding.logPatchSlice(*patch, node.name);
            if (!line.empty()) base::message("    {}", line);
        }
    }

}
