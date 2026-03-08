#pragma once

#include <iQSM/_forwards.h>
#include <iQSM/operations/schema.h>
#include <iQSM/operations/world.h>
#include <string>

namespace Toy {

    class Model {
    public:
        Model(const std::string& file)
            : fileBinding(file)
            , world(iqsm::ops::world::create(iqsm::ops::schema::assemble<>()))
        {} // TODO: bind to filesystem API

        ~Model() = default;
        void create();
        void loadFromFile();

        iqsm::World current() const { return world; }
    private:
        const std::string fileBinding;

        // model itself:
        iqsm::World world;

    };
}