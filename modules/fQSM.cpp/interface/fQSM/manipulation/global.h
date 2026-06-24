#pragma once

#include <utility>

#include <fQSM/meta/interface.include.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::manipulation::global {
    template<category::Any Meta>
    auto get(Reading context) -> const GlobalValue<Meta>&;

    template<category::Any Meta>
    struct update;
}

//
// impl
namespace fqsm::manipulation::global {
    template<category::Any Meta>
    auto get(Reading context) -> const GlobalValue<Meta>& {
        return context.aspect<Meta>().global();
    }

    template<category::Any Meta>
    struct update {
        explicit update(Writing gate) : gate(std::move(gate)), buffer(this->gate->aspect<Meta>().global()) {}

        ~update() { gate.patch().aspect<Meta>().global = std::move(buffer); }

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

}
