#pragma once

#include <string>

namespace Toy {

    class Model {
    public:
        Model(const std::string& file) : fileBinding(file) {} // TODO: bind to filesystem API

        ~Model() = default;
        void create();
        void loadFromFile(const std::string&);
    private:
        std::string fileBinding;
    };
}