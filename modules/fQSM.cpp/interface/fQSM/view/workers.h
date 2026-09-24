#pragma once

#include <stdexcept>
#include <utility>

#include <fQSM/erased/future_line.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/utility/messages.h>
#include <fQSM/view/items.h>

namespace fqsm::view {

    // Typed writes into one future line (the aspect view of a session or of a normalization wave).
    template<category::Any Meta>
    class WorkersInterface : public Aspect<Meta> {
    public:
        explicit WorkersInterface(const Lines& lines) : Aspect<Meta>(lines) {}
        WorkersInterface(const erased::ReadLine& reader, erased::Line* writable, erased::FutureLine* future)
            : Aspect<Meta>(Lines{&reader, writable, future}) {}

        void put_modification(Id<Meta> id, Quantum<Meta> value) { this->future->put_modification(id.raw(), &value); }
        void put_deletion(Id<Meta> id) { this->future->put_deletion(id.raw()); }
        void put_add(Id<Meta> id, Quantum<Meta> value) { this->future->put_add(id.raw(), &value); }
        void put_global(GlobalValue<Meta> value) { this->future->put_global(&value); }

        Quantum<Meta>& get_modification_access(Id<Meta> id) {
            void* found = this->future->get_modification_access(id.raw());
            if (not found)
                utility::messages::throw_not_present("cannot modify", Rtid::name<Meta>(), id.raw());
            return *static_cast<Quantum<Meta>*>(found);
        }

        GlobalValue<Meta>& get_access_global() {
            void* found = this->future->get_access_global();
            if (not found)
                throw std::logic_error(std::string("fQSM: global is not assembled yet: ") + std::string(Rtid::name<Meta>()));
            return *static_cast<GlobalValue<Meta>*>(found);
        }
    };

    // The typed view a complex state keeps per slot: items, global and writes over one Lines.
    template<category::Any Meta>
    using Slot = WorkersInterface<Meta>;
}
