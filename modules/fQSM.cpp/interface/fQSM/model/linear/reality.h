#pragma once

#include <type_traits>

#include <base/shared_reference.h>
#include <fQSM/erased/descriptor.h>
#include <fQSM/erased/line.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/linear/state.h>

namespace fqsm::model::linear {

    template<category::Any Meta>
    class Reality final : public State<Meta> {
    public:
        using Global = State<Meta>::Global;
        using Items = State<Meta>::Items;

        Reality() requires std::is_default_constructible_v<Global>
            : storage(erased::ops_of<Quantum<Meta>>(), erased::ops_of<Global>())
            , view(storage)
        {}

        explicit Reality(const Global& initial)
            : storage(erased::ops_of<Quantum<Meta>>(), erased::ops_of<Global>())
            , view(storage)
        {
            storage.set_global(&initial);
        }

        Reality(const Reality&) = delete;
        Reality& operator=(const Reality&) = delete;

        Items& items() override { return view; }
        const Items& items() const override { return view; }
        Global& global() override { return this->global_of(storage.global_mutable()); }
        const Global& global() const override { return this->global_of(storage.global()); }
        const erased::ReadLine& line() const override { return storage; }
        erased::Line& writableLine() { return storage; }

        static ref<state::Erased> create() requires std::is_default_constructible_v<Global> { return base::make_shared<Reality<Meta>>(); }
        static ref<state::Erased> createWith(const Global& initial) { return base::make_shared<Reality<Meta>>(initial); }
        static ref<state::Erased> from(const State<Meta>& slice) {
            auto out = base::make_shared<Reality<Meta>>(slice.global());
            out->storage.clone(slice.line());
            return out;
        }

    private:
        erased::Line storage;
        Items view;
    };
}
