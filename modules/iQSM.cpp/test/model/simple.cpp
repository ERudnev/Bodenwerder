#include "simple.h"

#include <Atomic/model.q1.h>
#include <iQSM/dag.h>
#include <iQSM/field.h>

iqsm::World SimpleModel::generate() {
    using namespace iqsm;
    using namespace Q1CORE::Example::Model;

    auto basis = DagState::define<Element, Molecule, Atom, Position>();

    auto elements = std::make_shared<FieldState<Element>>();
    elements->container = elements->container
        .insert(Element::Id::generate_random(), Aspect<Element>::create({"Hydrogen", seconds{0}, integer{1}}))
        .insert(Element::Id::generate_random(), Aspect<Element>::create({"Oxygen", seconds{0}, integer{2}}));

    auto molecules = std::make_shared<FieldState<Molecule>>();
    molecules->container = molecules->container
        .insert(Molecule::Id::generate_random(), Aspect<Molecule>::create({"first"}))
        .insert(Molecule::Id::generate_random(), Aspect<Molecule>::create({"second"}));

    auto world = std::make_shared<WorldState>(basis);
    world->fields = world->fields
        .insert(Aspect<Element>::typeId, std::static_pointer_cast<const FieldUntyped>(freeze(elements)))
        .insert(Aspect<Molecule>::typeId, std::static_pointer_cast<const FieldUntyped>(freeze(molecules)));

    return freeze(world);
}


