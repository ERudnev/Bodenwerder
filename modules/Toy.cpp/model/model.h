#pragma once

#include <iQSM/_forwards.h>
#include <string>

namespace Toy {

    class Model {
    public:
        Model(const std::string& file) : fileBinding(file) {} // TODO: bind to filesystem API

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