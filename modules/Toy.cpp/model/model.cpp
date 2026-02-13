#include "model.h"

#include <iQSM/logger.h>
#include <iQSM/field.h>
#include <memory>

// domain:
#include <Atomic/model.q1.h>

namespace Toy {
    void Model::create() {
        using namespace iqsm::logger;
        using namespace Q1CORE::Example::Model;
        using namespace iqsm;

        // temp code to buid compileable iQSM structure types:
        iqsm::FieldObject<Element> field;
        field.container = field.container
            .insert(Element::Id::generate_random(), Aspect<Element>::create({"H", seconds{0}, integer{1}}))
            .insert(Element::Id::generate_random(), Aspect<Element>::create({"He", seconds{0}, integer{0}}))
            .insert(Element::Id::generate_random(), Aspect<Element>::create({"Li", seconds{0}, integer{1}}))
            .insert(Element::Id::generate_random(), Aspect<Element>::create({"Be", seconds{0}, integer{2}}));

        message("Field: {}", field);


        // runtime blocker (temp)
        fatal("atomic model is empty, blocking application run..."); // will add "now()" to the message in "fatal"
    }

    void Model::loadFromFile(const std::string& file) {
        fileBinding = file;
    }
}