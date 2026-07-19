#pragma once

#include <memory>

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

        Schema schema() override;

        void install(Schema schema);
        void initDefaultWorld();
        void loadWorld(filepath from);
        int run();

        const Settings settings;

    private:
        struct State;

        std::shared_ptr<rmmr::Engine> engine;
        std::shared_ptr<State> state;
    };

}
