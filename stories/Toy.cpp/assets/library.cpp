#include "assets/library.h"

#include <stdexcept>
#include <unordered_map>

#include <base/logging.h>
#include <pQRF/database/engine.h>
#include <pQRF/json/engine.h>
#include <rmmr/api/_interface.h>
#include <rmmr/resources/builders/materialPresets.h>
#include <rmmr/resources/manager.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/resources/shaders.q1.h>

namespace toy::assets {

    using namespace fqsm::api;
    using namespace rmmr;
    namespace js = fqsm::processing::persistency::json;
    namespace db = fqsm::processing::persistency::database;

    namespace {

        auto catalogueSchema() -> fqsm::processing::persistency::Schema {
            return api::Assets::persistencySchema<js::Aspect>();
        }

        // Temporary (workbench): SQLite mirror schema; remove with the dual-write below.
        auto catalogueDbSchema() -> fqsm::processing::persistency::Schema {
            return api::Assets::persistencySchema<db::Aspect>();
        }

        // Temporary: sibling of catalogue.json → catalogue.sqlite
        auto catalogueDbPath(const Manager::Location& json_location) -> Manager::Location {
            auto path = json_location;
            path.replace_extension(".sqlite");
            return path;
        }

        auto unitByName(Reading context) -> umap<string, resource::Unit::Id> {
            umap<string, resource::Unit::Id> by_name;
            for (const auto entry : context->aspect<resource::Unit>().items())
                by_name.insert_or_assign(entry.value.name, entry.id);
            return by_name;
        }

        auto requireUnit(const umap<string, resource::Unit::Id>& by_name, const string& name) -> resource::Unit::Id {
            const auto found = by_name.find(name);
            if (found == by_name.end())
                throw std::runtime_error("toy assets: missing unit '" + name + "' after load");
            return found->second;
        }

        void fillHandles(Handles& handles, Reading context) {
            const auto by_name = unitByName(context);
            handles = {};
            handles.texture.debug = {
                requireUnit(by_name, "debug01"),
                requireUnit(by_name, "debug02"),
                requireUnit(by_name, "debug03"),
                requireUnit(by_name, "debug04"),
            };
            handles.texture.whiteCircle = requireUnit(by_name, "white_circle");
            handles.texture.whiteRing = requireUnit(by_name, "white_ring");
            handles.material.ambient = requireUnit(by_name, "ambient_material");
            handles.material.lit = requireUnit(by_name, "lit_material");
            handles.material.debugLitTextured = {
                requireUnit(by_name, "lit_textured_debug01"),
                requireUnit(by_name, "lit_textured_debug02"),
                requireUnit(by_name, "lit_textured_debug03"),
                requireUnit(by_name, "lit_textured_debug04"),
            };
            handles.material.litTexturedAlpha = requireUnit(by_name, "lit_textured_alpha_ring");
            handles.material.grid = requireUnit(by_name, "grid_material");
            handles.primitive.triangle = requireUnit(by_name, "triangle");
            handles.primitive.kube = requireUnit(by_name, "kube");
            handles.primitive.bagel = requireUnit(by_name, "bagel");
            handles.primitive.grid = requireUnit(by_name, "grid");
        }

    }

    auto Manager::statePath(const std::filesystem::path& assets_root) -> Location {
        return assets_root / "Toy" / "state" / "resources" / "catalogue.json";
    }

    auto Manager::prepare(establish::Realm& world, Location location) -> PrepareStatus {
        // Temporary (workshop pt.1): ignore existing catalogue — always reseed.
        // Load is off so Generated → save will overwrite JSON/SQLite on disk.
        // Restore the exists/loadFrom path below when load/remap is ready.
        (void)location;
        hardcodedInit(world);
        return PrepareStatus::Generated;

#if 0 // temporary: load path disabled for workshop overwrite mode
        if (!std::filesystem::exists(location)) {
            hardcodedInit(world);
            return PrepareStatus::Generated;
        }

        try {
            {
                Stewarding session = world;
                if (not loadFrom(session, location)) {
                    base::message("toy: assets state present but load failed: {}", location.string());
                    return PrepareStatus::Failed;
                }
            }
            fillHandles(handles, world);
            base::message("toy: assets loaded from {}", location.string());
            return PrepareStatus::Loaded;
        } catch (const std::exception& error) {
            base::message("toy: assets load threw: {} ({})", error.what(), location.string());
            return PrepareStatus::Failed;
        }
#endif
    }

    void Manager::hardcodedInit(Writing context) {
        using namespace resource;
        using geometry::Generator;

        base::message("toy: seeding assets (hardcoded)...");

        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context, core,
            item<Unit>{.manager = core, .name = "debug01", .library = "rmmr"},
            item<texture::Asset>{},
            item<texture::Loader>{.file = "textures/debug01.jpg"}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context, core,
            item<Unit>{.manager = core, .name = "debug02", .library = "rmmr"},
            item<texture::Asset>{},
            item<texture::Loader>{.file = "textures/debug02.jpg"}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context, core,
            item<Unit>{.manager = core, .name = "debug03", .library = "rmmr"},
            item<texture::Asset>{},
            item<texture::Loader>{.file = "textures/debug03.jpg"}));
        handles.texture.debug.push_back(with<Assets>::add_texture_loader(
            context, core,
            item<Unit>{.manager = core, .name = "debug04", .library = "rmmr"},
            item<texture::Asset>{},
            item<texture::Loader>{.file = "textures/debug04.jpg"}));
        handles.texture.whiteCircle = with<Assets>::add_texture_generator(
            context, core,
            item<Unit>{.manager = core, .name = "white_circle", .library = "rmmr"},
            item<texture::Asset>{},
            item<texture::Generator>{.size = index2{256, 256}, .pattern = texture::Generator::Pattern::whiteCircle});
        handles.texture.whiteRing = with<Assets>::add_texture_generator(
            context, core,
            item<Unit>{.manager = core, .name = "white_ring", .library = "rmmr"},
            item<texture::Asset>{},
            item<texture::Generator>{.size = index2{256, 256}, .pattern = texture::Generator::Pattern::whiteRing});

        const auto ambient_shader = with<Assets>::add_shader_loader(context, core, item<Unit>{.manager = core, .name = "ambient_shader", .library = "rmmr"}, item<shader::Asset>{}, item<shader::Loader>{.vertex = "shaders/ambient.vert.glsl", .fragment = "shaders/ambient.frag.glsl"});
        const auto lit_shader = with<Assets>::add_shader_loader(context, core, item<Unit>{.manager = core, .name = "lit_shader", .library = "rmmr"}, item<shader::Asset>{}, item<shader::Loader>{.vertex = "shaders/lit.vert.glsl", .fragment = "shaders/lit.frag.glsl"});
        const auto lit_textured_shader = with<Assets>::add_shader_loader(context, core, item<Unit>{.manager = core, .name = "lit_textured_shader", .library = "rmmr"}, item<shader::Asset>{}, item<shader::Loader>{.vertex = "shaders/litTextured.vert.glsl", .fragment = "shaders/litTextured.frag.glsl"});
        const auto lit_textured_alpha_shader = with<Assets>::add_shader_loader(context, core, item<Unit>{.manager = core, .name = "lit_textured_alpha_shader", .library = "rmmr"}, item<shader::Asset>{}, item<shader::Loader>{.vertex = "shaders/litTextured.vert.glsl", .fragment = "shaders/litTexturedAlpha.frag.glsl"});
        const auto grid_shader = with<Assets>::add_shader_loader(context, core, item<Unit>{.manager = core, .name = "grid_shader", .library = "rmmr"}, item<shader::Asset>{}, item<shader::Loader>{.vertex = "shaders/Grid.vert.glsl", .fragment = "shaders/Grid.frag.glsl"});
        const auto shadow_depth_shader = with<Assets>::add_shader_loader(context, core, item<Unit>{.manager = core, .name = "shadow_depth_shader", .library = "rmmr"}, item<shader::Asset>{}, item<shader::Loader>{.vertex = "shaders/shadowDepth.vert.glsl", .fragment = "shaders/shadowDepth.frag.glsl"});

        handles.material.ambient = with<Assets>::add_material(context, core, item<Unit>{.manager = core, .name = "ambient_material", .library = "rmmr"}, builders::material::Presets::ambient(with<Unit>::remember(context, ambient_shader), with<Unit>::remember(context, shadow_depth_shader)), item<resource::material::Composer>{});
        handles.material.lit = with<Assets>::add_material(context, core, item<Unit>{.manager = core, .name = "lit_material", .library = "rmmr"}, builders::material::Presets::lit(with<Unit>::remember(context, lit_shader), with<Unit>::remember(context, shadow_depth_shader)), item<resource::material::Composer>{});
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, core, item<Unit>{.manager = core, .name = "lit_textured_debug01", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[0]), with<Unit>::remember(context, shadow_depth_shader)), item<resource::material::Composer>{}));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, core, item<Unit>{.manager = core, .name = "lit_textured_debug02", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[1]), with<Unit>::remember(context, shadow_depth_shader)), item<resource::material::Composer>{}));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, core, item<Unit>{.manager = core, .name = "lit_textured_debug03", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[2]), with<Unit>::remember(context, shadow_depth_shader)), item<resource::material::Composer>{}));
        handles.material.debugLitTextured.push_back(with<Assets>::add_material(context, core, item<Unit>{.manager = core, .name = "lit_textured_debug04", .library = "rmmr"}, builders::material::Presets::litTextured(with<Unit>::remember(context, lit_textured_shader), with<Unit>::remember(context, handles.texture.debug[3]), with<Unit>::remember(context, shadow_depth_shader)), item<resource::material::Composer>{}));
        handles.material.litTexturedAlpha = with<Assets>::add_material(context, core, item<Unit>{.manager = core, .name = "lit_textured_alpha_ring", .library = "rmmr"}, builders::material::Presets::litTexturedTransparent(with<Unit>::remember(context, lit_textured_alpha_shader), with<Unit>::remember(context, *handles.texture.whiteRing)), item<resource::material::Composer>{});
        handles.material.grid = with<Assets>::add_material(context, core, item<Unit>{.manager = core, .name = "grid_material", .library = "rmmr"}, builders::material::Presets::grid(with<Unit>::remember(context, grid_shader)), item<resource::material::Composer>{});

        handles.primitive.triangle = with<Assets>::add_geometry_generator(context, core, item<Unit>{.manager = core, .name = "triangle", .library = "rmmr"}, item<geometry::Asset>{}, item<Generator>{.type = Generator::Type::triangle});
        handles.primitive.kube = with<Assets>::add_geometry_generator(context, core, item<Unit>{.manager = core, .name = "kube", .library = "rmmr"}, item<geometry::Asset>{}, item<Generator>{.type = Generator::Type::kube});
        handles.primitive.bagel = with<Assets>::add_geometry_generator(context, core, item<Unit>{.manager = core, .name = "bagel", .library = "rmmr"}, item<geometry::Asset>{}, item<Generator>{.type = Generator::Type::bagel});
        handles.primitive.grid = with<Assets>::add_geometry_generator(context, core, item<Unit>{.manager = core, .name = "grid", .library = "rmmr"}, item<geometry::Asset>{}, item<Generator>{.type = Generator::Type::gridPlane});

        base::message("toy: assets seeded");
    }

    bool Manager::loadFrom(Stewarding steward, Location location) {
        const auto schema = catalogueSchema();
        js::JsonArchivist archivist(schema);
        const auto palette = schema->types();
        if (not archivist.loadFromLocation(steward, palette, location))
            return false;
        // Archive rows are in; mapping onto the live Manager/Unit_group is not done.
        base::message("Id remap is not implemented, loading is not available");
        return false;
    }

    void Manager::save(Writing context, Location location) {
        const auto schema = catalogueSchema();
        js::JsonArchivist archivist(schema);
        if (not archivist.saveToLocation(context, schema->types(), location))
            throw std::runtime_error("toy assets: save failed: " + location.string());

        // Temporary workbench mirror: write SQLite next to JSON; never read back.
        // Drop this block (and catalogueDbSchema / catalogueDbPath) once DB/JSON parity is settled.
        {
            const auto db_location = catalogueDbPath(location);
            const auto db_schema = catalogueDbSchema();
            db::DatabaseArchivist db_archivist(db_schema);
            if (not db_archivist.saveToLocation(context, db_schema->types(), db_location))
                throw std::runtime_error("toy assets: sqlite mirror save failed: " + db_location.string());
        }
    }

}
