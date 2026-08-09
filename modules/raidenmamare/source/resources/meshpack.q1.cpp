#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/semantics/uniform.h>
#include <rmmr/system/content/loader_lwo.h>
#include <rmmr/system/content/unit_name.h>

#include <base/logging.h>
#include <base/serialization.h>

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>

namespace rmmr::resource::meshpack {

    using namespace fqsm::api;

    namespace {

        struct FileInstance {
            string material;
            umap<string, string> textures;
        };

        struct FileEntryBody {
            string geometry_file;
            umap<string, FileInstance> parts;
        };

        struct FilePayload {
            string name;
            string library;
            string texpack;
            umap<string, FileEntryBody> entries;
        };

        auto read_payload(std::istream& in) -> FilePayload {
            FilePayload out{};
            base::serialization::detail::expect(in, '{');
            out.name = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            out.library = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            out.texpack = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            out.entries = base::serialization::detail::read<umap<string, FileEntryBody>>(in);
            base::serialization::detail::expect(in, '}');
            return out;
        }

        struct LwoPackPayload {
            string name;
            string library;
            string lwo_file;
            string texpack;
            umap<string, FileInstance> parts;
        };

        auto read_lwo_pack_payload(std::istream& in) -> LwoPackPayload {
            LwoPackPayload out{};
            base::serialization::detail::expect(in, '{');
            out.name = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            out.library = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            out.lwo_file = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            out.texpack = base::serialization::detail::read<string>(in);
            base::serialization::detail::expect(in, ',');
            out.parts = base::serialization::detail::read<umap<string, FileInstance>>(in);
            base::serialization::detail::expect(in, '}');
            return out;
        }

        auto parse_unit_name(Writing context, const string& text, const string& where) -> optional<Unit::Name> {
            const auto parsed = system::content::UnitName::parse(text);
            if (not parsed) {
                (void)context.refuse(std::format("resource::meshpack: {} bad Unit::Name '{}'", where, text));
                return {};
            }
            return Unit::Name{.library = parsed->library, .own = parsed->own};
        }

        auto material_requires_albedo_map(Reading context, material::Asset::Id material_id) -> bool {
            const auto& material = with<material::Asset>::get(context, material_id);
            const auto albedo = ::rmmr::material::Semantics::id_of("albedoMap");
            for (const auto& [pass, technique] : material.techniques) {
                for (const auto uniform : technique.uniforms) {
                    if (uniform == albedo) {
                        return true;
                    }
                }
            }
            return false;
        }

        auto resolve_texpack(Writing context, const string& text) -> optional<texpack::Pack::Id> {
            if (text.empty() or text == "-") {
                return {};
            }
            const auto name = parse_unit_name(context, text, "texpack");
            if (not name) {
                return {};
            }
            const auto id = with<Assets>::find<texpack::Pack>(context, *name);
            if (not id) {
                (void)context.refuse(std::format("resource::meshpack: texpack '{}' not on shelf", text));
                return {};
            }
            return id;
        }

        auto layer_in_pack(Reading context, texpack::Pack::Id pack_id, const string& layer) -> bool {
            const auto& pack = with<texpack::Pack>::get(context, pack_id);
            return std::find(pack.layers.begin(), pack.layers.end(), layer) != pack.layers.end();
        }

        auto resolve_instance(
            Writing context,
            const FileInstance& file,
            const string& part,
            base::maybe<texpack::Pack::Id> texpack_id) -> optional<material::Instance>
        {
            const auto material_name = parse_unit_name(context, file.material, std::format("part '{}' material", part));
            if (not material_name) {
                return {};
            }
            const auto material_id = with<Assets>::find<material::Asset>(context, *material_name);
            if (not material_id) {
                (void)context.refuse(std::format("resource::meshpack: part '{}' material '{}' not on shelf", part, file.material));
                return {};
            }

            umap<string, string> textures{};
            for (const auto& [semantic, layer] : file.textures) {
                if (semantic.empty()) {
                    (void)context.refuse(std::format("resource::meshpack: part '{}' empty texture semantic", part));
                    return {};
                }
                try {
                    (void)::rmmr::material::Semantics::id_of(semantic);
                } catch (const std::exception&) {
                    (void)context.refuse(std::format("resource::meshpack: part '{}' unknown texture semantic '{}'", part, semantic));
                    return {};
                }
                if (layer.empty()) {
                    (void)context.refuse(std::format("resource::meshpack: part '{}' empty layer for '{}'", part, semantic));
                    return {};
                }
                textures.emplace(semantic, layer);
            }

            const bool needsAlbedo = material_requires_albedo_map(context, *material_id);
            const bool hasAlbedo = textures.find("albedoMap") != textures.end();
            if (needsAlbedo and not hasAlbedo) {
                (void)context.refuse(std::format("resource::meshpack: part '{}' material '{}' requires textures.albedoMap", part, file.material));
                return {};
            }
            if (not needsAlbedo and hasAlbedo) {
                (void)context.refuse(std::format("resource::meshpack: part '{}' material '{}' does not take albedoMap", part, file.material));
                return {};
            }
            if (hasAlbedo) {
                if (not texpack_id) {
                    (void)context.refuse(std::format("resource::meshpack: part '{}' has albedoMap but meshpack has no texpack", part));
                    return {};
                }
                const auto& layer = textures.at("albedoMap");
                if (not layer_in_pack(context, *texpack_id, layer)) {
                    (void)context.refuse(std::format(
                        "resource::meshpack: part '{}' albedoMap layer '{}' not in texpack",
                        part,
                        layer));
                    return {};
                }
            }

            return material::Instance{
                .material = *material_id,
                .textures = std::move(textures),
            };
        }

    } // namespace

    auto Asset::Actions::resolve(Reading context, Id pack_id, string name) -> optional<Resolved> {
        if (not with<Asset>::exists(context, pack_id)) {
            return {};
        }
        const auto& pack = with<Asset>::get(context, pack_id);
        const auto entry_it = pack.entries.find(name);
        if (entry_it == pack.entries.end()) {
            return {};
        }
        return Resolved{
            .geometry = entry_it->second.geometry,
            .parts = entry_it->second.parts,
            .texpack = pack.texpack,
        };
    }

    void LoaderObjs::Actions::load(Writing context, Id pack_id) {
        const auto& loader = with<LoaderObjs>::get(context, pack_id);
        const auto& unit = with<Unit>::get(context, pack_id);
        const auto path = with<Manager>::resolve(context, unit, loader.file);
        base::whisper("rmmr: meshpack::LoaderObjs '{}' ← {}", unit.name.text(), path.string());

        std::ifstream in{path};
        if (not in) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderObjs::load: failed to open '{}'", path.string()));
        }

        FilePayload payload;
        try {
            payload = read_payload(in);
        } catch (const std::exception& error) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderObjs::load: parse '{}': {}", path.string(), error.what()));
        }

        const auto file_name = Unit::Name{.library = payload.library, .own = payload.name};
        if (file_name != unit.name) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderObjs::load: file identity '{}' != unit '{}'", file_name.text(), unit.name.text()));
        }

        const auto texpack_id = resolve_texpack(context, payload.texpack);
        if (not payload.texpack.empty() and payload.texpack != "-" and not texpack_id) {
            return;
        }

        umap<string, Asset::Entry> entries;
        for (const auto& [entry_name, body] : payload.entries) {
            const auto geometry_id = with<Assets>::add_geometry_loader(
                context,
                Unit::Name::from(unit.name.library, entry_name),
                geometry::Loader::Quantum{.file = body.geometry_file, .layer = string{}});
            umap<string, material::Instance> parts{};
            for (const auto& [part, file_instance] : body.parts) {
                auto instance = resolve_instance(context, file_instance, part, texpack_id);
                if (not instance) {
                    return;
                }
                parts.emplace(part, std::move(*instance));
            }
            entries.emplace(entry_name, Asset::Entry{
                .geometry = geometry_id,
                .parts = std::move(parts),
            });
            base::message("rmmr: meshpack '{}' entry '{}' ← geometry '{}' ({})", unit.name.text(), entry_name, Unit::Name{.library = unit.name.library, .own = entry_name}.text(), body.geometry_file);
        }

        if (entries.empty()) {
            return (void)context.refuse(std::format(
                "resource::meshpack::LoaderObjs::load: '{}' produced no entries from '{}'",
                unit.name.text(),
                path.string()));
        }
        const auto entry_count = entries.size();
        with<Asset>::modify(context, pack_id)->texpack = texpack_id;
        with<Asset>::modify(context, pack_id)->entries = std::move(entries);
        base::message("rmmr: meshpack '{}' loaded ({} entries)", unit.name.text(), entry_count);
    }

    void LoaderLwo::Actions::load(Writing context, Id pack_id) {
        const auto& loader = with<LoaderLwo>::get(context, pack_id);
        const auto& unit = with<Unit>::get(context, pack_id);

        const auto pack_path = with<Manager>::resolve(context, unit, loader.file);
        base::whisper("rmmr: meshpack::LoaderLwo '{}' ← {}", unit.name.text(), pack_path.string());

        std::ifstream in{pack_path};
        if (not in) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::load: failed to open '{}'", pack_path.string()));
        }

        LwoPackPayload payload;
        try {
            payload = read_lwo_pack_payload(in);
        } catch (const std::exception& error) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::load: parse '{}': {}", pack_path.string(), error.what()));
        }

        const auto file_name = Unit::Name{.library = payload.library, .own = payload.name};
        if (file_name != unit.name) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::load: file identity '{}' != unit '{}'", file_name.text(), unit.name.text()));
        }

        const auto texpack_id = resolve_texpack(context, payload.texpack);
        if (not payload.texpack.empty() and payload.texpack != "-" and not texpack_id) {
            return;
        }

        const auto lwo_path = with<Manager>::resolve(context, unit, payload.lwo_file);
        auto opened = system::content::LwoDocument::open(lwo_path);
        if (not opened.document) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::load: {}", opened.error));
        }
        const auto& lwo = *opened.document;
        if (lwo.meshes().empty()) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::load: no meshes in '{}'", lwo_path.string()));
        }

        umap<string, Asset::Entry> entries;
        for (const auto& mesh : lwo.meshes()) {
            const auto geometry_id = with<Assets>::add_geometry_loader(
                context,
                Unit::Name::from(unit.name.library, mesh.name),
                geometry::Loader::Quantum{.file = payload.lwo_file, .layer = mesh.name});

            umap<string, material::Instance> parts{};
            for (const auto& sub : mesh.submeshes) {
                const auto map_it = payload.parts.find(sub.name);
                if (map_it == payload.parts.end()) {
                    return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::load: mesh '{}' submesh '{}' not in pack table", mesh.name, sub.name));
                }
                auto instance = resolve_instance(context, map_it->second, sub.name, texpack_id);
                if (not instance) {
                    return;
                }
                parts.emplace(sub.name, std::move(*instance));
            }
            if (parts.empty()) {
                return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::load: mesh '{}' has no submeshes", mesh.name));
            }

            const auto part_count = parts.size();
            entries.emplace(mesh.name, Asset::Entry{
                .geometry = geometry_id,
                .parts = std::move(parts),
            });
            base::message("rmmr: meshpack '{}' entry '{}' ← LWO mesh ({} submeshes)", unit.name.text(), mesh.name, part_count);
        }

        if (entries.empty()) {
            return (void)context.refuse(std::format(
                "resource::meshpack::LoaderLwo::load: '{}' produced no entries from '{}'",
                unit.name.text(),
                lwo_path.string()));
        }
        const auto entry_count = entries.size();
        with<Asset>::modify(context, pack_id)->texpack = texpack_id;
        with<Asset>::modify(context, pack_id)->entries = std::move(entries);
        base::message("rmmr: meshpack '{}' loaded ({} LWO entries)", unit.name.text(), entry_count);
    }

}
