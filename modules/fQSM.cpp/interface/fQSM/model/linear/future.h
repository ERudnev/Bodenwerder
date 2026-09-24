#pragma once

#include <fQSM/erased/future_line.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/linear/patch.h>
#include <fQSM/model/linear/state.h>
#include <fQSM/model/linear/workersInterface.h>


namespace fqsm::model::linear {

    template<category::Any Meta>
    class Future final : public State<Meta>, public WorkersInterface<Meta> {
    public:
        using Items = State<Meta>::Items;
        using Global = State<Meta>::Global;

        Future(const linear::State<Meta>& state, ref<linear::Patch<Meta>> patch)
            : WorkersInterface<Meta>(storage)
            , storage(state.line(), patch->line)
            , view(storage)
        {}

        Future(const Future&) = delete;
        Future& operator=(const Future&) = delete;

        Items& items() override { return view; }
        const Items& items() const override { return view; }
        Global& global() override { return this->get_access_global(); }
        const Global& global() const override { return this->global_of(storage.global()); }
        const erased::ReadLine& line() const override { return storage; }

    private:
        erased::FutureLine storage;
        Items view;
    };
}
