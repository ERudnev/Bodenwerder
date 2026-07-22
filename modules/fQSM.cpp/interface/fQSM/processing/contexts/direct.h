#pragma once

#include <functional>
#include <memory>
#include <fQSM/model/_forwards.h>
#include <fQSM/model/complex/reality.h>
#include <fQSM/processing/_forwards.h>

// TODO:
// this part of library needs major restructurisation.
namespace fqsm::processing::context {

    // direct context is "complex" (heteroneous full scope of all Aspects, while Breach is narrow
    struct Direct final {
        using Ptr = std::shared_ptr<Direct>;
        using Notification = std::function<void(Rtid::Set affected)>;
        //const model::complex::State& reference;

        model::complex::Reality& reality;
        Notification callback;
        Rtid::Set dirty;

        ~Direct() { finish(); }

        void finish() {
            if (callback)
                callback(std::move(dirty));
            callback = nullptr;
        }

    };

}

namespace fqsm::processing {

    template<category::Any Meta>
    struct Breach {
        using Context = context::Direct;
        using Container = base::cannonball::Table<Id<Meta>, Quantum<Meta>>;
        using Global = GlobalValue<Meta>;

        Breach(Context::Ptr parent)
            : items(static_cast<Container&>(parent->reality.aspect<Meta>().items()))
            , context(parent)
        {
            context->dirty.insert(Rtid::of<Meta>());
        }

        Container& items;

        auto global() -> Global& {
            return context->reality.aspect<Meta>().global();
        }

        operator Reading() const { return View(context->reality); }

    private:
        Context::Ptr context;
    };

}