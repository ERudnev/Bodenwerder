
#include <Atomic/varph.q1.h>
#include <cstdlib>
#include <unordered_map>
#include <unordered_set>
#include <glm/geometric.hpp>
#include <iQSM/_all.include.h>

// Spark:
namespace Q1CORE::Example::Varph {
    const Invariants Spark::invariants{{{
    }}};
}

// Inertia:
namespace Q1CORE::Example::Varph {
    const Invariants Inertia::invariants{{{
        invariant::anchor_attribute<Spark, Inertia>,
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
        invariant::anchor_attribute<Spark, Charge>,
    }}};
}

// Strong:
namespace Q1CORE::Example::Varph {
    const Invariants Strong::invariants{{{
        invariant::anchor_attribute<Spark, Strong>,
    }}};
}

// TextDescription:
namespace Q1CORE::Example::Varph {
    const Invariants TextDescription::invariants{{{
    }}};
}

// Electron:
namespace Q1CORE::Example::Varph {

    namespace Electron_impl {
        auto existence_rule(World, Charge::Id, const Charge::Quantum& charge)->bool {
            return charge.value == -1;
        }
    }
    const Invariants Electron::invariants{{{
        invariant::existence<Electron, Charge, &Electron_impl::existence_rule>,
        invariant::anchor_attribute<Charge, Electron>,
    }}};
}

// Nucleon:
namespace Q1CORE::Example::Varph {
    namespace Nucleon_impl {
        auto symbol_of(integer isospin2) -> string {
            if (isospin2 == +1) return "P";
            if (isospin2 == -1) return "N";
            return "?";
        }

        auto existence_rule(World, Strong::Id, const Strong::Quantum& q) -> bool {
            return std::abs(q.isospin2) == 1;
        }

        auto update_cache(World world, Nucleon::Id id, const Nucleon::Quantum& original) -> optional<Nucleon::Quantum> {
            const auto& strong = iqsm::ops::particle::get<Strong>(world, id);
            auto updated = original;
            updated.legend = symbol_of(strong.isospin2);

            if (Facet<Nucleon>::equal(original, updated)) return {};
            return updated;
        }
    }

    const Invariants Nucleon::invariants{{{
        invariant::existence<Nucleon, Strong, &Nucleon_impl::existence_rule>,
        invariant::anchor_attribute<Strong, Nucleon>,
        invariant::for_each_item<Nucleon, &Nucleon_impl::update_cache>,
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

        auto z_of(World world, const std::vector<Nucleon::Id>& nucleons) -> integer {
            integer z = 0;
            for (const auto& id : nucleons) {
                const auto q = Charge::Operations::value(world, id);
                if (q > 0) z += q;
            }
            return z;
        }

        void existence(iqsm::repo::Commit commit) {
            const auto world = commit.initial;
            const auto nucleons = world->field<Nucleon>();
            if (nucleons->container.empty()) return;

            const auto atoms = world->field<Atom>();

            std::unordered_set<Nucleon::Id> assigned;
            assigned.reserve(nucleons->container.size());

            for (const auto& kv : atoms->container) {
                const auto& atom_item = kv.second;
                for (const auto& nid : atom_item->core) {
                    if (nucleons->container.contains(nid)) assigned.insert(nid);
                }
            }

            std::vector<Nucleon::Id> homeless;
            homeless.reserve(nucleons->container.size());
            for (const auto& kv : nucleons->container) {
                const auto& nid = kv.first;
                if (not assigned.contains(nid)) homeless.push_back(nid);
            }
            if (homeless.empty()) return;

            struct Node {
                Spark::minkpt pos;
                float reach;
            };

            std::unordered_map<Nucleon::Id, Node> node;
            node.reserve(homeless.size());
            for (const auto& nid : homeless) {
                const auto& spark = iqsm::ops::particle::get<Spark>(world, nid);
                node.emplace(nid, Node{spark.position, spark.locality});
            }

            const auto linked = [&](Nucleon::Id a, Nucleon::Id b) -> bool {
                const auto& na = node.at(a);
                const auto& nb = node.at(b);
                const auto d = (vec3(na.pos) - vec3(nb.pos)); // ignore time component
                const float dist2 = glm::dot(d, d);
                const float reach = 0.5f * (na.reach + nb.reach);
                return dist2 <= (reach * reach);
            };

            constexpr std::size_t min_core = 2;

            std::unordered_set<Nucleon::Id> visited;
            visited.reserve(homeless.size());

            auto tx = iqsm::repo::Sequence{world};

            for (const auto& start : homeless) {
                if (not visited.insert(start).second) continue;

                std::vector<Nucleon::Id> group;
                group.push_back(start);

                for (std::size_t i = 0; i < group.size(); ++i) {
                    const auto cur = group[i];
                    for (const auto& other : homeless) {
                        if (visited.contains(other)) continue;
                        if (not linked(cur, other)) continue;
                        visited.insert(other);
                        group.push_back(other);
                    }
                }

                if (group.size() < min_core) continue;

                Atom::Quantum q{};
                q.core = std::move(group);
                q.legend = symbol_of(z_of(world, q.core));

                iqsm::ops::particle::create<Atom>(tx, std::move(q));
            }

            const auto delta = tx.push();
            if (not delta->empty()) {
                commit.push(delta);
            }
        }

        auto update_cache(World world, Atom::Id id, const Atom::Quantum& original) -> optional<Atom::Quantum> {
            auto updated = original;
            updated.legend = symbol_of(z_of(world, original.core));
            if (Facet<Atom>::equal(original, updated)) return {};
            return updated;
        }
    }

    auto Atom::Operations::tension(World world, Atom::Id atom) -> distance {
        const auto q = iqsm::ops::particle::get<Atom>(world, atom);

        distance out = 0.0f;
        for (std::size_t i = 0; i < q.core.size(); ++i) {
            const auto& pi = iqsm::ops::particle::get<Spark>(world, q.core[i]).position;
            for (std::size_t j = i + 1; j < q.core.size(); ++j) {
                const auto& pj = iqsm::ops::particle::get<Spark>(world, q.core[j]).position;
                const auto d = (pi - pj);
                out += glm::dot(d, d);
            }
        }
        return out;
    }

    const Invariants Atom::invariants{{{
        &Atom_impl::existence,
        invariant::anchor_any<Nucleon, Atom, &Quantum::core>,
        invariant::for_each_item<Atom, &Atom_impl::update_cache>,
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

        auto construct(World world, const Atom::Quantum& atom)->Chemical::Quantum {
            return Chemical::Quantum{
                .ionisation = ionisation_of(world, atom),
                .valency = 0,
            };
        }

        auto update_cache(World w, Chemical::Id id, const Chemical::Quantum& original) -> optional<Chemical::Quantum> {
            const auto& atom = iqsm::ops::particle::get<Atom>(w, id);
            auto updated = original;
            updated.ionisation = ionisation_of(w, atom);
            if (Facet<Chemical>::equal(original, updated)) return {};
            return updated;
        }
    }

    const Invariants Chemical::invariants{{{
        invariant::isomorphic<Atom, Chemical, &Chemical_impl::construct>,
        invariant::for_each_item<Chemical, &Chemical_impl::update_cache>,
    }}};
}


// Capture:
namespace Q1CORE::Example::Varph {
    const Invariants Capture::invariants{{{
        invariant::anchor<Atom, Capture, &Quantum::atom>,
        invariant::anchor<Electron, Capture, &Quantum::electron>,
    }}};
}

// Modecule:
namespace Q1CORE::Example::Varph {
    const Invariants Modecule::invariants{{{
        invariant::anchor_any<Atom, Modecule, &Quantum::atoms>,
    }}};
}

// Binding:
namespace Q1CORE::Example::Varph {
    const Invariants Binding::invariants{{{
        invariant::anchor_all<Atom, Binding, &Quantum::bound>,
    }}};
}

