#include <rmmr/resources/meshpack.q1.h>
#include <rmmr/resources/runtimes.q1.h>
#include <rmmr/semantics/uniform.h>
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

        auto resolveSurfaceBindings(Writing context, const geometry::Asset::Quantum& asset, geometry::EntryId entryId, const umap<string, material::Instance>& declarations, const string& where) -> optional<umap<geometry::SurfaceId, material::Instance>> {
            if (entryId < 0 or static_cast<std::size_t>(entryId) >= asset.surfaceCatalogs.size()) {
                (void)context.refuse(std::format("resource::meshpack: {} geometry entry {} has no surface catalog", where, entryId));
                return {};
            }
            umap<geometry::SurfaceId, material::Instance> surfaces;
            for (const auto& [surfaceName, surfaceId] : asset.surfaceCatalogs[entryId]) {
                const auto declaration = declarations.find(surfaceName);
                if (declaration == declarations.end()) {
                    (void)context.refuse(std::format("resource::meshpack: {} geometry surface '{}' has no material declaration", where, surfaceName));
                    return {};
                }
                surfaces.emplace(surfaceId, declaration->second);
            }
            return surfaces;
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
        const auto& selected = entry_it->second;
        if (not with<geometry::Asset>::exists(context, selected.geometry)) return {};
        const auto& geometryAsset = with<geometry::Asset>::get(context, selected.geometry);
        if (selected.entry >= geometryAsset.entries.size()) return {};
        const auto& range = geometryAsset.entries[selected.entry].surfaces;
        for (const auto& [surface, _] : selected.surfaces) {
            if (surface < static_cast<geometry::SurfaceId>(range.first) or surface >= static_cast<geometry::SurfaceId>(range.first + range.count)) return {};
        }
        return Resolved{
            .geometry = selected.geometry,
            .entry = selected.entry,
            .surfaces = selected.surfaces,
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

        umap<string, LoaderObjs::PendingEntry> pending;
        umap<string, geometry::Asset::Id> geometries;
        vector<std::pair<string, const FileEntryBody*>> orderedEntries;
        for (const auto& [entryName, body] : payload.entries) orderedEntries.emplace_back(entryName, &body);
        std::ranges::sort(orderedEntries, {}, &std::pair<string, const FileEntryBody*>::first);
        for (const auto& [entryName, bodyPointer] : orderedEntries) {
            const auto& body = *bodyPointer;
            auto knownGeometry = geometries.find(body.geometry_file);
            const auto geometryId = knownGeometry != geometries.end() ? knownGeometry->second : with<Assets>::add_geometry_loader(context, Unit::Name::from(unit.name.library, entryName), geometry::Loader::Quantum{.file = body.geometry_file});
            if (knownGeometry == geometries.end()) geometries.emplace(body.geometry_file, geometryId);
            umap<string, material::Instance> surfaces;
            vector<std::pair<string, const FileInstance*>> orderedParts;
            for (const auto& [part, fileInstance] : body.parts) orderedParts.emplace_back(part, &fileInstance);
            std::ranges::sort(orderedParts, {}, &std::pair<string, const FileInstance*>::first);
            for (const auto& [part, fileInstance] : orderedParts) {
                auto instance = resolve_instance(context, *fileInstance, part, texpack_id);
                if (not instance) {
                    return;
                }
                surfaces.emplace(part, std::move(*instance));
            }
            pending.emplace(entryName, LoaderObjs::PendingEntry{
                .geometry = geometryId,
                .surfaces = std::move(surfaces),
            });
            base::message("rmmr: meshpack '{}' pending entry '{}' ← geometry '{}' ({})", unit.name.text(), entryName, Unit::Name{.library = unit.name.library, .own = entryName}.text(), body.geometry_file);
        }

        if (pending.empty()) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderObjs::load: '{}' produced no declarations from '{}'", unit.name.text(), path.string()));
        }
        const auto declarationCount = pending.size();
        auto asset = with<Asset>::modify(context, pack_id);
        asset->texpack = texpack_id;
        asset->entries.clear();
        with<LoaderObjs>::modify(context, pack_id)->pending = std::move(pending);
        base::message("rmmr: meshpack '{}' declaration loaded ({} pending entries)", unit.name.text(), declarationCount);
    }

    void LoaderObjs::Actions::finalize(Writing context, Id packId) {
        const auto& pending = with<LoaderObjs>::get(context, packId).pending;
        if (pending.empty()) return;
        const auto& unit = with<Unit>::get(context, packId);
        umap<string, Asset::Entry> entries;
        for (const auto& [entryName, declaration] : pending) {
            if (not with<geometry::Asset>::exists(context, declaration.geometry)) {
                return (void)context.refuse(std::format("resource::meshpack::LoaderObjs::finalize: '{}' geometry for entry '{}' is missing", unit.name.text(), entryName));
            }
            const auto& geometryAsset = with<geometry::Asset>::get(context, declaration.geometry);
            geometry::EntryId entryId;
            if (const auto named = geometryAsset.entryCatalog.find(entryName); named != geometryAsset.entryCatalog.end()) {
                entryId = named->second;
            } else if (geometryAsset.entryCatalog.size() == 1) {
                entryId = geometryAsset.entryCatalog.begin()->second;
            } else {
                return (void)context.refuse(std::format("resource::meshpack::LoaderObjs::finalize: '{}' entry '{}' does not select one geometry entry", unit.name.text(), entryName));
            }
            auto surfaces = resolveSurfaceBindings(context, geometryAsset, entryId, declaration.surfaces, std::format("'{}' entry '{}'", unit.name.text(), entryName));
            if (not surfaces) return;
            if (surfaces->size() != declaration.surfaces.size()) {
                return (void)context.refuse(std::format("resource::meshpack::LoaderObjs::finalize: '{}' entry '{}' declares a surface absent from geometry", unit.name.text(), entryName));
            }
            entries.emplace(entryName, Asset::Entry{.geometry = declaration.geometry, .entry = entryId, .surfaces = std::move(*surfaces)});
        }
        const auto entryCount = entries.size();
        with<Asset>::modify(context, packId)->entries = std::move(entries);
        with<LoaderObjs>::modify(context, packId)->pending.clear();
        base::message("rmmr: meshpack '{}' finalized from geometry catalogs ({} entries)", unit.name.text(), entryCount);
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

        const auto geometryId = with<Assets>::add_geometry_loader(context, Unit::Name::from(unit.name.library, std::format("{}_geometry", unit.name.own)), geometry::Loader::Quantum{.file = payload.lwo_file});
        umap<string, material::Instance> pending;
        for (const auto& [part, fileInstance] : payload.parts) {
            auto instance = resolve_instance(context, fileInstance, part, texpack_id);
            if (not instance) return;
            pending.emplace(part, std::move(*instance));
        }

        if (pending.empty()) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::load: '{}' produced no surface declarations from '{}'", unit.name.text(), pack_path.string()));
        }
        const auto declarationCount = pending.size();
        auto asset = with<Asset>::modify(context, pack_id);
        asset->texpack = texpack_id;
        asset->entries.clear();
        auto state = with<LoaderLwo>::modify(context, pack_id);
        state->geometry = geometryId;
        state->pending = std::move(pending);
        base::message("rmmr: meshpack '{}' declaration loaded ({} pending LWO surfaces)", unit.name.text(), declarationCount);
    }

    void LoaderLwo::Actions::finalize(Writing context, Id packId) {
        const auto& state = with<LoaderLwo>::get(context, packId);
        if (state.pending.empty()) return;
        const auto& unit = with<Unit>::get(context, packId);
        if (not state.geometry or not with<geometry::Asset>::exists(context, *state.geometry)) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::finalize: '{}' pooled geometry is missing", unit.name.text()));
        }
        const auto geometryId = *state.geometry;
        const auto& geometryAsset = with<geometry::Asset>::get(context, geometryId);
        if (geometryAsset.entryCatalog.empty()) {
            return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::finalize: '{}' geometry has no entries", unit.name.text()));
        }

        umap<string, Asset::Entry> entries;
        umap<string, bool> usedSurfaces;
        for (const auto& [entryName, entryId] : geometryAsset.entryCatalog) {
            auto surfaces = resolveSurfaceBindings(context, geometryAsset, entryId, state.pending, std::format("'{}' entry '{}'", unit.name.text(), entryName));
            if (not surfaces or surfaces->empty()) return;
            for (const auto& surface : geometryAsset.surfaceCatalogs[entryId]) usedSurfaces.emplace(surface.first, true);
            entries.emplace(entryName, Asset::Entry{.geometry = geometryId, .entry = entryId, .surfaces = std::move(*surfaces)});
        }
        for (const auto& declaration : state.pending) {
            const auto& surfaceName = declaration.first;
            if (not usedSurfaces.contains(surfaceName)) {
                return (void)context.refuse(std::format("resource::meshpack::LoaderLwo::finalize: '{}' declares surface '{}' absent from geometry", unit.name.text(), surfaceName));
            }
        }

        const auto entryCount = entries.size();
        with<Asset>::modify(context, packId)->entries = std::move(entries);
        auto loader = with<LoaderLwo>::modify(context, packId);
        loader->geometry = base::maybe<geometry::Asset::Id>{};
        loader->pending.clear();
        base::message("rmmr: meshpack '{}' finalized from pooled geometry catalogs ({} entries)", unit.name.text(), entryCount);
    }

}
