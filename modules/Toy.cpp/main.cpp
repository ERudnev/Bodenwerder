#include <iostream>
#include <filesystem>

#include <iQSM/api/_gateway.h>
#include <iQSM/logger.h>
#include <iQSM/references.h>
#include <iQSM/repository/agents/collaboration.h>
#include <iQSM/repository/agents/subsystem.h>
#include <iQSM/repository/branch.h>
#include <Raidenmamare/engine.h>


namespace Model {
    struct Logic : iqsm::agents::Subsystem {
        explicit Logic(iqsm::Schema schema)
            : schema_(schema)
            , main(iqsm::helpers::world::create(schema_))
        {}

        auto schema() const -> iqsm::Schema override { return schema_; }

        auto access() -> Update override {
            struct Replacer {
                iqsm::repo::Branch* main;
                void operator()(iqsm::World next) const { main->rebase(next); }
            };

            return Update{
                .current = main,
                .replace = Replacer{&main},
            };
        }

    private:
        iqsm::Schema schema_;
        iqsm::repo::Branch main;
    };

}


int main() {
    using namespace iqsm::logger;
    message("[{}] Test app is started...", to_string(now()));

    const auto assets_root = std::filesystem::path(DAQL_ASSETS_DIR).string();
    const auto engine_startup_parameters = rmmr::Engine::StartupParameters{
        .assets_root = assets_root,
        .title = "Raidenmamare",
        .size = rmmr::index2{.x = 3000, .y = 1200},
        .context_major = 3,
        .context_minor = 3,
    };

    const auto engineObj = base::make_shared<rmmr::Engine>(engine_startup_parameters);
    const auto schema = engineObj->schema();
    const auto logicObj = base::make_shared<Model::Logic>(schema);

    iqsm::agents::Collaboration collaboration(logicObj, engineObj); // placeholder
    collaboration.sync();

    return engineObj->run_render_demo();
}