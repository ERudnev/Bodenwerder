#include <filesystem>
#include <format>
#include <optional>

#include <base/logging.h>

#include <fQSM/processing/orchestrators/realm.h>
#include <pQRF/database/engine.h>

namespace fqsm::processing::persistency::database {

    using namespace fqsm::api;
    using namespace detail;

    namespace {

        auto open_existing(const std::filesystem::path& dbPath) -> std::optional<DatabaseProxy> {
            if (!std::filesystem::exists(dbPath)) return std::nullopt;
            sqlite3* engine = nullptr;
            if (sqlite3_open(dbPath.string().c_str(), &engine) != SQLITE_OK) {
                sqlite3_close(engine);
                return std::nullopt;
            }
            return DatabaseProxy{engine};
        }

        auto ops_for(const Graph::Node& node) -> std::shared_ptr<ArchiveOps> {
            return std::dynamic_pointer_cast<ArchiveOps>(node.archive);
        }

    }

    auto DatabaseArchivist::getTypesAtLocation(Reading, Location location) -> Palette {
        Palette found;
        auto db = open_existing(location);
        if (!db) return found;

        for (const auto& [type, node] : schema_->nodes) {
            const auto ops = ops_for(node);
            if (!ops) continue;
            if (ops->present(*db))
                found.insert(type);
        }
        return found;
    }

    bool DatabaseArchivist::updateFromLocation(Writing context, Palette palette, Location location) {
        auto db = open_existing(location);
        if (!db) return false;

        bool loaded = false;
        for (const auto& type : palette) {
            const auto found = schema_->nodes.find(type);
            if (found == schema_->nodes.end()) continue;
            const auto ops = ops_for(found->second);
            if (!ops) continue;
            if (!ops->present(*db)) continue;
            ops->pull(context, *db);
            loaded = true;
        }
        return loaded;
    }

    bool DatabaseArchivist::replaceFromLocation(orchestrator::Realm& realm, Palette palette, Location location) {
        auto db = open_existing(location);
        if (!db) return false;

        bool loaded = false;
        for (const auto& type : palette) {
            const auto found = schema_->nodes.find(type);
            if (found == schema_->nodes.end()) continue;
            const auto ops = ops_for(found->second);
            if (!ops) continue;
            if (!ops->present(*db)) continue;
            ops->replace(realm, *db);
            loaded = true;
        }
        return loaded;
    }

    bool DatabaseArchivist::saveToLocation(Writing context, Palette palette, Location location) {
        std::filesystem::create_directories(location.parent_path());

        sqlite3* engine = nullptr;
        if (sqlite3_open(location.string().c_str(), &engine) != SQLITE_OK) {
            sqlite3_close(engine);
            return false;
        }
        DatabaseProxy db{engine};

        try {
            exec(db.engine(), "BEGIN");

            const Reading reading = context;
            for (const auto& type : palette) {
                const auto found = schema_->nodes.find(type);
                if (found == schema_->nodes.end()) continue;
                const auto ops = ops_for(found->second);
                if (!ops) continue;
                ops->push(reading, db);
            }

            exec(db.engine(), "COMMIT");
        } catch (...) {
            sqlite3_exec(db.engine(), "ROLLBACK", nullptr, nullptr, nullptr);
            throw;
        }

        base::message("saved registry to {}", location.string());
        return true;
    }

}
