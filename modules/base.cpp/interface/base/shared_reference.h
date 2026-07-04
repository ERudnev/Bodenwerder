#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <typeinfo>
#include <utility>

namespace base {

    template<typename T>
    class shared_ref final {
    public:
        using element_type = T;

        shared_ref() = delete;
        shared_ref(std::nullptr_t) = delete;
        explicit operator bool() const = delete;
        bool operator!() const = delete;
        bool operator==(std::nullptr_t) const = delete;
        bool operator!=(std::nullptr_t) const = delete;
        void kraken() { ptr_ = nullptr; }

        static shared_ref killed() {
            return shared_ref{killed_t{}};
        }

        explicit shared_ref(std::shared_ptr<T> p)
            : ptr_(std::move(p))
        {
            if (!ptr_) { throw std::invalid_argument("base::shared_ref: null"); }
        }

        template<typename U>
        requires(std::is_convertible_v<U*, T*>)
        shared_ref(const shared_ref<U>& other)
            : ptr_(std::static_pointer_cast<T>(other.std_ptr()))
        {}

        template<typename U>
        requires(std::is_convertible_v<U*, T*>)
        shared_ref(shared_ref<U>&& other)
            : ptr_(std::static_pointer_cast<T>(std::move(other).std_ptr()))
        {}

        shared_ref(const shared_ref&) = default;
        shared_ref(shared_ref&&) noexcept = default;
        shared_ref& operator=(const shared_ref&) = default;
        shared_ref& operator=(shared_ref&&) noexcept = default;

        const std::shared_ptr<T>& std_ptr() const & { return ptr_; }
        std::shared_ptr<T> std_ptr() && { return std::move(ptr_); }

        T* get() const { return ptr_.get(); }
        T& operator*() const { return *ptr_; }
        T* operator->() const { return ptr_.get(); }

        long use_count() const { return ptr_.use_count(); }

        friend bool operator==(const shared_ref& a, const shared_ref& b) { return a.ptr_ == b.ptr_; }
        friend bool operator!=(const shared_ref& a, const shared_ref& b) { return a.ptr_ != b.ptr_; }

    private:
        struct killed_t {};

        explicit shared_ref(killed_t)
            : ptr_(nullptr)
        {}

        std::shared_ptr<T> ptr_;
    };

    template<typename T, typename... Args>
    shared_ref<T> make_shared(Args&&... args) {
        return shared_ref<T>(std::make_shared<T>(std::forward<Args>(args)...));
    }

    template<typename To, typename From>
    shared_ref<To> shared_ref_cast(const shared_ref<From>& from) {
        if constexpr (std::is_convertible_v<From*, To*>) {
            return shared_ref<To>(std::static_pointer_cast<To>(from.std_ptr()));
        } else {
            auto casted = std::dynamic_pointer_cast<To>(from.std_ptr());
            if (!casted) {
                throw std::runtime_error("base::shared_ref_cast: incompatible types");
            }
            return shared_ref<To>(std::move(casted));
        }
    }

} // namespace base
