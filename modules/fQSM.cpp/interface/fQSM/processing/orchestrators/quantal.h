#pragma once

#include <fQSM/meta/interface.include.h>
#include <fQSM/processing/contexts/session.h>

namespace fqsm::processing::orchestrator {

    // Returned by modify(): holds its Writing, so a named gate keeps the session open until its scope ends.
    template<category::Any Meta>
    struct QuantumGate {
        const Id<Meta> id;

        explicit QuantumGate(Writing gate, Id<Meta> id) : id(id), gate(std::move(gate)) { quantum(); }
        QuantumGate(const QuantumGate&) = delete;
        QuantumGate& operator=(const QuantumGate&) = delete;

        Quantum<Meta>* operator->() { return &quantum(); }
        Quantum<Meta>& operator*() { return quantum(); }

    private:
        Writing gate;
        Quantum<Meta>& quantum() { return gate.workers_interface().updates<Meta>().get_modification_access(id); }
    };

    template<category::Any Meta>
    struct GlobalGate {
        explicit GlobalGate(Writing gate) : gate(std::move(gate)) { global(); }
        GlobalGate(const GlobalGate&) = delete;
        GlobalGate& operator=(const GlobalGate&) = delete;

        GlobalValue<Meta>* operator->() { return &global(); }
        GlobalValue<Meta>& operator*() { return global(); }

    private:
        Writing gate;
        GlobalValue<Meta>& global() { return gate.workers_interface().updates<Meta>().get_access_global(); }
    };
}
