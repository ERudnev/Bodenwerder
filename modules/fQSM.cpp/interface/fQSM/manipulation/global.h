#pragma once

#include <utility>

#include <fQSM/meta/interface.include.h>
#include <fQSM/processing/context.h>

namespace fqsm::manipulation::global {
    template<aspect::Any Meta>
    auto get(Reading) -> const GlobalValue<Meta>&;

    template<aspect::Any Meta>
    struct update;
}

//
// impl
namespace fqsm::manipulation::global {
    template<aspect::Any Meta>
    struct update {
        explicit update(Writing gate) : gate(std::move(gate)), buffer(this->gate.state.aspect<Meta>().global()) {}

        ~update() { gate.patch().aspect<Meta>().global() = std::move(buffer); }

        update(const update&) = delete;
        update& operator=(const update&) = delete;
        update(update&&) = delete;
        update& operator=(update&&) = delete;

        auto operator->() -> GlobalValue<Meta>* { return &buffer; }
        auto operator*() -> GlobalValue<Meta>& { return buffer; }

    private:
        Writing gate;
        GlobalValue<Meta> buffer;
    };

    template<aspect::Any Meta>
    auto get(Reading view) -> const GlobalValue<Meta>& {
        return view.aspect<Meta>.global();
    }
}
