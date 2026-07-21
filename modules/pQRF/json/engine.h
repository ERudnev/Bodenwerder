#pragma once

#include <fQSM/api/interface.h>
#include <fQSM/aspect/persistency.h>
#include <fQSM/meta/alias.h>
#include <fQSM/meta/categories.h>
#include <fQSM/utility/bad_value.h>
#include <pQRF/json/document.h>
#include <pQRF/json/leaf.h>
#include <pQRF/json/storage.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace fqsm::processing::persistency::json::detail {

    using namespace fqsm::api;
    using fqsm::aspect::Collection;
    using fqsm::aspect::Field;

    // File sections (labels only; collections live inlined inside one/all rows).
    constexpr std::string_view section_one = "one";
    constexpr std::string_view section_one_collections = "one.collections";
    constexpr std::string_view section_all = "all";
    constexpr std::string_view section_all_collections = "all.collections";

    template<typename Meta>
    struct AspectNameDesc {
        std::string_view aspectName{};

        void aspect(std::string_view name) { aspectName = name; }
        void one(auto&&) {}
        void all(auto&&) {}
    };

    template<typename Meta>
    auto aspect_name_of() -> std::string {
        AspectNameDesc<Meta> desc{};
        Meta::describe(desc);
        return std::string{desc.aspectName};
    }

    template<typename Meta>
    struct WriteOneRowDesc {
        Value& row;
        const typename Meta::Quantum& quantum;

        void aspect(std::string_view) {}

        template<auto... Members>
        void one(Field<Members...> slot) {
            row.array.push_back(leaf::write(slot.get(quantum)));
        }

        template<auto... Members>
        void one(Collection<Members...> slot) {
            Value nested = Value::array_value();
            for (const auto& element : slot.get(quantum))
                nested.array.push_back(leaf::write(element));
            row.array.push_back(std::move(nested));
        }

        template<auto... Members>
        void all(Field<Members...>) {}

        template<auto... Members>
        void all(Collection<Members...>) {}
    };

    template<typename Meta>
    struct ReadOneRowDesc {
        const Value& row;
        typename Meta::Quantum& quantum;
        std::size_t index = 1; // skip id at [0]

        void aspect(std::string_view) {}

        template<auto... Members>
        void one(Field<Members...> slot) {
            if (index >= row.array.size())
                throw std::runtime_error("json one-row: missing field");
            leaf::read(row.array[index++], slot.get(quantum));
        }

        template<auto... Members>
        void one(Collection<Members...> slot) {
            if (index >= row.array.size())
                throw std::runtime_error("json one-row: missing collection");
            const auto& nested = row.array[index++];
            if (!nested.is_array())
                throw std::runtime_error("json one-row: collection must be array");
            using Elem = typename std::decay_t<decltype(slot.get(quantum))>::value_type;
            auto& container = slot.get(quantum);
            container.clear();
            for (const auto& element : nested.array) {
                Elem value = fqsm::utility::BadValue{};
                leaf::read(element, value);
                container.push_back(std::move(value));
            }
        }

        template<auto... Members>
        void all(Field<Members...>) {}

        template<auto... Members>
        void all(Collection<Members...>) {}
    };

    template<typename Meta>
    struct WriteAllRowDesc {
        Value& row;
        const fqsm::GlobalValue<Meta>& global;

        void aspect(std::string_view) {}

        template<auto... Members>
        void one(Field<Members...>) {}

        template<auto... Members>
        void one(Collection<Members...>) {}

        template<auto... Members>
        void all(Field<Members...> slot) {
            row.array.push_back(leaf::write(slot.get(global)));
        }

        template<auto... Members>
        void all(Collection<Members...> slot) {
            Value nested = Value::array_value();
            for (const auto& element : slot.get(global))
                nested.array.push_back(leaf::write(element));
            row.array.push_back(std::move(nested));
        }
    };

    template<typename Meta>
    struct ReadAllRowDesc {
        const Value& row;
        fqsm::GlobalValue<Meta>& global;
        std::size_t index = 0;

        void aspect(std::string_view) {}

        template<auto... Members>
        void one(Field<Members...>) {}

        template<auto... Members>
        void one(Collection<Members...>) {}

        template<auto... Members>
        void all(Field<Members...> slot) {
            if (index >= row.array.size())
                throw std::runtime_error("json all-row: missing field");
            leaf::read(row.array[index++], slot.get(global));
        }

        template<auto... Members>
        void all(Collection<Members...> slot) {
            if (index >= row.array.size())
                throw std::runtime_error("json all-row: missing collection");
            const auto& nested = row.array[index++];
            if (!nested.is_array())
                throw std::runtime_error("json all-row: collection must be array");
            using Elem = typename std::decay_t<decltype(slot.get(global))>::value_type;
            auto& container = slot.get(global);
            container.clear();
            for (const auto& element : nested.array) {
                Elem value = fqsm::utility::BadValue{};
                leaf::read(element, value);
                container.push_back(std::move(value));
            }
        }
    };

    template<typename Meta>
    struct HasAllSlotsDesc {
        bool has_all = false;

        void aspect(std::string_view) {}
        void one(auto&&) {}
        void all(auto&&) { has_all = true; }
    };

    template<typename Meta>
    auto has_all_slots() -> bool {
        HasAllSlotsDesc<Meta> desc{};
        Meta::describe(desc);
        return desc.has_all;
    }

    template<typename Meta>
    auto make_aspect_document(Reading context) -> Value {
        Value aspect = Value::object_value();

        Value one_table = Value::array_value();
        for (const auto entry : context->aspect<Meta>().items()) {
            Value row = Value::array_value();
            row.array.push_back(leaf::write(entry.id));
            WriteOneRowDesc<Meta> writer{row, entry.value};
            Meta::describe(writer);
            one_table.array.push_back(std::move(row));
        }
        aspect.set(std::string{section_one}, std::move(one_table));
        aspect.set(std::string{section_one_collections}, Value::array_value());

        if (has_all_slots<Meta>()) {
            Value all_table = Value::array_value();
            Value row = Value::array_value();
            WriteAllRowDesc<Meta> writer{row, with<Meta>::get_global(context)};
            Meta::describe(writer);
            all_table.array.push_back(std::move(row));
            aspect.set(std::string{section_all}, std::move(all_table));
        } else {
            aspect.set(std::string{section_all}, Value::array_value());
        }
        aspect.set(std::string{section_all_collections}, Value::array_value());

        return aspect;
    }

    template<typename Meta>
    void load_aspect_document(Writing context, const Value& aspect) {
        const auto* one_table = aspect.find(section_one);
        if (!one_table || !one_table->is_array())
            throw std::runtime_error("json aspect: missing one table");

        for (const auto& row : one_table->array) {
            if (!row.is_array() || row.array.empty())
                throw std::runtime_error("json aspect: broken one-row");
            typename Meta::Id id = fqsm::utility::BadValue{};
            leaf::read(row.array[0], id);
            with<Meta>::restore(context, id, fqsm::utility::BadValue{});
            auto quantum = with<Meta>::modify(context, id);
            ReadOneRowDesc<Meta> reader{row, *quantum, 1};
            Meta::describe(reader);
        }

        if (!has_all_slots<Meta>()) return;

        const auto* all_table = aspect.find(section_all);
        if (!all_table || !all_table->is_array() || all_table->array.empty()) return;

        const auto& row = all_table->array[0];
        if (!row.is_array())
            throw std::runtime_error("json aspect: broken all-row");
        auto global = with<Meta>::modify_global(context);
        ReadAllRowDesc<Meta> reader{row, *global, 0};
        Meta::describe(reader);
    }

    template<typename Meta>
    auto has_aspect_document(const Value& root) -> bool {
        const auto name = aspect_name_of<Meta>();
        const auto* aspect = root.find(name);
        if (!aspect || !aspect->is_object()) return false;
        const auto* one_table = aspect->find(section_one);
        return one_table && one_table->is_array();
    }

    template<typename Meta>
    void clear_aspect(Writing context) {
        std::vector<typename Meta::Id> ids;
        for (const auto entry : context->aspect<Meta>().items())
            ids.push_back(entry.id);
        for (const auto id : ids)
            with<Meta>::remove(context, id);
    }

}

namespace fqsm::processing::persistency::json {

    using namespace fqsm::api;
    using namespace detail;

    template<typename Meta>
    struct ArchiveOpsFor final : ArchiveOps {
        bool present(JsonDocument& document) override {
            return has_aspect_document<Meta>(document.root());
        }

        void clear(Writing context) override {
            clear_aspect<Meta>(context);
        }

        void pull(Writing context, JsonDocument& document) override {
            const auto name = aspect_name_of<Meta>();
            const auto* aspect = document.root().find(name);
            if (!aspect) return;
            load_aspect_document<Meta>(context, *aspect);
        }

        void push(Reading context, JsonDocument& document) override {
            document.root().set(aspect_name_of<Meta>(), make_aspect_document<Meta>(context));
        }
    };

    template<fqsm::meta::category::Any Meta>
        requires fqsm::meta::category::musthave::Retrospection<Meta>
    auto ArchiveOps::of() -> std::shared_ptr<ArchiveOps> {
        return std::make_shared<ArchiveOpsFor<Meta>>();
    }

    template<fqsm::meta::category::Any Meta>
        requires fqsm::meta::category::musthave::Retrospection<Meta>
    auto aspect() -> Schema {
        return persistency::aspect<Meta>(std::shared_ptr<AspectArchive>{ArchiveOps::of<Meta>()});
    }

}
