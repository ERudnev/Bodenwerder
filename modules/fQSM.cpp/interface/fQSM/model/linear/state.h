#pragma once

#include <cstddef>

#include <fQSM/erased/line.h>
#include <fQSM/model/_forwards.h>
#include <fQSM/meta/interface.include.h>
#include <fQSM/model/linear/items.h>

namespace fqsm::model::linear {

    template<category::Any Meta>
    class State : public state::Erased {
    public:
        using Items = linear::Items<Meta>;
        using Global = GlobalValue<Meta>;

        virtual Items& items() = 0;
        virtual const Items& items() const = 0;
        virtual Global& global() = 0;
        virtual const Global& global() const = 0;
        virtual const erased::ReadLine& line() const = 0;

        std::size_t quanta() const override { return line().size(); }
    };
}
