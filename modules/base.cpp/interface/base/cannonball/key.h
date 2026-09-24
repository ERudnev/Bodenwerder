#pragma once

#include <type_traits>

namespace base::cannonball {

// Table's index is keyed on RawKey<Key>::type rather than Key itself, so that all
// Identifier<Meta> tables (one raw() type, many Meta) share one unordered_map instantiation.
template<typename Key>
struct RawKey {
    using type = Key;
    static const Key& of(const Key& key) { return key; }
};

template<typename Key>
    requires requires (const Key& k) { k.raw(); }
struct RawKey<Key> {
    using type = std::decay_t<decltype(std::declval<const Key&>().raw())>;
    static type of(const Key& key) { return key.raw(); }
};

} // namespace base::cannonball
