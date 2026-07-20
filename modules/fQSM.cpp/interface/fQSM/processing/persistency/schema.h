#pragma once

#include <initializer_list>
#include <memory>
#include <string>
#include <unordered_map>

#include <base/shared_reference.h>

#include <fQSM/meta/interface.include.h>
#include <fQSM/model/intertype/set.h>
#include <fQSM/references.h>

namespace fqsm::processing::persistency {

    // Type-erased per-aspect archive facet (lives on persistency::Graph::Node).
    struct AspectArchive {
        virtual ~AspectArchive() = default;
    };

    // Persist schema: may be a subset of world types. World Schema does not know about this.
    struct Graph {
        struct Node {
            std::string name;
            std::shared_ptr<AspectArchive> archive;
        };

        template<meta::category::Any Meta>
        bool accepts() const { return nodes.contains(TypeId<Meta>); }

        auto types() const -> model::intertype::Set {
            model::intertype::Set out;
            for (const auto& [type, _] : nodes)
                out.insert(type);
            return out;
        }

        std::unordered_map<meta::Rtid, Node, meta::Rtid::Hash> nodes;
    };

    using Schema = cref<Graph>;

    Schema merge(std::initializer_list<Schema> parts);

    template<meta::category::Any Meta>
    Schema aspect(std::shared_ptr<AspectArchive> archive);

}

namespace fqsm::processing::persistency {

    inline Schema merge(std::initializer_list<Schema> parts) {
        auto out = base::make_shared<Graph>();
        for (const auto& part : parts) {
            for (const auto& [type, node] : part->nodes)
                out->nodes.emplace(type, node);
        }
        return fqsm::freeze(out);
    }

    template<meta::category::Any Meta>
    Schema aspect(std::shared_ptr<AspectArchive> archive) {
        auto out = base::make_shared<Graph>();
        out->nodes.emplace(
            TypeId<Meta>,
            Graph::Node{
                std::string{meta::Rtid::name<Meta>()},
                std::move(archive),
            }
        );
        return fqsm::freeze(out);
    }

}
