
#include <Atomic/varph.q1.h>
#include <iQSM/operations/cache.h>
#include <iQSM/operations/particle.h>

// Spark:
namespace Q1CORE::Example::Varph {
    const Invariants Spark::invariants{{{
    }}};
}

// Inertia:
namespace Q1CORE::Example::Varph {
    const Invariants Inertia::invariants{{{
        Invariants::anchor_attribute<Spark, Inertia>,
    }}};
}


// Charge:
namespace Q1CORE::Example::Varph {

    namespace Charge_impl {
        float force_between(World, Spark::Id, Spark::Id) { return 0.0f; }
    }

    auto Charge::Operations::value(World world, Spark::Id spark)->integer {
        if (not iqsm::ops::particle::exists<Charge>(world, spark)) { return 0; }
        return iqsm::ops::particle::get<Charge>(world, spark).value;
    }

    const Invariants Charge::invariants{{{
        Invariants::anchor_attribute<Spark, Charge>,
    }}};
}

// Strong:
namespace Q1CORE::Example::Varph {
    const Invariants Strong::invariants{{{
        Invariants::anchor_attribute<Spark, Strong>,
    }}};
}

// TextDescription:
namespace Q1CORE::Example::Varph {
    const Invariants TextDescription::invariants{{{
    }}};
}

// Electron:
namespace Q1CORE::Example::Varph {
    const Invariants Electron::invariants{{{
        Invariants::anchor_attribute<Charge, Electron>,
    }}};
}

// Hadron:
namespace Q1CORE::Example::Varph {
    namespace Hadron_impl {
        auto symbol_of(integer isospin2) -> string {
            if (isospin2 == +1) return "P";
            if (isospin2 == -1) return "N";
            return "?";
        }

        auto update(World world, Hadron::Id id) -> Facet<Hadron>::Item {
            const auto before = world->field<Hadron>()->container.at(id);
            const auto& hadron = *before;

            const auto& strong = iqsm::ops::particle::get<Strong>(world, id);
            const auto next_legend = symbol_of(strong.isospin2);
            if (hadron.legend == next_legend) return before;

            auto updated = hadron;
            updated.legend = next_legend;
            return Facet<Hadron>::create(std::move(updated));
        }
    }

    const Invariants Hadron::invariants{{{
        Invariants::anchor_attribute<Strong, Hadron>,
        &iqsm::ops::cache::update<Hadron, &Hadron_impl::update>,
    }}};
}

// Atom:
namespace Q1CORE::Example::Varph {
    namespace Atom_impl {
        auto symbol_of(integer z) -> string {
            if (z == 1) return "H";
            if (z == 2) return "He";
            return "X";
        }

        auto z_of(World world, const std::vector<Hadron::Id>& hadrons) -> integer {
            integer z = 0;
            for (const auto& hid : hadrons) {
                const auto q = Charge::Operations::value(world, hid);
                if (q > 0) z += q;
            }
            return z;
        }

        auto update(World world, Atom::Id id) -> Facet<Atom>::Item {
            const auto before = world->field<Atom>()->container.at(id);
            const auto& atom = *before;

            const auto next_legend = symbol_of(z_of(world, atom.core));
            if (atom.legend == next_legend) return before;

            auto updated = atom;
            updated.legend = next_legend;
            return Facet<Atom>::create(std::move(updated));
        }
    }

    const Invariants Atom::invariants{{{
        Invariants::anchor_any<Hadron, Atom, &Quantum::core>,
        &iqsm::ops::cache::update<Atom, &Atom_impl::update>,
    }}};
}

// Chemical:
namespace Q1CORE::Example::Varph {
    namespace Chemical_impl {
        auto ionisation_of(World world, const Atom::Quantum& atom) -> integer {
            integer total_charge = 0;
            for (const auto& id : atom.core) { total_charge += Charge::Operations::value(world, id); }
            for (const auto& id : atom.captured) {
                total_charge += Charge::Operations::value(world, id);
            }
            return total_charge;
        }

        Chemical::Quantum construct(World world, const Atom::Quantum& atom) {
            return Chemical::Quantum{
                .ionisation = ionisation_of(world, atom),
                .valency = 0,
            };
        }

        auto update(World w, Chemical::Id id) -> Facet<Chemical>::Item {
            const auto before = w->field<Chemical>()->container.at(id);
            const auto& chemical = *before;

            const auto& atom = iqsm::ops::particle::get<Atom>(w, id);
            const auto ionisation = ionisation_of(w, atom);
            if (chemical.ionisation == ionisation) return before;

            auto updated = chemical;
            updated.ionisation = ionisation;
            return Facet<Chemical>::create(std::move(updated));
        }
    }

    const Invariants Chemical::invariants{{{
        Invariants::anchor_component<Atom, Chemical, &Chemical_impl::construct>,
        &iqsm::ops::cache::update<Chemical, &Chemical_impl::update>,
    }}};
}


// Capture:
namespace Q1CORE::Example::Varph {
    const Invariants Capture::invariants{{{
        Invariants::anchor<Atom, Capture, &Quantum::atom>,
        Invariants::anchor<Electron, Capture, &Quantum::electron>,
    }}};
}

// Modecule:
namespace Q1CORE::Example::Varph {
    const Invariants Modecule::invariants{{{
        Invariants::anchor_any<Atom, Modecule, &Quantum::atoms>,
    }}};
}

// Binding:
namespace Q1CORE::Example::Varph {
    const Invariants Binding::invariants{{{
        Invariants::anchor_all<Atom, Binding, &Quantum::bound>,
    }}};
}

