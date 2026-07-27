#pragma once

#include <format>
#include <stdexcept>
#include <utility>

#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/categories.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/model/linear/patch.h>
#include <fQSM/processing/contexts/operational.h>

namespace fqsm::processing::orchestrator {

template<category::Any Meta>
struct QuantumGate {
    const Id<Meta> id;

    explicit QuantumGate(Writing gate, Id<Meta> id) : id(id), gate(std::move(gate)) {
        workers().get_modification_access(id); // ensure?
    }

    QuantumGate(const QuantumGate&) = delete;
    QuantumGate& operator=(const QuantumGate&) = delete;
    QuantumGate(QuantumGate&&) = delete;
    QuantumGate& operator=(QuantumGate&&) = delete;

    Quantum<Meta>* operator->() { return &quantum(); }
    Quantum<Meta>& operator*() { return quantum(); }

private:
    Writing gate;

    auto& workers() {
        return gate.workers_interface().updates<Meta>();
    }

    Quantum<Meta>& quantum() {
        return workers().get_modification_access(id);
    }
};

template<category::Any Meta>
struct GlobalGate {
    explicit GlobalGate(Writing gate) : gate(std::move(gate)) {
        workers().get_access_global(); // ensure?
    }

    GlobalGate(const GlobalGate&) = delete;
    GlobalGate& operator=(const GlobalGate&) = delete;
    GlobalGate(GlobalGate&&) = delete;
    GlobalGate& operator=(GlobalGate&&) = delete;

    GlobalValue<Meta>* operator->() { return &global(); }
    GlobalValue<Meta>& operator*() { return global(); }

private:
    Writing gate;

    auto& workers() {
        return gate.workers_interface().updates<Meta>();
    }

    GlobalValue<Meta>& global() {
        return workers().get_access_global();
    }
};
}