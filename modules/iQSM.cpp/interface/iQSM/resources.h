#pragma once

#include <concepts>
#include <format>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include <iQSM/aspects.h>
#include <iQSM/world.h>

namespace iqsm::resources {

    template<typename HandlerType>
    concept Handler = requires(const HandlerType& handler) {
        { handler.name } -> std::same_as<const std::string&>;
    };

    struct LayerAbstract {
        using RuntimeTypeId = ::iqsm::internals::Types::RuntimeId;
        virtual ~LayerAbstract() = default;

        // Handle type for internal plumbing (stored in Manager containers).
        using Ref = ref<LayerAbstract>;

        virtual void sync(World world) = 0;
        virtual std::string report() const = 0;
    };

    template<AspectResource Meta, Handler HandlerType>
    struct LayerData final : LayerAbstract {
        using ResourceId = typename Meta::Id;
        using Passport = typename Meta::Passport;
        std::map<ResourceId, std::unique_ptr<HandlerType>> handlers;

        void sync(World world) override;
        std::string report() const override;

    private:
        void load(const ResourceId& id, const Passport& passport);
        void sync(const ResourceId& id, const Passport& passport);
    };
}

// details
namespace iqsm::detail::resources {
    template<AspectResource Meta>
    struct handler_of;

    template<AspectResource Meta>
    using handler_t = typename handler_of<Meta>::type;
}


// implementation
namespace iqsm::resources {
    // Handle
    template<AspectResource Meta>
    using Layer = ref<LayerData<Meta, iqsm::detail::resources::handler_t<Meta>>>;

    struct ManagerData {
        using TypeId = iqsm::internals::Types::RuntimeId;

        std::map<TypeId, LayerAbstract::Ref> layers;

        template<AspectResource Meta>
        void register_layer();

        template<AspectResource Meta>
        Layer<Meta> layer();

        void sync(World world);
        std::string report() const;
    };

    // Handle
    using Manager = ref<ManagerData>;

    template<AspectResource Meta, Handler HandlerType>
    void LayerData<Meta, HandlerType>::load(const ResourceId& id, const Passport& passport) {
        if (handlers.contains(id)) {
            throw std::runtime_error("resources::LayerData::load(): already loaded");
        }
        handlers[id] = std::make_unique<HandlerType>(passport);
    }

    template<AspectResource Meta, Handler HandlerType>
    void LayerData<Meta, HandlerType>::sync(const ResourceId& id, const Passport& passport) {
        if (handlers.contains(id)) return;
        load(id, passport);
    }

    template<AspectResource Meta, Handler HandlerType>
    std::string LayerData<Meta, HandlerType>::report() const {
        if (handlers.empty()) return std::format("{} [empty]", Facet<Meta>::typeName);

        std::string out;
        out.push_back('[');
        for (const auto& [id, handler] : handlers) {
            out.append(std::format("{}: {}", id, handler->name));
            out.append(", ");
        }
        out.resize(out.size() - 2);
        out.push_back(']');
        return std::format("{} {}", Facet<Meta>::typeName, out);
    }

    template<AspectResource Meta, Handler HandlerType>
    void LayerData<Meta, HandlerType>::sync(World world) {
        required(world, "resources::LayerData::sync(world): world");
        const auto field = world->field<Meta>();
        static_assert(requires(typename Facet<Meta>::Quantum q) { q.passport; },
            "resources::LayerData::sync(world): Resource instance must have `Quantum::passport`");
        for (const auto& [id, item] : field->container) {
            required(item, "resources::LayerData::sync(world): instance");
            sync(id, item->passport);
        }
    }

    template<AspectResource Meta>
    void ManagerData::register_layer() {
        const auto tid = Facet<Meta>::typeId;
        if (layers.contains(tid)) return;
        layers.emplace(tid, std::make_shared<LayerData<Meta, iqsm::detail::resources::handler_t<Meta>>>());
    }

    template<AspectResource Meta>
    Layer<Meta> ManagerData::layer() {
        const auto tid = Facet<Meta>::typeId;

        if (auto it = layers.find(tid); it != layers.end()) {
            auto typed = std::dynamic_pointer_cast<LayerData<Meta, iqsm::detail::resources::handler_t<Meta>>>(it->second);
            if (!typed) { throw std::runtime_error("resources::ManagerData::layer(): layer type mismatch"); }
            return typed;
        }

        register_layer<Meta>();
        return layer<Meta>();
    }

    inline void ManagerData::sync(World world) {
        required(world, "resources::ManagerData::sync(): world");
        for (auto& [_, layer] : layers) {
            required(layer, "resources::ManagerData::sync(): layer");
            layer->sync(world);
        }
    }

    inline std::string ManagerData::report() const {
        if (layers.empty()) return "[empty]";

        std::string out;
        out.append("[\n");
        for (const auto& [_, layer] : layers) {
            out.append("    ");
            out.append(layer->report());
            out.push_back('\n');
        }
        out.push_back(']');
        return out;
    }
}