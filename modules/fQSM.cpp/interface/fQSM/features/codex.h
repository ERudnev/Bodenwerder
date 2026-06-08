#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include <fQSM/features/_forwards.h>
#include <fQSM/features/norma.h>

namespace fqsm::features {

    struct Codex {
        const Normas normas;

        Codex() = default;

        Codex(Normas in) : normas(std::move(in)) {}

        // syntax sugar for Aspect definitions to allow {normaA(), normaB(p), normaC(c,d)}
        template<typename FirstNorma, typename... RestNormas, typename = std::enable_if_t<std::is_base_of_v<features::Norma, std::decay_t<FirstNorma>> && (std::is_base_of_v<features::Norma, std::decay_t<RestNormas>> && ...)>>
        Codex(FirstNorma&& firstNorma, RestNormas&&... restNormas)
            : normas(make_normas(std::forward<FirstNorma>(firstNorma), std::forward<RestNormas>(restNormas)...)) {}

    private:
        template<typename FirstNorma, typename... RestNormas>
        static Normas make_normas(FirstNorma&& firstNorma, RestNormas&&... restNormas) {
            Normas out;
            out.reserve(1 + sizeof...(RestNormas));

            out.push_back(std::make_shared<std::decay_t<FirstNorma>>(std::forward<FirstNorma>(firstNorma)));
            (out.push_back(std::make_shared<std::decay_t<RestNormas>>(std::forward<RestNormas>(restNormas))), ...);

            return out;
        }
    };
}