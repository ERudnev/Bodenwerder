#pragma once

#include <filesystem>

#include <fQSM/model/intertype/set.h>
#include <fQSM/processing/persistency/schema.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::processing::orchestrator {
    struct Realm;
}

namespace fqsm::processing::persistency {

    struct Archivist {
        virtual ~Archivist() = default;

        using Location = std::filesystem::path;
        using Palette = model::intertype::Set;

        explicit Archivist(Schema schema) : schema_(std::move(schema)) {}

        auto schema() const -> Schema { return schema_; }

        virtual Palette getTypesAtLocation(Reading, Location) = 0;
        virtual bool updateFromLocation(Writing, Palette, Location) = 0;
        virtual bool replaceFromLocation(orchestrator::Realm&, Palette, Location) = 0;
        virtual bool saveToLocation(Writing, Palette, Location) = 0;

    protected:
        Schema schema_;
    };

}
