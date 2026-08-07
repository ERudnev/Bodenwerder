#include <eltanin/resources/blueprint.q1.h>

#include <rmmr/resources/manager.q1.h>

#include <base/logging.h>

#include <cctype>
#include <format>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace eltanin::resource::blueprint {

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

        template<typename Enum>
        auto take_enum(Cursor& cursor, const std::unordered_map<std::string_view, Enum>& table, std::string_view what) -> Enum {
            const auto name = take_string(cursor);
            const auto it = table.find(name);
            if (it == table.end())
                throw std::runtime_error(std::format("blueprint: unknown {} '{}'", what, name));
            return it->second;
        }

        const std::unordered_map<std::string_view, mech::frame::shape> frame_shapes{
            {"k8", mech::frame::shape::k8},
            {"k7", mech::frame::shape::k7},
            {"k6", mech::frame::shape::k6},
            {"k4", mech::frame::shape::k4},
            {"k4f1111", mech::frame::shape::k4f1111},
            {"k3f121", mech::frame::shape::k3f121},
            {"k4f2121", mech::frame::shape::k4f2121},
            {"k3f222", mech::frame::shape::k3f222},
        };
        const std::unordered_map<std::string_view, mech::plate::shape> plate_shapes{
            {"p1111", mech::plate::shape::p1111},
            {"p121", mech::plate::shape::p121},
            {"p2121", mech::plate::shape::p2121},
            {"p222A", mech::plate::shape::p222A},
            {"p222V", mech::plate::shape::p222V},
        };
        const std::unordered_map<std::string_view, mech::slot::inner> slot_inners{
            {"multi", mech::slot::inner::multi},
            {"engine", mech::slot::inner::engine},
            {"power", mech::slot::inner::power},
            {"battery", mech::slot::inner::battery},
            {"hardpoint", mech::slot::inner::hardpoint},
            {"hangar", mech::slot::inner::hangar},
            {"cargo", mech::slot::inner::cargo},
            {"logistic", mech::slot::inner::logistic},
            {"emissive", mech::slot::inner::emissive},
            {"control", mech::slot::inner::control},
            {"living", mech::slot::inner::living},
        };
        const std::unordered_map<std::string_view, mech::slot::plate> slot_plates{
            {"armor", mech::slot::plate::armor},
            {"thruster", mech::slot::plate::thruster},
            {"cooling", mech::slot::plate::cooling},
            {"turret", mech::slot::plate::turret},
            {"barrel", mech::slot::plate::barrel},
            {"pd", mech::slot::plate::pd},
            {"hatch", mech::slot::plate::hatch},
            {"bay", mech::slot::plate::bay},
            {"antenna", mech::slot::plate::antenna},
            {"cockpit", mech::slot::plate::cockpit},
            {"windowed", mech::slot::plate::windowed},
            {"agfe", mech::slot::plate::agfe},
            {"utility", mech::slot::plate::utility},
            {"logistic", mech::slot::plate::logistic},
        };

        auto take_cell(Cursor& cursor) -> mech::Element::Cell {
            expect(cursor, '[');
            const auto pos = take_index3(cursor);
            expect(cursor, ',');
            const auto ori = take_int(cursor);
            expect(cursor, ',');
            const auto shape = take_enum(cursor, frame_shapes, "frame shape");
            expect(cursor, ',');
            const auto role = take_enum(cursor, slot_inners, "inner slot");
            expect(cursor, ']');
            return mech::Element::Cell{.pose = mech::Pose{.pos = pos, .ori = ori}, .shape = shape, .role = role};
        }

        auto take_plate(Cursor& cursor) -> mech::Element::Plate {
            expect(cursor, '[');
            const auto pos = take_index3(cursor);
            expect(cursor, ',');
            const auto ori = take_int(cursor);
            expect(cursor, ',');
            const auto shape = take_enum(cursor, plate_shapes, "plate shape");
            expect(cursor, ',');
            const auto role = take_enum(cursor, slot_plates, "plate slot");
            expect(cursor, ']');
            return mech::Element::Plate{.pose = mech::Pose{.pos = pos, .ori = ori}, .shape = shape, .role = role};
        }

        template<typename Item, typename ParseItem>
        auto take_array(Cursor& cursor, ParseItem parse_item) -> std::vector<Item> {
            expect(cursor, '[');
            skip_ws(cursor);
            std::vector<Item> out;
            if (cursor.at < cursor.text.size() and cursor.text[cursor.at] == ']') {
                ++cursor.at;
                return out;
            }
            for (;;) {
                out.push_back(parse_item(cursor));
                skip_ws(cursor);
                if (cursor.at < cursor.text.size() and cursor.text[cursor.at] == ']') {
                    ++cursor.at;
                    break;
                }
                expect(cursor, ',');
            }
            return out;
        }

        auto parse_blueprint(std::string_view text) -> mech::Blueprint {
            Cursor cursor{.text = text, .at = 0};
            expect(cursor, '{');
            auto name = take_string(cursor);
            expect(cursor, ',');
            auto author = take_string(cursor);
            expect(cursor, ',');
            auto cells = take_array<mech::Element::Cell>(cursor, take_cell);
            expect(cursor, ',');
            auto hull = take_array<mech::Element::Plate>(cursor, take_plate);
            expect(cursor, '}');
            return mech::Blueprint{
                .name = std::move(name),
                .author = std::move(author),
                .cells = std::move(cells),
                .hull = std::move(hull),
            };
        }

        template<typename Enum>
        auto enum_name(const std::unordered_map<std::string_view, Enum>& table, Enum value, std::string_view what) -> std::string_view {
            for (const auto& [name, entry] : table) {
                if (entry == value)
                    return name;
            }
            throw std::runtime_error(std::format("blueprint: unknown {} value", what));
        }

        auto format_index3(const base::common_types::index3& pos) -> std::string {
            return std::format("[{}, {}, {}]", pos.x, pos.y, pos.z);
        }

        auto format_cell(const mech::Element::Cell& cell) -> std::string {
            return std::format("[{}, {}, \"{}\", \"{}\"]", format_index3(cell.pose.pos), cell.pose.ori, enum_name(frame_shapes, cell.shape, "frame shape"), enum_name(slot_inners, cell.role, "inner slot"));
        }

        auto format_plate(const mech::Element::Plate& plate) -> std::string {
            return std::format("[{}, {}, \"{}\", \"{}\"]", format_index3(plate.pose.pos), plate.pose.ori, enum_name(plate_shapes, plate.shape, "plate shape"), enum_name(slot_plates, plate.role, "plate slot"));
        }

        template<typename Item, typename FormatItem>
        void append_array(std::ostringstream& out, const std::vector<Item>& items, FormatItem format_item, bool trailing_comma) {
            if (items.empty()) {
                out << "    []" << (trailing_comma ? ",\n" : "\n");
                return;
            }
            out << "    [\n";
            for (std::size_t i = 0; i < items.size(); ++i) {
                out << "        " << format_item(items[i]);
                out << (i + 1 < items.size() ? ",\n" : "\n");
            }
            out << "    ]" << (trailing_comma ? ",\n" : "\n");
        }

        auto format_blueprint(const mech::Blueprint& data) -> std::string {
            std::ostringstream out;
            out << "{\n";
            out << "    \"" << data.name << "\",\n";
            out << "    \"" << data.author << "\",\n";
            append_array(out, data.cells, format_cell, true);
            append_array(out, data.hull, format_plate, false);
            out << "}\n";
            return out.str();
        }

    } // namespace

    void Loader::Actions::load(Writing context, Id id) {
        const auto& unit = with<rmmr::resource::Unit>::get(context, id);
        const auto& loader = with<Loader>::get(context, id);
        const auto path = with<rmmr::resource::Manager>::resolve(context, unit, loader.file);

        std::ifstream input{path};
        if (not input)
            return (void)context.refuse(std::format("resource::blueprint::Loader::load: cannot open '{}'", path.string()));

        std::ostringstream buffer;
        buffer << input.rdbuf();
        try {
            auto parsed = parse_blueprint(buffer.str());
            base::message("eltanin: blueprint '{}' ← {} (cells={}, hull={})", unit.name.text(), path.string(), parsed.cells.size(), parsed.hull.size());
            with<Asset>::modify(context, id)->data = std::move(parsed);
        } catch (const std::exception& error) {
            return (void)context.refuse(std::format("resource::blueprint::Loader::load: '{}': {}", path.string(), error.what()));
        }
    }

    void Loader::Actions::save(Writing context, Id id) {
        const auto& unit = with<rmmr::resource::Unit>::get(context, id);
        const auto& loader = with<Loader>::get(context, id);
        const auto& asset = with<Asset>::get(context, id);
        const auto path = with<rmmr::resource::Manager>::resolve(context, unit, loader.file);

        std::ofstream output{path, std::ios::binary | std::ios::trunc};
        if (not output)
            return (void)context.refuse(std::format("resource::blueprint::Loader::save: cannot open '{}'", path.string()));

        try {
            output << format_blueprint(asset.data);
            if (not output)
                return (void)context.refuse(std::format("resource::blueprint::Loader::save: write failed '{}'", path.string()));
            base::message("eltanin: blueprint '{}' → {} (cells={}, hull={})", unit.name.text(), path.string(), asset.data.cells.size(), asset.data.hull.size());
        } catch (const std::exception& error) {
            return (void)context.refuse(std::format("resource::blueprint::Loader::save: '{}': {}", path.string(), error.what()));
        }
    }

}
