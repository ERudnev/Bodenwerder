#pragma once

#include <optional>
#include <type_traits>
#include <utility>

namespace base {
    template<typename T>
    class maybe {
        static_assert(!std::is_reference_v<T>, "base::maybe<T>: T must not be a reference");

    public:
        using value_type = T;

        constexpr maybe() noexcept = default;
        constexpr maybe(std::nullopt_t) noexcept : opt_(std::nullopt) {}

        constexpr maybe(const T& v) : opt_(v) {}
        constexpr maybe(T&& v) : opt_(std::move(v)) {}

        constexpr maybe(const std::optional<T>& o) : opt_(o) {}
        constexpr maybe(std::optional<T>&& o) noexcept(std::is_nothrow_move_constructible_v<std::optional<T>>) : opt_(std::move(o)) {}

        constexpr maybe& operator=(const T& v)
            requires(std::is_copy_constructible_v<T>)
        {
            opt_.reset();
            opt_.emplace(v);
            return *this;
        }

        constexpr maybe& operator=(T&& v) noexcept(std::is_nothrow_move_constructible_v<T>)
            requires(std::is_move_constructible_v<T>)
        {
            opt_.reset();
            opt_.emplace(std::move(v));
            return *this;
        }

        [[nodiscard]] constexpr bool exists() const noexcept { return opt_.has_value(); }
        constexpr explicit operator bool() const noexcept
            requires (!std::is_same_v<std::remove_cv_t<T>, bool>)
        {
            return exists();
        }

        constexpr operator T&() & { return opt_.value(); }
        constexpr operator const T&() const & { return opt_.value(); }
        constexpr operator T&&() && { return std::move(opt_).value(); }
        constexpr operator const T&&() const && { return std::move(opt_).value(); }

        constexpr T& value() & { return opt_.value(); }
        constexpr const T& value() const & { return opt_.value(); }
        constexpr T&& value() && { return std::move(opt_).value(); }
        constexpr const T&& value() const && { return std::move(opt_).value(); }

        constexpr T* operator->() { return &opt_.value(); }
        constexpr const T* operator->() const { return &opt_.value(); }
        constexpr T& operator*() & { return opt_.value(); }
        constexpr const T& operator*() const & { return opt_.value(); }
        constexpr T&& operator*() && { return std::move(opt_).value(); }
        constexpr const T&& operator*() const && { return std::move(opt_).value(); }

        constexpr void reset() noexcept { opt_.reset(); }

        template<typename U>
        constexpr T value_or(U&& fallback) const & {
            return opt_.value_or(static_cast<U&&>(fallback));
        }

        [[nodiscard]] constexpr const std::optional<T>& std_optional() const & { return opt_; }
        [[nodiscard]] constexpr std::optional<T>& std_optional() & { return opt_; }
        [[nodiscard]] constexpr std::optional<T>&& std_optional() && { return std::move(opt_); }

    private:
        std::optional<T> opt_;
    };
}

