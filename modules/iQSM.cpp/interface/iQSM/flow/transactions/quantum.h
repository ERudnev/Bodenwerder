#pragma once

#include <format>
#include <stdexcept>
#include <utility>

#include <iQSM/internals/delta_builders.h>
#include <iQSM/meta/concepts.h>
#include <iQSM/meta/facade.h>
#include <iQSM/repository/transaction.h>

namespace iqsm::flow {

    template<meta::Particle Meta>
    class Quantum final : protected Transaction {
    public:
        using Id = iqsm::Id<Meta>;
        using QuantumData = iqsm::Quantum<Meta>;
        using Node = iqsm::Node<Meta>;

        explicit Quantum(Writing writing, Id id)
            : Transaction(std::move(writing))
            , id(id)
            , original(required_node(head.state, id))
            , value(*original)
        {}

        ~Quantum() override { on_finish(); }

        Quantum(const Quantum&) = delete;
        Quantum& operator=(const Quantum&) = delete;
        Quantum(Quantum&&) = delete;
        Quantum& operator=(Quantum&&) = delete;

        QuantumData* operator->() { dirty = true; return &value; }
        QuantumData& operator*() { dirty = true; return value; }

    private:
        void on_finish() override {
            if (unwinding()) return;
            if (not head.upstream) return;
            if (not dirty) {
                disconnect();
                return;
            }

            head.upstream({{}, internals::delta::make_atomic<Meta>(
                id,
                original,
                base::make_shared<const QuantumData>(std::move(value)))});
            disconnect();
        }

        static Node required_node(Reading world, const Id& id) {
            const auto field = world->field<Meta>();
            if (not field->container.contains(id)) { throw std::runtime_error(std::format("modifier: missing entity: {}", id)); }
            const auto node = field->container.at(id);
            return node;
        }

        Id id;
        Node original;
        QuantumData value;
        bool dirty = false;

    protected:
        void absorb(Channel::Result) override {}
    };
}
