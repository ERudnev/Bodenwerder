#pragma once

#include <format>
#include <stdexcept>

#include <iQSM/meta/concepts.h>
#include <iQSM/state/view.h>

namespace iqsm::manipulator::item {

    // experimental prototype of policy-independent manipulator
    template<meta::Aspect Meta>
    auto get(Reading, Id<Meta>) -> const Quantum<Meta>&;

    template<meta::Entity Meta>
    auto create(Writing, Quantum<Meta> value) -> Id<Meta>;
}


// impl:
namespace iqsm::manipulator::item {

template<meta::Aspect Meta>
auto get(Reading channel, Id<Meta> id) -> const Quantum<Meta>&
{
    const auto field = channel->slice<Meta>();

    if (not field->container.contains(id)) { throw std::runtime_error(std::format( "manipulator::item::get(): missing entity: {}", id)); }
    const auto& item = field->container.at(id);

    return Meta::Runtime::read(item);
}


    template<meta::Entity Meta>
    auto create(Writing channel, Quantum<Meta> value) -> Id<Meta>
    {
        _INCOMPLETE_;
        return Id<Meta>::generate_random();
    }

}