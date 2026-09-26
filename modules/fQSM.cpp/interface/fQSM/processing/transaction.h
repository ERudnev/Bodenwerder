#pragma once

#include <cstddef>

#include <fQSM/model/_forwards.h>
#include <fQSM/processing/contexts/session.h>

namespace fqsm::processing {

    // Something that gives Writing: a Realm (normalizes and integrates) or a Branch (merges into its parent).
    struct Transaction {
        enum class Mode {
            normal,
            silent,
        };

        virtual ~Transaction() = default;

        virtual operator Reading() const = 0;
        operator Writing() { return writing(Mode::normal); }
        Writing silent_work() { return writing(Mode::silent); }

    protected:
        friend struct SettingUp;
        friend struct orchestrator::Branch;

        virtual auto writing(Mode) -> Writing = 0;
        // A child Branch reads this state and hands its patch back when it closes.
        virtual auto child_base() const -> const model::complex::State& = 0;
        virtual void accept_child(ref<model::complex::Patch>) = 0;

        std::size_t depth = 0;   // patch layers over the reality: 0 for a Realm, parent + 1 for a Branch
    };
}
