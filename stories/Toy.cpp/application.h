#pragma once

#include <memory>

#include <base/maybe.h>
#include <fQSM/api/interface.h>
#include <rmmr/engine.h>

namespace toy {

    using namespace fqsm::api;

    class Application : public establish::Module {
    public:
        struct Settings {
            filepath assets_root;
            string title;
            index2 window_size;
            struct {
                integer major;
                integer minor;
            } glVersion;
        };

        explicit Application(Settings settings);
        ~Application() override;

        void addEngine() { engine = add<rmmr::Engine>(); }
        void install(Schema schema);
        void initDefaultWorld(establish::Realm& world);
        void loadWorld(establish::Realm& world, filepath from);
        int run(establish::Realm& world);

        const Settings settings;

    private:
        struct State;

        Schema domain() override;
        std::shared_ptr<establish::Module::State> installState(Schema finalSchema) override;
        void prepareEngineWindow();

        std::shared_ptr<rmmr::Engine> engine;
        std::shared_ptr<State> state;
        maybe<filepath> worldFrom;
    };

}
