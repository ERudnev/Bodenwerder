#include <base/testing/macros.h>

#include <Atomic/model.q1.h>
#include <iQSM/dag.h>

namespace tests {
    void dagdata_define() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Model;
        auto dag = DagState::define<Element, Body, Atom>();

        EXPECT_TRUE(dag != nullptr);
        EXPECT_EQ(dag->aspects.size(), 3);

        const auto& atom = dag->aspects.at(Aspect<Atom>::typeId);
        EXPECT_TRUE(atom.depends_from.contains(Aspect<Element>::typeId));
        EXPECT_TRUE(atom.depends_from.contains(Aspect<Body>::typeId));
        EXPECT_EQ(atom.depends_from.size(), 2);

        const auto& element = dag->aspects.at(Aspect<Element>::typeId);
        EXPECT_TRUE(element.they_depend.contains(Aspect<Atom>::typeId));
        EXPECT_TRUE(element.zero != nullptr);

        const auto& body = dag->aspects.at(Aspect<Body>::typeId);
        EXPECT_TRUE(body.they_depend.contains(Aspect<Atom>::typeId));
        EXPECT_TRUE(body.zero != nullptr);

        EXPECT_TRUE(atom.zero != nullptr);
    }
}


