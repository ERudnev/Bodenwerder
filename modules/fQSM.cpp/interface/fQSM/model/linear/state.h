#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

#include <fQSM/erased/future_line.h>
#include <fQSM/erased/line.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/linear/items.h>
#include <fQSM/model/linear/workersInterface.h>

namespace fqsm::model::linear {

    // Typed view of one aspect line of a complex state. writable is set over a reality line,
    // future over a future line; a plain read view has neither.
    template<category::Any Meta>
    class State : public state::Erased {
    public:
        using Items = linear::Items<Meta>;
        using Global = GlobalValue<Meta>;

        State(const erased::ReadLine& reader, erased::Line* writable, erased::FutureLine* future)
            : reader(&reader)
            , writable(writable)
            , future(future)
            , view(reader, writable)
        {}

        State(const State&) = delete;
        State& operator=(const State&) = delete;

        void rebind(const erased::ReadLine& line, erased::Line* writableLine, erased::FutureLine* futureLine) override {
            reader = &line;
            writable = writableLine;
            future = futureLine;
            view.rebind(line, writableLine);
        }

        Items& items() { return view; }
        const Items& items() const { return view; }
        const erased::ReadLine& line() const { return *reader; }

        const Global& global() const { return global_of(reader->global()); }
        Global& global() {
            if (writable) return global_of(writable->global_mutable());
            if (future) return global_of(future->get_access_global());
            throw std::logic_error("fQSM: global of a read-only view");
        }

    private:
        // An assembled global is absent until Always::assemble ran.
        static Global& global_of(const void* value) {
            if (not value)
                throw std::logic_error(std::string("fQSM: global is not assembled yet: ") + std::string(Rtid::name<Meta>()));
            return *static_cast<Global*>(const_cast<void*>(value));
        }

        const erased::ReadLine* reader;
        erased::Line* writable;
        erased::FutureLine* future;
        Items view;
    };

    // The object a complex state caches per slot: read view plus typed writes (future lines only).
    template<category::Any Meta>
    class View final : public State<Meta>, public WorkersInterface<Meta> {
    public:
        View(const erased::ReadLine& reader, erased::Line* writable, erased::FutureLine* future)
            : State<Meta>(reader, writable, future)
            , WorkersInterface<Meta>(future)
        {}

        void rebind(const erased::ReadLine& line, erased::Line* writableLine, erased::FutureLine* futureLine) override {
            State<Meta>::rebind(line, writableLine, futureLine);
            this->retarget(futureLine);
        }
    };
}
