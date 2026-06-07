#pragma once

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include <fQSM/features/norma.h>

namespace fqsm::features {

    struct Codex {
        std::vector<std::unique_ptr<features::Norma>> normas;

        Codex() = default;

        Codex(std::vector<std::unique_ptr<features::Norma>> normas)
            : normas(std::move(normas)) {}

        template<typename FirstNorma, typename... RestNormas,
            typename = std::enable_if_t<std::is_base_of_v<features::Norma, std::decay_t<FirstNorma>> && (std::is_base_of_v<features::Norma, std::decay_t<RestNormas>> && ...)>>
        Codex(FirstNorma&& firstNorma, RestNormas&&... restNormas) {
            normas.reserve(1 + sizeof...(RestNormas));

            append(std::forward<FirstNorma>(firstNorma));
            (append(std::forward<RestNormas>(restNormas)), ...);
        }

    private:
        template<typename NormaType>
        void append(NormaType&& norma) {
            using StoredNorma = std::decay_t<NormaType>;
            normas.push_back(std::make_unique<StoredNorma>(std::forward<NormaType>(norma)));
        }
    };
}