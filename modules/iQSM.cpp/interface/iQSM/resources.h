#pragma once

#include <concepts>
#include <format>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>

#include <iQSM/meta.h>
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

    template<meta::Resource Meta, Handler HandlerType>
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
    template<meta::Resource Meta>
    struct handler_of;

    template<meta::Resource Meta>
    using handler_t = typename handler_of<Meta>::type;
}


// implementation
namespace iqsm::resources {
    // Handle
    template<meta::Resource Meta>
    using Layer = ref<LayerData<Meta, iqsm::detail::resources::handler_t<Meta>>>;

    struct ManagerData {
        using TypeId = iqsm::internals::Types::RuntimeId;

        std::map<TypeId, LayerAbstract::Ref> layers;

        template<meta::Resource Meta>
        void register_layer();

        template<meta::Resource Meta>
        Layer<Meta> layer();

        void sync(World world);
        std::string report() const;
    };

    // Handle
    using Manager = ref<ManagerData>;

    template<meta::Resource Meta, Handler HandlerType>
    void LayerData<Meta, HandlerType>::load(const ResourceId& id, const Passport& passport) {
        if (handlers.contains(id)) {
            throw std::runtime_error("resources::LayerData::load(): already loaded");
        }
        handlers[id] = std::make_unique<HandlerType>(passport);
    }

    template<meta::Resource Meta, Handler HandlerType>
    void LayerData<Meta, HandlerType>::sync(const ResourceId& id, const Passport& passport) {
        if (handlers.contains(id)) return;
        load(id, passport);
    }

    template<meta::Resource Meta, Handler HandlerType>
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

    template<meta::Resource Meta, Handler HandlerType>
    void LayerData<Meta, HandlerType>::sync(World world) {
        const auto field = world->field<Meta>();
        static_assert(requires(typename Facet<Meta>::Quantum q) { q.passport; },
            "resources::LayerData::sync(world): Resource instance must have `Quantum::passport`");
        for (const auto& [id, item] : field->container) {
            sync(id, item->passport);
        }
    }

    template<meta::Resource Meta>
    void ManagerData::register_layer() {
        const auto tid = Facet<Meta>::typeId;
        if (layers.contains(tid)) return;
        layers.emplace(tid, base::make_shared<LayerData<Meta, iqsm::detail::resources::handler_t<Meta>>>());
    }

    template<meta::Resource Meta>
    Layer<Meta> ManagerData::layer() {
        const auto tid = Facet<Meta>::typeId;

        if (auto it = layers.find(tid); it != layers.end()) {
            return base::shared_ref_cast<LayerData<Meta, iqsm::detail::resources::handler_t<Meta>>>(it->second);
        }

        register_layer<Meta>();
        return layer<Meta>();
    }

    inline void ManagerData::sync(World world) {
        for (auto& [_, layer] : layers) {
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