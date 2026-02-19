#include "model.h"

#include <iQSM/logger.h>
#include <iQSM/field.h>
#include <memory>

// domain:
#include <Atomic/varph.q1.h>

namespace Toy {
    void Model::create() {
        using namespace iqsm::logger;
        using namespace Q1CORE::Example::Varph;
        using namespace iqsm;

        // runtime blocker (temp)
        fatal("atomic model is empty, blocking application run..."); // will add "now()" to the message in "fatal"
    }

    void Model::loadFromFile(const std::string& file) {
        fileBinding = file;
    }
}