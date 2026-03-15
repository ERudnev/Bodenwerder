#include "_common.h"

#include <Atomic/varph.q1.h>
#include <Atomic/resources/textDescription.h>

namespace Test {
    using namespace iqsm::dsl_gateway;

    // locally defined Aspect (no DSL, no Q1, just internal test entity
    struct ColorRamp : Resource<ColorRamp>, Require<> {
        struct Passport {
            std::string name;
            int length = 0;
            glm::vec3 from{};
            glm::vec3 to{};
        };

        struct Quantum {
            Passport passport;
        };

        inline static const Structural invariants{{{
        }}};
    };

    struct ColorRampHandler {
        using Passport = ColorRamp::Passport;

        explicit ColorRampHandler(const Passport& passport)
            : name(passport.name) {
            iqsm::required(passport.length > 0, "ColorRampHandler: passport.length");
            colors.reserve(static_cast<size_t>(passport.length));

            if (passport.length == 1) {
                colors.push_back(passport.from);
                return;
            }

            for (int i = 0; i < passport.length; ++i) {
                const auto t = static_cast<float>(i) / static_cast<float>(passport.length - 1);
                colors.push_back(passport.from * (1.0f - t) + passport.to * t);
            }
        }

        ~ColorRampHandler() {
            iqsm::logger::message("ColorRampHandler::~ColorRampHandler(): releasing '{}', size={}", name, colors.size());
        }

        const std::string name;
        std::vector<glm::vec3> colors;
    };
}

namespace iqsm::detail::resources {
    template<>
    struct handler_of<Test::ColorRamp> {
        using type = Test::ColorRampHandler;
    };
}

namespace tests {
    void resources_scalar() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;

        World world = ops::world::create(ops::schema::assemble<Modecule>());
        auto declaration = repo::Accumulator{world};

        resources::LayerData<TextDescription, handlers::TextDescription> layer;

        const auto hydrogen = ops::resource::declare<TextDescription>(declaration, TextDescription::Quantum{{"Hydrogen.txt"}, logger::now()});
        const auto helium = ops::resource::declare<TextDescription>(declaration, TextDescription::Quantum{{"Helium.txt"}, logger::now()});

        world = ops::integrate(world, declaration.push());
        layer.sync(world);
        logger::message("resources (scalar): {}", layer.report());

        auto transaction = repo::Accumulator{world};

        const auto proton_h = ops::particle::create<Spark>(transaction, Spark::Quantum{vec4{0, 0, 0, 0}, eVt{1}});
        ops::particle::create<Charge>(transaction, proton_h, Charge::Quantum{integer{+1}});
        ops::particle::create<Strong>(transaction, proton_h, Strong::Quantum{integer{+1}});
        ops::particle::create<Nucleon>(transaction, proton_h, Nucleon::Quantum{""});

        const auto proton_he1 = ops::particle::create<Spark>(transaction, Spark::Quantum{vec4{1, 0, 0, 0}, eVt{1}});
        ops::particle::create<Charge>(transaction, proton_he1, Charge::Quantum{integer{+1}});
        ops::particle::create<Strong>(transaction, proton_he1, Strong::Quantum{integer{+1}});
        ops::particle::create<Nucleon>(transaction, proton_he1, Nucleon::Quantum{""});

        const auto proton_he2 = ops::particle::create<Spark>(transaction, Spark::Quantum{vec4{2, 0, 0, 0}, eVt{1}});
        ops::particle::create<Charge>(transaction, proton_he2, Charge::Quantum{integer{+1}});
        ops::particle::create<Strong>(transaction, proton_he2, Strong::Quantum{integer{+1}});
        ops::particle::create<Nucleon>(transaction, proton_he2, Nucleon::Quantum{""});

        const auto atom_h = ops::particle::create<Atom>(transaction, Atom::Quantum{std::vector<Spark::Id>{proton_h}, {}, "H"});
        const auto atom_he = ops::particle::create<Atom>(transaction, Atom::Quantum{std::vector<Spark::Id>{proton_he1, proton_he2}, {}, "He"});

        const auto mol_h = ops::particle::create<Modecule>(transaction, Modecule::Quantum{std::vector<Atom::Id>{atom_h}, hydrogen});
        const auto mol_he = ops::particle::create<Modecule>(transaction, Modecule::Quantum{std::vector<Atom::Id>{atom_he}, helium});

        world = ops::validate(ops::integrate(world, transaction.push()));
        EXPECT_EQ(std::string(layer.handlers.at(ops::particle::get<Modecule>(world, mol_h).descrition)->c_str()), "mock:Hydrogen.txt");
        EXPECT_EQ(std::string(layer.handlers.at(ops::particle::get<Modecule>(world, mol_he).descrition)->c_str()), "mock:Helium.txt");
    }

    void resources_manager() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;

        auto world = ops::world::create(ops::schema::assemble<Modecule>());
        auto transaction = repo::Accumulator{world};

        const auto hydrogen = ops::resource::declare<TextDescription>(transaction, TextDescription::Quantum{{"Hydrogen.txt"}, logger::now()});
        const auto helium = ops::resource::declare<TextDescription>(transaction, TextDescription::Quantum{{"Helium.txt"}, logger::now()});

        const resources::Manager manager = base::make_shared<resources::ManagerData>();
        manager->register_layer<TextDescription>();
        world = ops::validate(ops::integrate(world, transaction.push()));
        manager->sync(world);

        logger::message("resources (manager): {}", manager->report());

        EXPECT_EQ(manager->layer<TextDescription>()->handlers.at(hydrogen)->name, "Hydrogen");
        EXPECT_EQ(manager->layer<TextDescription>()->handlers.at(helium)->name, "Helium");
    }

    void resources_add_custom_layer() {
        using namespace iqsm;
        using namespace Q1CORE::Example::Varph;
        using namespace Test;

        auto transaction = repo::Sequence{ops::world::create(ops::schema::assemble<Modecule, ColorRamp>())};

        const resources::Manager manager = base::make_shared<resources::ManagerData>();
        manager->register_layer<TextDescription>();
        manager->register_layer<ColorRamp>();

        const auto hydrogen = ops::resource::declare<TextDescription>(transaction, TextDescription::Quantum{{"Hydrogen.txt"}, logger::now()});
        const auto red = ops::resource::declare<ColorRamp>(transaction, ColorRamp::Quantum{{"RedRamp", 8, glm::vec3{0, 0, 0}, glm::vec3{1, 0, 0}}});
        const auto green = ops::resource::declare<ColorRamp>(transaction, ColorRamp::Quantum{{"GreenRamp", 8, glm::vec3{0, 0, 0}, glm::vec3{0, 1, 0}}});
        const auto blue = ops::resource::declare<ColorRamp>(transaction, ColorRamp::Quantum{{"BlueRamp", 8, glm::vec3{0, 0, 0}, glm::vec3{0, 0, 1}}});

        const auto validated = ops::validate(static_cast<World>(transaction));
        manager->sync(validated);

        logger::message("resources (add custom layer): schema={}", utilities::type_names_multiline(*validated->schema));
        logger::message("resources (add custom layer): hydrogen={}", hydrogen);
        logger::message("resources (add custom layer): ramps: red={}, green={}, blue={}", red, green, blue);
        logger::message("resources (add custom layer): {}", manager->report());
    }

    void resources() {
        resources_scalar();
        resources_manager();
        resources_add_custom_layer();
    }
}

