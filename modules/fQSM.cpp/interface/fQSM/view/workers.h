#pragma once

#include <stdexcept>
#include <utility>

#include <fQSM/erased/future_line.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/utility/messages.h>
#include <fQSM/view/items.h>

namespace fqsm::view {

    // Typed write access to one future line (null for views that cannot write).
    template<category::Any Meta>
    class WorkersInterface {
    public:
        explicit WorkersInterface(erased::FutureLine* line) : target(line) {}
        void retarget(erased::FutureLine* line) { target = line; }

        void put_modification(Id<Meta> id, Quantum<Meta> value) { target->put_modification(id.raw(), &value); }
        void put_deletion(Id<Meta> id) { target->put_deletion(id.raw()); }
        void put_add(Id<Meta> id, Quantum<Meta> value) { target->put_add(id.raw(), &value); }
        void put_global(GlobalValue<Meta> value) { target->put_global(&value); }

        Quantum<Meta>& get_modification_access(Id<Meta> id) {
            void* found = target->get_modification_access(id.raw());
            if (not found)
                utility::messages::throw_not_present("cannot modify", Rtid::name<Meta>(), id.raw());
            return *static_cast<Quantum<Meta>*>(found);
        }

        GlobalValue<Meta>& get_access_global() {
            void* found = target->get_access_global();
            if (not found)
                throw std::logic_error(std::string("fQSM: global is not assembled yet: ") + std::string(Rtid::name<Meta>()));
            return *static_cast<GlobalValue<Meta>*>(found);
        }

    private:
        erased::FutureLine* target;
    };

    // The object a complex state caches per slot: the aspect view plus typed writes (future lines only).
    template<category::Any Meta>
    class Slot final : public Aspect<Meta>, public WorkersInterface<Meta> {
    public:
        Slot(const erased::ReadLine& reader, erased::Line* writable, erased::FutureLine* future)
            : Aspect<Meta>(reader, writable, future)
            , WorkersInterface<Meta>(future)
        {}

        void rebind(const erased::ReadLine& line, erased::Line* writableLine, erased::FutureLine* futureLine) override {
            Aspect<Meta>::rebind(line, writableLine, futureLine);
            this->retarget(futureLine);
        }
    };
}
