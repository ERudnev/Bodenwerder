#pragma once

#include <base/cannonball/table.h>
#include <base/shared_reference.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/linear/state.h>

#include <type_traits>

namespace fqsm::model::linear {

    template<category::Any Meta>
    class Reality : public State<Meta> {
    public:
        using Global = State<Meta>::Global;
        using Items = State<Meta>::Items;

        Reality()=default;
        Reality(const Global& initial) : line{}, globalValue(initial) {}
        //Reality(const state::Erased& initial) { _INCOMPLETE_; }

        Items& items() override { return line; }
        const Items& items() const override { return line; }
        Global& global() override { return globalValue; }
        const Global& global() const override { return globalValue; }
        static ref<state::Erased> create() requires std::is_default_constructible_v<Global> { return base::make_shared<Reality<Meta>>(); }
        static ref<state::Erased> createWith(const Global& initial) { return base::make_shared<Reality<Meta>>(initial); }
        static ref<state::Erased> from(const State<Meta>& slice) {
            auto out = base::make_shared<Reality<Meta>>(slice.global());
            for (const auto entry : slice.items()) {
                out->items().insert(entry.id, entry.value);
            }
            return out;
        }
    private:
        base::cannonball::Table<Id<Meta>, Quantum<Meta>> line;
        GlobalValue<Meta> globalValue;
    };
}
