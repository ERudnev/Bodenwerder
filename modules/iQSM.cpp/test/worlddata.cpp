#include <base/testing/macros.h>

#include <Atomic/model.q1.h>
#include <iQSM/dag.h>
#include <iQSM/world.h>
#include <stdexcept>

namespace tests {
    void worlddata_construct() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Model;

        auto basis = DagState::define<Element, Molecule, Atom, Position>();
        EXPECT_TRUE(basis != nullptr);

        World world = std::make_shared<const WorldState>(basis);
        EXPECT_TRUE(world != nullptr);
        EXPECT_TRUE(world->basis == basis);

        auto element = world->field<Element>();
        EXPECT_TRUE(element != nullptr);
        EXPECT_EQ(element->container.size(), size_t{0});
    }

    void worlddata_unclosed_basis_returns_null() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Model;

        // Atom depends on Element/Molecule, but we intentionally don't include them.
        bool threw = false;
        std::string msg;
        try {
            (void)DagState::define<Atom>();
        } catch (const std::runtime_error& e) {
            threw = true;
            msg = e.what();
        }
        EXPECT_TRUE(threw);
        EXPECT_TRUE(msg.find("DagState is not closed") != std::string::npos);
        EXPECT_TRUE(msg.find("missing dependency") != std::string::npos);
        EXPECT_TRUE(msg.find("required by") != std::string::npos);
        EXPECT_TRUE(msg.find("Atom") != std::string::npos);
    }
}


