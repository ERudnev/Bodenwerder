#pragma once

#include <memory>

#include <base/maybe.h>
#include <fQSM/api/interface.h>
#include <rmmr/engine.h>

#include <rmmr/wrapper/product.h>

namespace rmmr::wrapper {

    using namespace fqsm::api;

    class Application : public establish::Module {
    public:
        struct FrameCapture {
            filepath destination;
            integer after_frames = 180;
            bool close_after = true;
        };

        struct Settings {
            filepath assets_root;
            string title;
            index2 window_size;
            system::Window::Presentation presentation;
            struct {
                integer major;
                integer minor;
            } glVersion;
            base::maybe<FrameCapture> capture;
        };

        explicit Application(Settings settings);
        ~Application() override;

        void setProduct(std::unique_ptr<Product> product);

        Schema schema() override;
        std::shared_ptr<establish::Module::State> install(Schema schema) override;

        void initDefaultWorld();
        void loadWorld(filepath from);
        int run();

        const Settings settings;

    private:
        struct State;

        std::shared_ptr<rmmr::Engine> engine;
        std::unique_ptr<Product> product;
        std::shared_ptr<State> state;
    };

}
