#include <eltanin/mech/blueprint.q1.h>

#include <rmmr/resources/manager.q1.h>
#include <rmmr/system/content/unit_name.h>

#include <base/logging.h>

#include <cctype>
#include <format>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace eltanin::mech {

    using namespace fqsm::api;

    namespace {

        struct Cursor {
            std::string_view text;
            std::size_t at;
        };

        void skip_ws(Cursor& cursor) {
            while (cursor.at < cursor.text.size() and std::isspace(static_cast<unsigned char>(cursor.text[cursor.at])))
                ++cursor.at;
        }

        void expect(Cursor& cursor, char ch) {
            skip_ws(cursor);
            if (cursor.at >= cursor.text.size() or cursor.text[cursor.at] != ch)
                throw std::runtime_error(std::format("blueprint: expected '{}'", ch));
            ++cursor.at;
        }

        auto take_string(Cursor& cursor) -> string {
            skip_ws(cursor);
            expect(cursor, '"');
            string out;
            while (cursor.at < cursor.text.size() and cursor.text[cursor.at] != '"') {
                out.push_back(cursor.text[cursor.at]);
                ++cursor.at;
            }
            expect(cursor, '"');
            return out;
        }

        auto take_int(Cursor& cursor) -> integer {
            skip_ws(cursor);
            const auto begin = cursor.at;
            if (cursor.at < cursor.text.size() and (cursor.text[cursor.at] == '-' or cursor.text[cursor.at] == '+'))
                ++cursor.at;
            while (cursor.at < cursor.text.size() and std::isdigit(static_cast<unsigned char>(cursor.text[cursor.at])))
                ++cursor.at;
            if (begin == cursor.at or (cursor.at - begin == 1 and (cursor.text[begin] == '-' or cursor.text[begin] == '+')))
                throw std::runtime_error("blueprint: expected integer");
            return static_cast<integer>(std::stoi(std::string{cursor.text.substr(begin, cursor.at - begin)}));
        }

        auto take_index3(Cursor& cursor) -> base::common_types::index3 {
            expect(cursor, '[');
            const auto x = take_int(cursor);
            expect(cursor, ',');
            const auto y = take_int(cursor);
            expect(cursor, ',');
            const auto z = take_int(cursor);
            expect(cursor, ']');
            return base::common_types::index3{.x = x, .y = y, .z = z};
        }

        const std::unordered_map<std::string_view, mech::skeleton::Corner::Kind> knotKinds{
            {"c124", mech::skeleton::Corner::Kind::c124},
            {"c1364", mech::skeleton::Corner::Kind::c1364},
            {"c164", mech::skeleton::Corner::Kind::c164},
            {"c134", mech::skeleton::Corner::Kind::c134},
            {"c135", mech::skeleton::Corner::Kind::c135},
            {"c12", mech::skeleton::Corner::Kind::c12},
            {"c13", mech::skeleton::Corner::Kind::c13},
            {"c15", mech::skeleton::Corner::Kind::c15},
            {"c16", mech::skeleton::Corner::Kind::c16},
            {"c34", mech::skeleton::Corner::Kind::c34},
            {"c35", mech::skeleton::Corner::Kind::c35},
        };

        const std::unordered_map<std::string_view, mech::skeleton::Halfrib::Kind> halfChordKinds{
            {"he1deg90", mech::skeleton::Halfrib::Kind::he1deg90},
            {"he1deg45", mech::skeleton::Halfrib::Kind::he1deg45},
            {"he3deg71", mech::skeleton::Halfrib::Kind::he3deg71},
            {"he3deg90", mech::skeleton::Halfrib::Kind::he3deg90},
            {"he3deg125", mech::skeleton::Halfrib::Kind::he3deg125},
        };

        const std::unordered_map<std::string_view, mech::skeleton::Membrane::Kind> wallKinds{
            {"u1111", mech::skeleton::Membrane::Kind::u1111},
            {"u121", mech::skeleton::Membrane::Kind::u121},
            {"u2121", mech::skeleton::Membrane::Kind::u2121},
            {"u222A", mech::skeleton::Membrane::Kind::u222A},
            {"u222V", mech::skeleton::Membrane::Kind::u222V},
        };

        const std::unordered_map<std::string_view, mech::frame::shape> frameShapes{
            {"k8", mech::frame::shape::k8},
            {"k7", mech::frame::shape::k7},
            {"k6", mech::frame::shape::k6},
            {"k4", mech::frame::shape::k4},
            {"k4f1111", mech::frame::shape::k4f1111},
            {"k3f121", mech::frame::shape::k3f121},
            {"k4f2121", mech::frame::shape::k4f2121},
            {"k3f222", mech::frame::shape::k3f222},
        };

        auto knotKindName(mech::skeleton::Corner::Kind kind) -> std::string_view {
            for (const auto& [name, value] : knotKinds) {
                if (value == kind)
                    return name;
            }
            throw std::runtime_error("blueprint: unknown knot kind");
        }

        auto halfChordKindName(mech::skeleton::Halfrib::Kind kind) -> std::string_view {
            for (const auto& [name, value] : halfChordKinds) {
                if (value == kind)
                    return name;
            }
            throw std::runtime_error("blueprint: unknown half-chord kind");
        }

        auto wallKindName(mech::skeleton::Membrane::Kind kind) -> std::string_view {
            for (const auto& [name, value] : wallKinds) {
                if (value == kind)
                    return name;
            }
            throw std::runtime_error("blueprint: unknown wall kind");
        }

        auto frameShapeName(mech::frame::shape shape) -> std::string_view {
            for (const auto& [name, value] : frameShapes) {
                if (value == shape)
                    return name;
            }
            throw std::runtime_error("blueprint: unknown frame shape");
        }

        auto take_knot(Cursor& cursor) -> mech::skeleton::Corner {
            expect(cursor, '[');
            const auto kindName = take_string(cursor);
            const auto kindIt = knotKinds.find(kindName);
            if (kindIt == knotKinds.end())
                throw std::runtime_error(std::format("blueprint: unknown knot kind '{}'", kindName));
            expect(cursor, ',');
            const auto ori = take_int(cursor);
            expect(cursor, ']');
            return mech::skeleton::Corner{.kind = kindIt->second, .ori = static_cast<mech::space::orient::key>(ori)};
        }

        auto take_half_chord(Cursor& cursor) -> mech::skeleton::Halfrib {
            expect(cursor, '[');
            const auto kindName = take_string(cursor);
            const auto kindIt = halfChordKinds.find(kindName);
            if (kindIt == halfChordKinds.end())
                throw std::runtime_error(std::format("blueprint: unknown half-chord kind '{}'", kindName));
            expect(cursor, ',');
            const auto poleName = take_string(cursor);
            mech::skeleton::Halfrib::Pole pole = mech::skeleton::Halfrib::Pole::starts;
            if (poleName == "starts")
                pole = mech::skeleton::Halfrib::Pole::starts;
            else if (poleName == "ends")
                pole = mech::skeleton::Halfrib::Pole::ends;
            else
                throw std::runtime_error(std::format("blueprint: unknown half-chord pole '{}'", poleName));
            expect(cursor, ',');
            const auto ori = take_int(cursor);
            expect(cursor, ']');
            return mech::skeleton::Halfrib{.kind = kindIt->second, .pole = pole, .ori = static_cast<mech::space::orient::key>(ori)};
        }

        auto take_wall(Cursor& cursor) -> mech::skeleton::Membrane {
            expect(cursor, '[');
            const auto kindName = take_string(cursor);
            const auto kindIt = wallKinds.find(kindName);
            if (kindIt == wallKinds.end())
                throw std::runtime_error(std::format("blueprint: unknown wall kind '{}'", kindName));
            expect(cursor, ',');
            const auto ori = take_int(cursor);
            expect(cursor, ']');
            return mech::skeleton::Membrane{.kind = kindIt->second, .ori = static_cast<mech::space::orient::key>(ori)};
        }

        template <typename Item, typename Take>
        auto take_list(Cursor& cursor, Take take) -> std::vector<Item> {
            expect(cursor, '[');
            skip_ws(cursor);
            std::vector<Item> out;
            if (cursor.at < cursor.text.size() and cursor.text[cursor.at] == ']') {
                ++cursor.at;
                return out;
            }
            for (;;) {
                out.push_back(take(cursor));
                skip_ws(cursor);
                if (cursor.at < cursor.text.size() and cursor.text[cursor.at] == ']') {
                    ++cursor.at;
                    break;
                }
                expect(cursor, ',');
            }
            return out;
        }

        auto take_cell(Cursor& cursor) -> mech::Blueprint::Cell {
            expect(cursor, '[');
            const auto pos = take_index3(cursor);
            expect(cursor, ',');
            const auto ori = take_int(cursor);
            expect(cursor, ',');
            const auto shapeName = take_string(cursor);
            const auto shapeIt = frameShapes.find(shapeName);
            if (shapeIt == frameShapes.end())
                throw std::runtime_error(std::format("blueprint: unknown frame shape '{}'", shapeName));
            expect(cursor, ',');
            auto corners = take_list<mech::skeleton::Corner>(cursor, take_knot);
            expect(cursor, ',');
            auto halfribs = take_list<mech::skeleton::Halfrib>(cursor, take_half_chord);
            expect(cursor, ',');
            auto membranes = take_list<mech::skeleton::Membrane>(cursor, take_wall);
            expect(cursor, ']');
            return mech::Blueprint::Cell{
                .placement = skeleton::Placement{.cell = pos, .ori = static_cast<space::orient::key>(ori)},
                .shape = shapeIt->second,
                .corners = std::move(corners),
                .halfribs = std::move(halfribs),
                .membranes = std::move(membranes),
            };
        }

        auto take_unit_name(Cursor& cursor) -> rmmr::resource::Unit::Name {
            const auto text = take_string(cursor);
            const auto parsed = rmmr::system::content::UnitName::parse(text);
            if (not parsed)
                throw std::runtime_error(std::format("blueprint: bad Unit::Name '{}'", text));
            return rmmr::resource::Unit::Name::from(parsed->library, parsed->own);
        }

        auto take_mounted(Cursor& cursor) -> Blueprint::Mounted {
            expect(cursor, '[');
            auto mount = take_unit_name(cursor);
            expect(cursor, ',');
            const auto pos = take_index3(cursor);
            expect(cursor, ',');
            const auto ori = take_int(cursor);
            expect(cursor, ']');
            return Blueprint::Mounted{.mount = std::move(mount), .transform = space::Transform{.grid = pos, .rotation = static_cast<space::orient::key>(ori)}};
        }

        auto parse_blueprint(std::string_view text) -> Blueprint::Quantum {
            Cursor cursor{.text = text, .at = 0};
            expect(cursor, '{');
            auto name = take_string(cursor);
            expect(cursor, ',');
            auto author = take_string(cursor);
            skip_ws(cursor);
            std::vector<Blueprint::Cell> cells;
            std::vector<Blueprint::Mounted> mounts;
            if (cursor.at < cursor.text.size() and cursor.text[cursor.at] == ',') {
                ++cursor.at;
                cells = take_list<Blueprint::Cell>(cursor, take_cell);
                skip_ws(cursor);
                if (cursor.at < cursor.text.size() and cursor.text[cursor.at] == ',') {
                    ++cursor.at;
                    mounts = take_list<Blueprint::Mounted>(cursor, take_mounted);
                }
            }
            expect(cursor, '}');
            return Blueprint::Quantum{.name = std::move(name), .author = std::move(author), .cells = std::move(cells), .mounts = std::move(mounts), .file = {}};
        }

        auto format_knot(const mech::skeleton::Corner& knot) -> std::string {
            return std::format("[\"{}\", {}]", knotKindName(knot.kind), knot.ori);
        }

        auto format_half_chord(const mech::skeleton::Halfrib& halfChord) -> std::string {
            const char* pole = halfChord.pole == mech::skeleton::Halfrib::Pole::starts ? "starts" : "ends";
            return std::format("[\"{}\", \"{}\", {}]", halfChordKindName(halfChord.kind), pole, halfChord.ori);
        }

        auto format_wall(const mech::skeleton::Membrane& wall) -> std::string {
            return std::format("[\"{}\", {}]", wallKindName(wall.kind), wall.ori);
        }

        template <typename Item, typename Format>
        void format_list_inline(std::ostringstream& out, const std::vector<Item>& items, Format format, std::string_view indent) {
            if (items.empty()) {
                out << "[]";
                return;
            }
            out << "[\n";
            for (std::size_t i = 0; i < items.size(); ++i) {
                out << indent << "    " << format(items[i]);
                out << (i + 1 < items.size() ? ",\n" : "\n");
            }
            out << indent << "]";
        }

        auto format_mounted(const Blueprint::Mounted& mounted) -> std::string {
            return std::format("[\"{}\", [{}, {}, {}], {}]", mounted.mount.text(), mounted.transform.grid.x, mounted.transform.grid.y, mounted.transform.grid.z, mounted.transform.rotation);
        }

        auto format_blueprint(const Blueprint::Quantum& data) -> std::string {
            std::ostringstream out;
            out << "{\n";
            out << "    \"" << data.name << "\",\n";
            out << "    \"" << data.author << "\",\n";
            if (data.cells.empty()) {
                out << "    [],\n";
            } else {
                out << "    [\n";
                for (std::size_t c = 0; c < data.cells.size(); ++c) {
                    const auto& cell = data.cells[c];
                    out << "        [\n";
                    out << "            [" << cell.placement.cell.x << ", " << cell.placement.cell.y << ", " << cell.placement.cell.z << "],\n";
                    out << "            " << cell.placement.ori << ",\n";
                    out << "            \"" << frameShapeName(cell.shape) << "\",\n";
                    out << "            ";
                    format_list_inline(out, cell.corners, format_knot, "            ");
                    out << ",\n            ";
                    format_list_inline(out, cell.halfribs, format_half_chord, "            ");
                    out << ",\n            ";
                    format_list_inline(out, cell.membranes, format_wall, "            ");
                    out << "\n        ]";
                    out << (c + 1 < data.cells.size() ? ",\n" : "\n");
                }
                out << "    ],\n";
            }
            if (data.mounts.empty()) {
                out << "    []\n";
            } else {
                out << "    [\n";
                for (std::size_t i = 0; i < data.mounts.size(); ++i) {
                    out << "        " << format_mounted(data.mounts[i]);
                    out << (i + 1 < data.mounts.size() ? ",\n" : "\n");
                }
                out << "    ]\n";
            }
            out << "}\n";
            return out.str();
        }

    } // namespace

    void Blueprint::Actions::load(Writing context, Id id) {
        const auto& unit = with<rmmr::resource::Unit>::get(context, id);
        const auto& blueprint = with<Blueprint>::get(context, id);
        const auto path = with<rmmr::resource::Manager>::resolve(context, unit, blueprint.file);

        std::ifstream input{path};
        if (not input)
            return (void)context.refuse(std::format("mech::Blueprint::load: cannot open '{}'", path.string()));

        std::ostringstream buffer;
        buffer << input.rdbuf();
        try {
            auto parsed = parse_blueprint(buffer.str());
            auto writable = with<Blueprint>::modify(context, id);
            const auto file = writable->file;
            *writable = std::move(parsed);
            writable->file = file;
        } catch (const std::exception& error) {
            return (void)context.refuse(std::format("mech::Blueprint::load: '{}': {}", path.string(), error.what()));
        }
    }

    void Blueprint::Actions::save(Writing context, Id id) {
        const auto& unit = with<rmmr::resource::Unit>::get(context, id);
        const auto& blueprint = with<Blueprint>::get(context, id);
        const auto path = with<rmmr::resource::Manager>::resolve(context, unit, blueprint.file);

        std::ofstream output{path, std::ios::binary | std::ios::trunc};
        if (not output)
            return (void)context.refuse(std::format("mech::Blueprint::save: cannot open '{}'", path.string()));

        try {
            output << format_blueprint(blueprint);
            if (not output)
                return (void)context.refuse(std::format("mech::Blueprint::save: write failed '{}'", path.string()));
        } catch (const std::exception& error) {
            return (void)context.refuse(std::format("mech::Blueprint::save: '{}': {}", path.string(), error.what()));
        }
    }

}
