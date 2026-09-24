#include <fQSM/utility/messages.h>

#include <format>
#include <stdexcept>

namespace fqsm::utility::messages {

    namespace {
        std::string id_text(RawId id) {
            return "#" + internal::id::info_hash(id);
        }
    }

    void throw_not_present(std::string_view operation, std::string_view aspect, RawId id) {
        throw std::runtime_error(std::format(R"({} "{}" {}: not present)", operation, aspect, id_text(id)));
    }

    std::string structural_missing(std::string_view missing, std::string_view forNew, RawId id) {
        return std::format("structural: {} missing for new {} {}", missing, forNew, id_text(id));
    }

    std::string structural_same_patch(std::string_view parent, std::string_view parasitic, RawId id) {
        return std::format("structural: {} must appear in the same patch as new {} {}", parent, parasitic, id_text(id));
    }

}
