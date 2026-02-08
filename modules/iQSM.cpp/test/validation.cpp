#include <base/testing/macros.h>

#include <Atomic/model.q1.h>
#include <iQSM/field.h>
#include <iQSM/operations/integration.h>
#include <iQSM/operations/validators.h>
#include <iQSM/world.h>

namespace tests {
    void validation_anchor() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Model;

        // Scenario 1: Atom.link anchors to Molecule
        {
            auto basis = DagState::define<Element, Molecule, Atom>();
            auto world = std::make_shared<WorldState>(basis);

            const auto element_id = Element::Id::generate_random();

            auto molecules = std::make_shared<FieldState<Molecule>>();
            const auto molecule_ok = Molecule::Id::generate_random();
            molecules->container = molecules->container.insert(molecule_ok, Aspect<Molecule>::create({"first"}));

            auto atoms = std::make_shared<FieldState<Atom>>();
            const auto atom_ok = Atom::Id::generate_random();
            const auto atom_bad = Atom::Id::generate_random();
            atoms->container = atoms->container
                .insert(atom_ok, Aspect<Atom>::create({element_id, molecule_ok}))
                .insert(atom_bad, Aspect<Atom>::create({element_id, Molecule::Id::generate_random()}));

            world->fields = world->fields
                .insert(Aspect<Molecule>::typeId, std::static_pointer_cast<const FieldUntyped>(freeze(molecules)))
                .insert(Aspect<Atom>::typeId, std::static_pointer_cast<const FieldUntyped>(freeze(atoms)));

            EXPECT_EQ(world->field<Atom>()->container.size(), size_t{2});

            auto delta = validators::structural::anchor<Molecule, Atom>(
                freeze(world),
                [](Aspect<Atom>::Item item) { return item->link; });

            auto next = integrate(freeze(world), delta);
            EXPECT_EQ(next->field<Atom>()->container.size(), size_t{1});
            EXPECT_TRUE(next->field<Atom>()->container.contains(atom_ok));
            EXPECT_TRUE(not next->field<Atom>()->container.contains(atom_bad));
        }

        // Scenario 2: Position (quark) anchors to Atom by id
        {
            auto basis = DagState::define<Element, Molecule, Atom, Position>();
            auto world = std::make_shared<WorldState>(basis);

            const auto element_id = Element::Id::generate_random();

            auto molecules = std::make_shared<FieldState<Molecule>>();
            const auto molecule_ok = Molecule::Id::generate_random();
            molecules->container = molecules->container.insert(molecule_ok, Aspect<Molecule>::create({"first"}));

            auto atoms = std::make_shared<FieldState<Atom>>();
            const auto atom_ok = Atom::Id::generate_random();
            atoms->container = atoms->container.insert(atom_ok, Aspect<Atom>::create({element_id, molecule_ok}));

            auto positions = std::make_shared<FieldState<Position>>();
            positions->container = positions->container
                .insert(atom_ok, Aspect<Position>::create({Vec3{0, 0, 0}}))
                .insert(Atom::Id::generate_random(), Aspect<Position>::create({Vec3{1, 1, 1}}));

            world->fields = world->fields
                .insert(Aspect<Molecule>::typeId, std::static_pointer_cast<const FieldUntyped>(freeze(molecules)))
                .insert(Aspect<Atom>::typeId, std::static_pointer_cast<const FieldUntyped>(freeze(atoms)))
                .insert(Aspect<Position>::typeId, std::static_pointer_cast<const FieldUntyped>(freeze(positions)));

            EXPECT_EQ(world->field<Position>()->container.size(), size_t{2});

            auto delta = validators::structural::anchor_quark<Atom, Position>(freeze(world));
            auto next = integrate(freeze(world), delta);

            EXPECT_EQ(next->field<Position>()->container.size(), size_t{1});
            EXPECT_TRUE(next->field<Position>()->container.contains(atom_ok));
        }
    }
}


