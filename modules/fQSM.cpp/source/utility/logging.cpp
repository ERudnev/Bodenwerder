#include <fQSM/utility/logging.h>

#include <format>
#include <string>
#include <vector>
#include <fQSM/erased/algorithms.h>
#include <fQSM/model/intertype/schema.h>
#include <fQSM/model/complex/patch.h>

namespace fqsm::utility {

    namespace {
        void collect_lines(const model::complex::Patch& patch, std::vector<std::string>& out) {
            for (model::complex::Patch::Slot slot = 0; slot < patch.schema->slotCount(); ++slot) {
                const auto* line = patch.line(slot);
                if (not line) continue;
                auto text = erased::format_patch_line(*line, patch.schema->descriptors[slot].name);
                if (not text.empty()) out.push_back(std::move(text));
            }
        }
    }

    auto format_patch(cref<model::complex::Patch> patch) -> std::string {
        return format_patch(*patch);
    }

    auto format_patch(const model::complex::Patch& patch) -> std::string {
        std::vector<std::string> lines;
        collect_lines(patch, lines);

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

    void log_rejected_transaction(const model::complex::Patch::Summary& result) {
        if (result.good()) return;

        base::message("Transaction rejected; proposed changes were not applied. Reported issues:");
        for (const auto& msg : result.critical)
            base::message("  critical: {}", msg);
        for (const auto& msg : result.warning)
            base::message("  warning: {}", msg);
    }

}
