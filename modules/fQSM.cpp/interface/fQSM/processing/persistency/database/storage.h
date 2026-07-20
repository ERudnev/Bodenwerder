#pragma once

#include <filesystem>
#include <memory>
#include <utility>

#include <sqlite3.h>

#include <fQSM/meta/categories.h>
#include <fQSM/processing/persistency/archivist.h>
#include <fQSM/processing/persistency/database/retrospection.h>
#include <fQSM/processing/_forwards.h>

namespace fqsm::processing::persistency::database {

    class DatabaseProxy {
    public:
        explicit DatabaseProxy(sqlite3* engine) : engine_(engine) {}
        DatabaseProxy(const DatabaseProxy&) = delete;
        auto operator=(const DatabaseProxy&) -> DatabaseProxy& = delete;
        DatabaseProxy(DatabaseProxy&& other) noexcept : engine_(std::exchange(other.engine_, nullptr)) {}
        auto operator=(DatabaseProxy&& other) noexcept -> DatabaseProxy& {
            if (this != &other) {
                if (engine_) sqlite3_close(engine_);
                engine_ = std::exchange(other.engine_, nullptr);
            }
            return *this;
        }
        ~DatabaseProxy() {
            if (engine_) sqlite3_close(engine_);
        }

        auto engine() const -> sqlite3* { return engine_; }

    private:
        sqlite3* engine_ = nullptr;
    };

    struct ArchiveOps : AspectArchive {
        virtual ~ArchiveOps() = default;

        virtual bool present(DatabaseProxy&) = 0;
        virtual void clear(Writing) = 0;
        virtual void pull(Writing, DatabaseProxy&) = 0;
        virtual void push(Reading, DatabaseProxy&) = 0;

        template<meta::category::Any Meta>
            requires HasRetrospection<Meta>
        static auto of() -> std::shared_ptr<ArchiveOps>;
    };

    struct DatabaseArchivist : Archivist {
        DatabaseArchivist() = default;

        auto getTypesAtLocation(Reading, Location) -> Palette override;
        bool updateFromLocation(Writing, Palette, Location) override;
        bool replaceFromLocation(Writing, Palette, Location) override;
        bool saveToLocation(Writing, Palette, Location) override;
    };

}

#include <fQSM/processing/persistency/database/engine.h>

#include <fQSM/manipulation/schema.h>

namespace fqsm::manipulation::schema {

    template<meta::category::Any Meta>
        requires processing::persistency::database::HasRetrospection<Meta>
    Schema aspect(bool persist) {
        if (!persist)
            return aspect<Meta>();
        return aspect<Meta>(std::shared_ptr<processing::AspectArchive>{
            processing::persistency::database::ArchiveOps::of<Meta>()
        });
    }

}
