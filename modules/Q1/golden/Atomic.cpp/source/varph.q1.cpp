
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

    integer Charge::Operations::value(World world, Spark::Id spark) {
        required(world, "Charge::Operations::value(): world");
        if (not iqsm::ops::particle::exists<Charge>(world, spark)) { return 0; }
        return iqsm::ops::particle::get<Charge>(world, spark).value;
    }

    const Invariants Charge::invariants{{{
        Invariants::anchor_attribute<Spark, Charge>,
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
        Invariants::anchor<Spark, Electron, &Quantum::spark>,
    }}};
}

// Atom:
namespace Q1CORE::Example::Varph {
    namespace Atom_impl {
        static auto symbol_of(integer z) -> string {
            if (z == 1) return "H";
            if (z == 2) return "He";
            return "X";
        }

        static auto legend_of(integer z, integer ionisation) -> string {
            auto out = symbol_of(z);
            if (ionisation == 0) return out;
            if (ionisation == 1) return out + "+";
            if (ionisation == -1) return out + "-";
            // keep it simple in the golden example: no numeric suffixes yet
            return out;
        }

        Delta update(World world) {
            return iqsm::ops::cache::recompute<Atom>(world, [&](World w, Atom::Id, const Atom::Quantum& atom) -> std::optional<Atom::Quantum> {
                integer z = 0;
                integer ionisation = 0;

                for (const auto& id : atom.core) {
                    const auto q = Charge::Operations::value(w, id);
                    if (q > 0) z += q;
                    ionisation += q;
                }
                for (const auto& id : atom.captured) {
                    const auto& electron = iqsm::ops::particle::get<Electron>(w, id);
                    ionisation += Charge::Operations::value(w, electron.spark);
                }

                const auto legend = legend_of(z, ionisation);
                if (atom.legend == legend) return std::nullopt;

                auto updated = atom;
                updated.legend = legend;
                return updated;
            });
        }
    }

    const Invariants Atom::invariants{{{
        Invariants::anchor_any<Spark, Atom, &Quantum::core>,
        &Atom_impl::update,
    }}};
}

// Chemical:
namespace Q1CORE::Example::Varph {
    namespace Chemical_impl {
        static auto ionisation_of(World world, const Atom::Quantum& atom) -> integer {
            integer total_charge = 0;
            for (const auto& id : atom.core) { total_charge += Charge::Operations::value(world, id); }
            for (const auto& id : atom.captured) {
                const auto& electron = iqsm::ops::particle::get<Electron>(world, id);
                total_charge += Charge::Operations::value(world, electron.spark);
            }
            return total_charge;
        }

        Chemical::Quantum construct(World world, const Atom::Quantum& atom) {
            required(world, "Chemical::construct(): world");
            return Chemical::Quantum{
                .ionisation = ionisation_of(world, atom),
                .valency = 0,
            };
        }

        Delta update(World world) {
            return iqsm::ops::cache::recompute<Chemical>(world, [&](World w, Chemical::Id id, const Chemical::Quantum& chemical) -> std::optional<Chemical::Quantum> {
                const auto& atom = iqsm::ops::particle::get<Atom>(w, id);
                const auto ionisation = ionisation_of(w, atom);
                if (chemical.ionisation == ionisation) return std::nullopt;

                auto updated = chemical;
                updated.ionisation = ionisation;
                return updated;
            });
        }
    }

    const Invariants Chemical::invariants{{{
        Invariants::anchor_component<Atom, Chemical, &Chemical_impl::construct>,
        &Chemical_impl::update,
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

