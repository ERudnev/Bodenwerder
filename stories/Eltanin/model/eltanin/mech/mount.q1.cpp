#include <eltanin/mech/mount.q1.h>

#include <rmmr/resources/manager.q1.h>
#include <rmmr/system/content/unit_name.h>

#include <cctype>
#include <format>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace eltanin::mech {

    using namespace fqsm::api;

    namespace {

        struct Cursor {
            std::string_view text;
            std::size_t at;
        };

        void skip_ws(Cursor& cursor) {
            while (cursor.at < cursor.text.size()) {
                const auto ch = cursor.text[cursor.at];
                if (std::isspace(static_cast<unsigned char>(ch))) {
                    ++cursor.at;
                    continue;
                }
                if (ch == '/' and cursor.at + 1 < cursor.text.size() and cursor.text[cursor.at + 1] == '/') {
                    cursor.at += 2;
                    while (cursor.at < cursor.text.size() and cursor.text[cursor.at] != '\n')
                        ++cursor.at;
                    continue;
                }
                break;
            }
        }

        void expect(Cursor& cursor, char ch) {
            skip_ws(cursor);
            if (cursor.at >= cursor.text.size() or cursor.text[cursor.at] != ch)
                throw std::runtime_error(std::format("mount: expected '{}'", ch));
            ++cursor.at;
        }

        auto peek(Cursor& cursor) -> char {
            skip_ws(cursor);
            return cursor.at < cursor.text.size() ? cursor.text[cursor.at] : '\0';
        }

        auto take_string(Cursor& cursor) -> string {
            skip_ws(cursor);
            expect(cursor, '"');
            string out;
            while (cursor.at < cursor.text.size() and cursor.text[cursor.at] != '"') {
                if (cursor.text[cursor.at] == '\\' and cursor.at + 1 < cursor.text.size()) {
                    ++cursor.at;
                    out.push_back(cursor.text[cursor.at]);
                    ++cursor.at;
                    continue;
                }
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
                throw std::runtime_error("mount: expected integer");
            return static_cast<integer>(std::stoi(std::string{cursor.text.substr(begin, cursor.at - begin)}));
        }

        auto take_key(Cursor& cursor) -> string {
            const auto key = take_string(cursor);
            expect(cursor, ':');
            return key;
        }

        void expect_key(Cursor& cursor, std::string_view key) {
            if (take_key(cursor) != key)
                throw std::runtime_error(std::format("mount: expected key '{}'", key));
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

        auto take_attachment(Cursor& cursor) -> Attachment {
            expect(cursor, '{');
            expect_key(cursor, "points");
            expect(cursor, '[');
            Attachment attachment{.points = {}};
            if (peek(cursor) != ']') {
                for (;;) {
                    attachment.points.push_back(take_index3(cursor));
                    skip_ws(cursor);
                    if (peek(cursor) == ']')
                        break;
                    expect(cursor, ',');
                }
            }
            expect(cursor, ']');
            expect(cursor, '}');
            return attachment;
        }

        auto take_temp_mesh(Cursor& cursor) -> Mount::TempMesh {
            expect(cursor, '{');
            expect_key(cursor, "pack");
            const auto packText = take_string(cursor);
            const auto parsed = rmmr::system::content::UnitName::parse(packText);
            if (not parsed)
                throw std::runtime_error(std::format("mount: bad pack Unit::Name '{}'", packText));
            expect(cursor, ',');
            expect_key(cursor, "entry");
            const auto entry = take_string(cursor);
            expect(cursor, '}');
            return Mount::TempMesh{
                .pack = rmmr::resource::Unit::Name::from(parsed->library, parsed->own),
                .entry = entry,
            };
        }

        auto parse_mount(std::string_view text) -> Mount::Quantum {
            Cursor cursor{.text = text, .at = 0};
            expect(cursor, '{');
            expect_key(cursor, "name");
            auto name = take_string(cursor);
            expect(cursor, ',');
            expect_key(cursor, "author");
            auto author = take_string(cursor);
            expect(cursor, ',');
            expect_key(cursor, "attachment");
            auto attachment = take_attachment(cursor);
            expect(cursor, ',');
            expect_key(cursor, "tempMesh");
            auto tempMesh = take_temp_mesh(cursor);
            expect(cursor, '}');
            return Mount::Quantum{
                .name = std::move(name),
                .author = std::move(author),
                .attachment = std::move(attachment),
                .tempMesh = std::move(tempMesh),
                .file = {},
            };
        }

        auto format_mount(const Mount::Quantum& data) -> std::string {
            std::ostringstream out;
            out << "{\n";
            out << "  \"name\": \"" << data.name << "\",\n";
            out << "  \"author\": \"" << data.author << "\",\n";
            out << "  \"attachment\": {\n";
            out << "    \"points\": [\n";
            for (std::size_t i = 0; i < data.attachment.points.size(); ++i) {
                const auto& point = data.attachment.points[i];
                out << "      [" << point.x << ", " << point.y << ", " << point.z << "]" << (i + 1 < data.attachment.points.size() ? ",\n" : "\n");
            }
            out << "    ]\n";
            out << "  },\n";
            out << "  \"tempMesh\": {\n";
            out << "    \"pack\": \"" << data.tempMesh.pack.text() << "\",\n";
            out << "    \"entry\": \"" << data.tempMesh.entry << "\"\n";
            out << "  }\n";
            out << "}\n";
            return out.str();
        }

    } // namespace

    void Mount::Actions::load(Writing context, Id id) {
        const auto& unit = with<rmmr::resource::Unit>::get(context, id);
        const auto& mount = with<Mount>::get(context, id);
        const auto path = with<rmmr::resource::Manager>::resolve(context, unit, mount.file);

        std::ifstream input{path};
        if (not input)
            return (void)context.refuse(std::format("mech::Mount::load: cannot open '{}'", path.string()));

        std::ostringstream buffer;
        buffer << input.rdbuf();
        try {
            auto parsed = parse_mount(buffer.str());
            auto writable = with<Mount>::modify(context, id);
            const auto file = writable->file;
            *writable = std::move(parsed);
            writable->file = file;
        } catch (const std::exception& error) {
            return (void)context.refuse(std::format("mech::Mount::load: '{}': {}", path.string(), error.what()));
        }
    }

    void Mount::Actions::save(Writing context, Id id) {
        const auto& unit = with<rmmr::resource::Unit>::get(context, id);
        const auto& mount = with<Mount>::get(context, id);
        const auto path = with<rmmr::resource::Manager>::resolve(context, unit, mount.file);

        std::ofstream output{path, std::ios::binary | std::ios::trunc};
        if (not output)
            return (void)context.refuse(std::format("mech::Mount::save: cannot open '{}'", path.string()));

        try {
            output << format_mount(mount);
            if (not output)
                return (void)context.refuse(std::format("mech::Mount::save: write failed '{}'", path.string()));
        } catch (const std::exception& error) {
            return (void)context.refuse(std::format("mech::Mount::save: '{}': {}", path.string(), error.what()));
        }
    }

}
