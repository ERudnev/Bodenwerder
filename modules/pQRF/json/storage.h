#pragma once

#include <filesystem>
#include <iosfwd>
#include <memory>
#include <string>
#include <utility>

#include <fQSM/meta/categories.h>
#include <fQSM/processing/persistency/archivist.h>
#include <fQSM/processing/persistency/schema.h>
#include <fQSM/processing/_forwards.h>
#include <pQRF/json/document.h>

namespace fqsm::processing::persistency::json {

    class JsonDocument {
    public:
        JsonDocument() : root_(Value::object_value()) {}
        explicit JsonDocument(Value root) : root_(std::move(root)) {
            if (!root_.is_object()) root_ = Value::object_value();
        }

        auto root() -> Value& { return root_; }
        auto root() const -> const Value& { return root_; }

    private:
        Value root_;
    };

    struct ArchiveOps : AspectArchive {
        virtual ~ArchiveOps() = default;

        virtual bool present(JsonDocument&) = 0;
        virtual void replace(orchestrator::Realm&, JsonDocument&) = 0;
        virtual void pull(Writing, JsonDocument&) = 0;
        virtual void push(Reading, JsonDocument&) = 0;

        template<meta::category::Any Meta>
            requires meta::category::musthave::Retrospection<Meta>
        static auto of() -> std::shared_ptr<ArchiveOps>;
    };

    struct JsonArchivist : Archivist {
        explicit JsonArchivist(Schema schema) : Archivist(std::move(schema)) {}

        auto capture(Reading, Palette) const -> JsonDocument;
        auto to_string(Reading, Palette) const -> std::string;

        bool saveToStream(Reading, Palette, std::ostream&) const;
        bool updateFromStream(Writing, Palette, std::istream&) const;
        bool replaceFromStream(orchestrator::Realm&, Palette, std::istream&) const;

        auto getTypesAtLocation(Reading, Location) -> Palette override;
        bool updateFromLocation(Writing, Palette, Location) override;
        bool replaceFromLocation(orchestrator::Realm&, Palette, Location) override;
        bool saveToLocation(Writing, Palette, Location) override;
    };

    template<meta::category::Any Meta>
        requires meta::category::musthave::Retrospection<Meta>
    auto aspect() -> Schema;

}
