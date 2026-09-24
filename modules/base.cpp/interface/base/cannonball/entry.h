#pragma once

namespace base::cannonball {

template<typename Key, typename Val>
struct Entry {
    Key id;
    Val value;
};

template<typename Key, typename Val>
struct EntryView {
    const Key& id;
    const Val& value;
};

template<typename Key, typename Val>
struct EntryRef {
    const Key& id;
    Val& value;
};

} // namespace base::cannonball
