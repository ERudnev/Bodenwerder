#pragma once

namespace fqsm::utility {

    struct Poisoned {
        template<typename T>
        operator T() const {
            alignas(T) unsigned char storage[sizeof(T)];
            std::memset(storage, 0xCD, sizeof(T));
            return *reinterpret_cast<T*>(storage);
        }
    };
}