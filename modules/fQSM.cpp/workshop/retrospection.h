#pragma once

#include <memory>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <vector>

#include "sqlPolicy.h"

namespace placeholder {

    namespace detail::retrospection {

        inline auto qualify(std::string_view lhs, std::string_view rhs) -> std::string {
            return std::string{lhs} + "." + std::string{rhs};
        }

        struct PathStep {
            std::string_view name;
            const void* (*project)(const void*) = nullptr;
            void* (*projectMutable)(void*) = nullptr;
        };

        inline auto descend(const void* source, const std::vector<PathStep>& path) -> const void* {
            const void* cursor = source;
            for (const auto step : path)
                cursor = step.project(cursor);
            return cursor;
        }

        inline auto descend(void* source, const std::vector<PathStep>& path) -> void* {
            void* cursor = source;
            for (const auto& step : path)
                cursor = step.projectMutable(cursor);
            return cursor;
        }

        inline auto name_of(const std::vector<PathStep>& path) -> std::string {
            std::string out;
            for (std::size_t index = 0; index < path.size(); ++index) {
                if (index != 0) out += '.';
                out += path[index].name;
            }
            return out;
        }

        template<auto Member>
        struct member_pointer;

        template<typename Owner, typename Field, Field Owner::*Member>
        struct member_pointer<Member> {
            static auto project(const void* source) -> const void* {
                return std::addressof((static_cast<const Owner*>(source)->*Member));
            }

            static auto projectMutable(void* source) -> void* {
                return std::addressof((static_cast<Owner*>(source)->*Member));
            }
        };

    }

    struct Retrospection {
        using StorageAtom = detail::sql_policy::StorageAtom;
        enum struct Persistence {
            scalar_column,
            collection_table,
            ignored,
        };

        struct Field {
            std::string fieldPath;
            Persistence persistence;
            StorageAtom atom;
            std::string targetAspect;
            std::vector<detail::retrospection::PathStep> path;
            void (*bindLeaf)(sqlite3_stmt*, int, const void*) = nullptr;
            void (*readLeaf)(sqlite3_stmt*, int, void*) = nullptr;
            std::size_t (*countElements)(const void*) = nullptr;
            const void* (*elementAt)(const void*, std::size_t) = nullptr;
            void (*appendElement)(sqlite3_stmt*, int, void*) = nullptr;
        };

        struct Quanta {
            std::vector<Field> fields;
        };

        struct Globals {
            std::vector<Field> fields;
        };

        std::string aspectName;
        Quanta quanta;
        std::vector<Field> collections;
        std::optional<Globals> globals;

        template<auto Member>
        static auto step(std::string_view name) -> detail::retrospection::PathStep {
            return {
                .name = name,
                .project = &detail::retrospection::member_pointer<Member>::project,
                .projectMutable = &detail::retrospection::member_pointer<Member>::projectMutable,
            };
        }

        auto quantaTable() const -> std::string {
            return detail::retrospection::qualify(aspectName, "table");
        }

        auto globalsTable() const -> std::string {
            return detail::retrospection::qualify(aspectName, "globals");
        }

        auto collectionTable(const Field& field) const -> std::string {
            return detail::retrospection::qualify(aspectName, field.fieldPath);
        }

        template<typename Leaf>
        static auto column(std::string fieldPath, std::initializer_list<detail::retrospection::PathStep> path) -> Field {
            detail::sql_policy::atom<Leaf>::require();
            return {
                .fieldPath = std::move(fieldPath),
                .persistence = Persistence::scalar_column,
                .atom = detail::sql_policy::atom<Leaf>::storage,
                .targetAspect = {},
                .path = path,
                .bindLeaf = detail::sql_policy::atom<Leaf>::bind,
                .readLeaf = detail::sql_policy::atom<Leaf>::read,
            };
        }

        template<typename Leaf>
        static auto column(std::initializer_list<detail::retrospection::PathStep> path) -> Field {
            return column<Leaf>(detail::retrospection::name_of(std::vector<detail::retrospection::PathStep>{path}), path);
        }

        template<typename Reference>
        static auto referenceColumn(std::string fieldPath, std::string targetAspect, std::initializer_list<detail::retrospection::PathStep> path) -> Field {
            detail::sql_policy::atom<Reference>::require();
            static_assert(detail::sql_policy::atom<Reference>::storage == StorageAtom::reference, "referenceColumn requires reference-like SQL atom");
            return {
                .fieldPath = std::move(fieldPath),
                .persistence = Persistence::scalar_column,
                .atom = detail::sql_policy::atom<Reference>::storage,
                .targetAspect = std::move(targetAspect),
                .path = path,
                .bindLeaf = detail::sql_policy::atom<Reference>::bind,
                .readLeaf = detail::sql_policy::atom<Reference>::read,
            };
        }

        template<typename Reference>
        static auto referenceColumn(std::string targetAspect, std::initializer_list<detail::retrospection::PathStep> path) -> Field {
            return referenceColumn<Reference>(detail::retrospection::name_of(std::vector<detail::retrospection::PathStep>{path}), std::move(targetAspect), path);
        }

        static auto ignored(std::string fieldPath, std::initializer_list<detail::retrospection::PathStep> path) -> Field {
            return {
                .fieldPath = std::move(fieldPath),
                .persistence = Persistence::ignored,
                .atom = StorageAtom::string,
                .targetAspect = {},
                .path = path,
                .bindLeaf = nullptr,
                .readLeaf = nullptr,
                .countElements = nullptr,
                .elementAt = nullptr,
                .appendElement = nullptr,
            };
        }

        static auto ignored(std::initializer_list<detail::retrospection::PathStep> path) -> Field {
            return ignored(detail::retrospection::name_of(std::vector<detail::retrospection::PathStep>{path}), path);
        }

        template<typename SequenceType>
        static auto referenceCollection(
            std::string targetAspect,
            std::initializer_list<detail::retrospection::PathStep> path
        ) -> Field {
            detail::sql_policy::sequence<SequenceType>::require();
            static_assert(detail::sql_policy::sequence<SequenceType>::storage == StorageAtom::reference, "referenceSequence requires reference-like sequence values");
            return {
                .fieldPath = detail::retrospection::name_of(std::vector<detail::retrospection::PathStep>{path}),
                .persistence = Persistence::collection_table,
                .atom = detail::sql_policy::sequence<SequenceType>::storage,
                .targetAspect = std::move(targetAspect),
                .path = path,
                .bindLeaf = detail::sql_policy::sequence<SequenceType>::bind,
                .readLeaf = detail::sql_policy::sequence<SequenceType>::read,
                .countElements = detail::sql_policy::sequence<SequenceType>::count,
                .elementAt = detail::sql_policy::sequence<SequenceType>::element,
                .appendElement = detail::sql_policy::sequence<SequenceType>::append,
            };
        }
    };

}