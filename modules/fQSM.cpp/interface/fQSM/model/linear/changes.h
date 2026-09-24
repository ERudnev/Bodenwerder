#pragma once

#include <optional>

namespace fqsm::model::linear {

    // One delta entry. before: nullopt when tainted (mutated in place, value before unknown), nullptr when absent.
    template<typename Key, typename Val>
    struct Change {
        Key id;
        std::optional<const Val*> before;
        const Val* after;

        bool good() const { return before.has_value() or after; }
        bool tainted() const { return not before.has_value() and after; }
        bool add() const { return before.has_value() and not before.value() and after; }
        bool update() const { return before.has_value() and before.value() and after; }
        bool addedOrUpdated() const { return add() or update() or tainted(); }
        bool remove() const { return before.has_value() and before.value() and not after; }

        const Val& throwing_before() const { return *(*before); }
    };

    template<typename Key, typename Val>
    struct Appeared {
        Key id;
        const Val& now;
    };

    template<typename Key, typename Val>
    struct Updated {
        Key id;
        const Val& old;
        const Val& now;
    };

    template<typename Key, typename Val>
    struct Gone {
        Key id;
        const Val& old;
    };

    // add, update, or tainted: always has now; old is nullptr when appeared or unknown.
    template<typename Key, typename Val>
    struct Upserted {
        Key id;
        const Val* old;
        const Val& now;
    };

    template<typename T>
    struct FieldEvent {
        const T& old;
        const T& now;
        const bool changed;
    };

    // optional member: appeared / removed report Some <-> nullopt only; Some -> Some stays changed.
    template<typename T>
    struct FieldEvent<std::optional<T>> {
        const std::optional<T>& old;
        const std::optional<T>& now;
        const bool changed;
        const bool appeared;
        const bool removed;
    };

    template<typename Key, typename Val, typename T>
    auto field_event(const Updated<Key, Val>& change, T Val::* member) -> FieldEvent<T> {
        const T& old = change.old.*member;
        const T& now = change.now.*member;
        return FieldEvent<T>{old, now, old != now};
    }

    template<typename Key, typename Val, typename T>
    auto field_event(const Updated<Key, Val>& change, std::optional<T> Val::* member) -> FieldEvent<std::optional<T>> {
        const auto& old = change.old.*member;
        const auto& now = change.now.*member;
        return FieldEvent<std::optional<T>>{
            old, now, old != now,
            not old.has_value() and now.has_value(),
            old.has_value() and not now.has_value(),
        };
    }
}
