#pragma once

#include <cstddef>
#include <iterator>
#include <memory>
#include <optional>
#include <utility>

namespace base::cannonball::delta {

template<typename Key, typename Val>
struct Change {
    const Key& id;
    std::optional<const Val*> before; // TODO: implement "Taint mode" as std::optional<const Val*>
    const Val* after;

    bool good() const { return before.has_value() || after; }
    bool tainted() const { return !before.has_value() && after; }
    bool add() const { return before.has_value() && !before.value() && after; }
    bool update() const { return before.has_value() && before.value() && after; }
    bool addedOrUpdated() const { return add() || update() || tainted(); }
    bool remove() const { return before.has_value() && before.value() && !after; }

    // experimental / library guts — not for domain reactions:
    const Val& throwing_before() const { return *(*before); }
};

// Layer-narrow views: value_type of added() / updated() / removed() / addedOrUpdated().
template<typename Key, typename Val>
struct Appeared {
    const Key& id;
    const Val& now;
};

template<typename Key, typename Val>
struct Updated {
    const Key& id;
    const Val& old;
    const Val& now;
};

template<typename Key, typename Val>
struct Gone {
    const Key& id;
    const Val& old;
};

// add, update, or tainted-with-after: always has now; old may be absent.
template<typename Key, typename Val>
struct Upserted {
    const Key& id;
    const Val* old; // nullptr if appeared / no known before
    const Val& now;
};

// Computed field event on Updated: what happened to one member (old → now).
template<typename T>
struct FieldEvent {
    const T& old;
    const T& now;
    const bool changed;
};

// optional: same poles + appeared / removed (Some↔nullopt only; Some→Some stays changed).
template<typename T>
struct FieldEvent<std::optional<T>> {
    const std::optional<T>& old;
    const std::optional<T>& now;
    const bool changed;
    const bool appeared; // !old && now
    const bool removed;  // old && !now
};

template<typename Key, typename Val, typename T>
auto field_event(const Updated<Key, Val>& change, T Val::* member) -> FieldEvent<T> {
    const T& old = change.old.*member;
    const T& now = change.now.*member;
    return FieldEvent<T>{old, now, old != now};
}

template<typename Key, typename Val, typename T>
auto field_event(const Updated<Key, Val>& change, std::optional<T> Val::* member)
    -> FieldEvent<std::optional<T>> {
    const auto& old = change.old.*member;
    const auto& now = change.now.*member;
    const bool changed = old != now;
    return FieldEvent<std::optional<T>>{
        old, now, changed,
        not old.has_value() and now.has_value(),
        old.has_value() and not now.has_value(),
    };
}

namespace detail {

    enum class NarrowKind {
        appeared,
        updated,
        gone,
        upserted,
    };

    template<NarrowKind Kind, typename Key, typename Val>
    struct narrow_result;

    template<typename Key, typename Val>
    struct narrow_result<NarrowKind::appeared, Key, Val> { using type = Appeared<Key, Val>; };
    template<typename Key, typename Val>
    struct narrow_result<NarrowKind::updated, Key, Val> { using type = Updated<Key, Val>; };
    template<typename Key, typename Val>
    struct narrow_result<NarrowKind::gone, Key, Val> { using type = Gone<Key, Val>; };
    template<typename Key, typename Val>
    struct narrow_result<NarrowKind::upserted, Key, Val> { using type = Upserted<Key, Val>; };

    template<NarrowKind Kind, typename Key, typename Val>
    using narrow_result_t = typename narrow_result<Kind, Key, Val>::type;

    template<NarrowKind Kind, typename Key, typename Val>
    auto project_as(Change<Key, Val> change) -> narrow_result_t<Kind, Key, Val> {
        if constexpr (Kind == NarrowKind::appeared)
            return Appeared<Key, Val>{change.id, *change.after};
        else if constexpr (Kind == NarrowKind::updated)
            return Updated<Key, Val>{change.id, **change.before, *change.after};
        else if constexpr (Kind == NarrowKind::gone)
            return Gone<Key, Val>{change.id, **change.before};
        else {
            const Val* old = (change.before && change.before.value()) ? change.before.value() : nullptr;
            return Upserted<Key, Val>{change.id, old, *change.after};
        }
    }

    } // detail

}
