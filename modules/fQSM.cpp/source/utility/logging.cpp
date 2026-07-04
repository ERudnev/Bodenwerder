#include <fQSM/utility/logging.h>

#include <format>
#include <string>
#include <vector>
#include <fQSM/model/intertype/schema.h>

namespace fqsm::utility {

    auto format_patch(cref<model::complex::Patch> patch) -> std::string {
        return format_patch(*patch);
    }

    auto format_patch(const model::complex::Patch& patch) -> std::string {
        std::vector<std::string> lines;
        for (const auto& [_, node] : patch.schema->nodes) {
            const auto line = node.binding.patch.log(patch, node.name);
            if (!line.empty()) lines.push_back(line);
        }

        const auto summary = not patch.has_changes()
            ? std::string{"empty"}
            : std::format("{{H:{}}}", lines.size());

        if (lines.empty()) return summary;

        std::string body;
        for (std::size_t i = 0; i < lines.size(); ++i) {
            if (i != 0) body += ' ';
            body += lines[i];
        }
        return std::format("{} {}", summary, body);
    }

    void log_patch(std::string_view legend, cref<model::complex::Patch> patch) {
        std::vector<std::string> lines;
        for (const auto& [_, node] : patch->schema->nodes) {
            const auto line = node.binding.patch.log(*patch, node.name);
            if (!line.empty()) lines.push_back(line);
        }

        const auto summary = not patch->has_changes()
            ? std::string{"empty"}
            : std::format("{{H:{}}}", lines.size());

        base::message(std::format("{}: {}", legend, summary));

        for (const auto& line : lines)
            base::message("    {}", line);
    }

    void log_rejected_transaction(const processing::review::Result& result) {
        if (result.good()) return;

        base::message("Transaction rejected; proposed changes were not applied. Reported issues:");
        for (const auto& msg : result.critical)
            base::message("  critical: {}", msg);
        for (const auto& msg : result.warning)
            base::message("  warning: {}", msg);
    }

}
