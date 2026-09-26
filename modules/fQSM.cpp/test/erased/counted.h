#pragma once

#include <string>
#include <utility>

namespace tests::erased {

    struct Counts {
        int constructed = 0;
        int copied = 0;
        int moved = 0;
        int destroyed = 0;

        int alive() const { return constructed + copied + moved - destroyed; }
    };

    struct Counted {
        Counts* counts;
        std::string text;

        Counted(Counts& c, std::string t) : counts(&c), text(std::move(t)) { ++counts->constructed; }
        Counted(const Counted& other) : counts(other.counts), text(other.text) { ++counts->copied; }
        Counted(Counted&& other) noexcept : counts(other.counts), text(std::move(other.text)) { ++counts->moved; }
        Counted& operator=(const Counted&) = delete;
        Counted& operator=(Counted&&) = delete;
        ~Counted() { ++counts->destroyed; }
    };
}
