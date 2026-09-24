#pragma once

#include <utility>

#include <fQSM/erased/overlay.h>
#include <fQSM/erased/patch_line.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/meta/rtid.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/linear/patch.h>
#include <fQSM/model/linear/state.h>
#include <fQSM/model/linear/workersInterface.h>
#include <fQSM/utility/messages.h>


namespace fqsm::model::linear {

    template<category::Any Meta>
    class Future final : public State<Meta>, public WorkersInterface<Meta> {
    public:
        using Items = State<Meta>::Items;
        using Global = State<Meta>::Global;

        Future(const linear::State<Meta>& state, ref<linear::Patch<Meta>> patch)
            : origin(state)
            , changes(patch->line)
            , overlay(state.line(), patch->line)
            , view(overlay)
        {}

        Future(const Future&) = delete;
        Future& operator=(const Future&) = delete;

        Items& items() override { return view; }
        const Items& items() const override { return view; }
        Global& global() override { return get_access_global(); }
        const Global& global() const override { return *static_cast<const Global*>(overlay.global()); }
        const erased::ReadLine& line() const override { return overlay; }

         // WorkersInterface
         void put_modification(Id<Meta>, Quantum<Meta>) override;
         void put_deletion(Id<Meta>) override;
         void put_add(Id<Meta>, Quantum<Meta>) override;
         void put_global(GlobalValue<Meta>) override;
         Quantum<Meta>& get_modification_access(Id<Meta>) override;
         GlobalValue<Meta>& get_access_global() override;

    private:
        const State<Meta>& origin;
        erased::PatchLine& changes;
        erased::Overlay overlay;
        Items view;
    };
}

namespace fqsm::model::linear {

    template<category::Any Meta>
    void Future<Meta>::put_modification(Id<Meta> id, Quantum<Meta> value) {
        changes.modify_move(id.raw(), &value);
    }

    template<category::Any Meta>
    void Future<Meta>::put_deletion(Id<Meta> id) {
        if (const auto patched = changes.mention(id.raw()); patched.found) {
            changes.del(id.raw(), patched.value);
            return;
        }
        if (const void* current = origin.line().find(id.raw()))
            changes.del(id.raw(), current);
    }

    template<category::Any Meta>
    void Future<Meta>::put_add(Id<Meta> id, Quantum<Meta> value) {
        changes.modify_move(id.raw(), &value);
    }

    template<category::Any Meta>
    void Future<Meta>::put_global(GlobalValue<Meta> value) {
        changes.set_global(&value);
    }

    template<category::Any Meta>
    Quantum<Meta>& Future<Meta>
    ::get_modification_access(Id<Meta> id) {
        if (void* patched = changes.find_mutable(id.raw()))
            return *static_cast<Quantum<Meta>*>(patched);
        const void* current = origin.line().find(id.raw());
        if (not current)
            utility::messages::throw_not_present("cannot modify", Rtid::name<Meta>(), id.raw());
        return *static_cast<Quantum<Meta>*>(changes.touch(id.raw(), current));
    }

    template<category::Any Meta>
    GlobalValue<Meta>& Future<Meta>
    ::get_access_global() {
        return *static_cast<GlobalValue<Meta>*>(changes.touch_global(origin.line().global()));
    }
}
