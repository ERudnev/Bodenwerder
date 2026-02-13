#include <base/testing/macros.h>

#include <Atomic/model.q1.h>
#include <iQSM/field.h>
#include <iQSM/operations/integration.h>
#include <iQSM/operations/validation.h>
#include <iQSM/schema.h>
#include <iQSM/world.h>

namespace tests {
    void validation_anchor() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Model;

        // Scenario 1: Atom.link anchors to Molecule
        {
            auto schema = std::make_shared<const SchemaObject>(SchemaObject::assemble<Atom>());
            auto world = std::make_shared<WorldObject>(schema);

            const auto element_id = Element::Id::generate_random();

            auto molecules = std::make_shared<FieldObject<Molecule>>();
            const auto molecule_ok = Molecule::Id::generate_random();
            molecules->container = molecules->container.insert(molecule_ok, Aspect<Molecule>::create({"first"}));

            auto atoms = std::make_shared<FieldObject<Atom>>();
            const auto atom_ok = Atom::Id::generate_random();
            const auto atom_bad = Atom::Id::generate_random();
            atoms->container = atoms->container
                .insert(atom_ok, Aspect<Atom>::create({element_id, molecule_ok}))
                .insert(atom_bad, Aspect<Atom>::create({element_id, Molecule::Id::generate_random()}));

            world->fields = world->fields
                .insert(Aspect<Molecule>::typeId, std::static_pointer_cast<const FieldAbstract>(freeze(molecules)))
                .insert(Aspect<Atom>::typeId, std::static_pointer_cast<const FieldAbstract>(freeze(atoms)));

            EXPECT_EQ(world->field<Atom>()->container.size(), size_t{2});

            auto delta = ops::validation::Structural::anchor<Molecule, Atom, &Atom::Quantum::molecule>(freeze(world));

            auto next = ops::integrate_raw(freeze(world), delta);
            EXPECT_EQ(next->field<Atom>()->container.size(), size_t{1});
            EXPECT_TRUE(next->field<Atom>()->container.contains(atom_ok));
            EXPECT_TRUE(not next->field<Atom>()->container.contains(atom_bad));
        }

        // Scenario 2: Position (quark) anchors to Atom by id
        {
            auto schema = std::make_shared<const SchemaObject>(SchemaObject::assemble<Position>());
            auto world = std::make_shared<WorldObject>(schema);

            const auto element_id = Element::Id::generate_random();

            auto molecules = std::make_shared<FieldObject<Molecule>>();
            const auto molecule_ok = Molecule::Id::generate_random();
            molecules->container = molecules->container.insert(molecule_ok, Aspect<Molecule>::create({"first"}));

            auto atoms = std::make_shared<FieldObject<Atom>>();
            const auto atom_ok = Atom::Id::generate_random();
            atoms->container = atoms->container.insert(atom_ok, Aspect<Atom>::create({element_id, molecule_ok}));

            auto positions = std::make_shared<FieldObject<Position>>();
            positions->container = positions->container
                .insert(atom_ok, Aspect<Position>::create({Vec3{0, 0, 0}}))
                .insert(Atom::Id::generate_random(), Aspect<Position>::create({Vec3{1, 1, 1}}));

            world->fields = world->fields
                .insert(Aspect<Molecule>::typeId, std::static_pointer_cast<const FieldAbstract>(freeze(molecules)))
                .insert(Aspect<Atom>::typeId, std::static_pointer_cast<const FieldAbstract>(freeze(atoms)))
                .insert(Aspect<Position>::typeId, std::static_pointer_cast<const FieldAbstract>(freeze(positions)));

            EXPECT_EQ(world->field<Position>()->container.size(), size_t{2});

            auto delta = ops::validation::Structural::anchor_quark<Atom, Position>(freeze(world));
            auto next = ops::integrate_raw(freeze(world), delta);

            EXPECT_EQ(next->field<Position>()->container.size(), size_t{1});
            EXPECT_TRUE(next->field<Position>()->container.contains(atom_ok));
        }
    }
}


