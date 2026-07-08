#pragma once

#include <type_traits>
#include <utility>

namespace base {

// Non-owning type-erased callable. Valid only for the lifetime of the bound callable.
template<typename Sig>
class function_ref;

template<typename Ret, typename... Args>
class function_ref<Ret(Args...)> final {
public:
    function_ref() = delete;

    template<typename Callable>
        requires std::is_invocable_r_v<Ret, Callable, Args...>
    function_ref(Callable&& callable) noexcept
        : storage_(std::addressof(callable))
        , invoke_(&thunk<std::remove_reference_t<Callable>>)
    {}

    Ret operator()(Args... args) const {
        return invoke_(storage_, std::forward<Args>(args)...);
    }

private:
    using invoke_type = Ret (*)(void*, Args...);

    template<typename Callable>
    static Ret thunk(void* storage, Args... args) {
        return (*static_cast<Callable*>(storage))(std::forward<Args>(args)...);
    }

    void* storage_;
    invoke_type invoke_;
};

} // namespace base
