#pragma once
#include <algorithm>
#include <type_traits>
#include <utility>
#include "observable/subject.hpp"
#include "observable/subscription.hpp"
#include "observable/detail/type_traits.hpp"

namespace observable {

template <typename ValueType,
          typename EqualityComparator=std::equal_to<>,
          typename ...>
class value;

//! Observable values provide a way to get notified when a value-type changes.
//!
//! When setting a new value, if the new value is different than the existing one,
//! any subscribed observers will be notified.
//!
//! All methods of this class can be safely called in parallel from multiple
//! threads.
//!
//! \tparam ValueType The value-type that will be stored inside the observable.
//! \tparam EqualityComparator A comparator to use when checking if new values
//!                            are different than the stored value.
template <typename ValueType, typename EqualityComparator>
class value<ValueType, EqualityComparator>
{
public:
    //! Create a default-constructed observable value.
    //!
    //! Depending on the value type, the stored value will be either uninitialized
    //! or it will be default constructed.
    value() =default;

    //! Create an initialized observable value.
    //!
    //! \param initial_value The observable's initial value.
    //! \tparam ValueType_ Initial value type. Must be convertible to the value's
    //!                    ValueType.
    template <typename ValueType_>
    explicit value(ValueType_ && initial_value);

    //! Convert the observable value to its stored value type.
    operator ValueType const &() const noexcept;

    //! Retrieve the stored value.
    auto get() const noexcept -> ValueType const &;

    //! Subscribe to changes to the observable value.
    //!
    //! These subscriptions will be triggered whenever the stored value changes.
    //!
    //! \tparam Callable A callable taking no parameters or a callable taking one
    //!                  parameter that will be called with the new value:
    //!                  - ``void()`` -- will be called when the value changes but
    //!                    will not receive the new value.
    //!                  - ``void(T const &)`` or ``void(T)`` -- will be called
    //!                    with the new value. The expression
    //!                    ``T { value.get() }`` must be correct.
    //!
    //! \see subject::subscribe()
    template <typename Callable>
    auto subscribe(Callable && callable) -> unique_subscription;

    //! Set a new value, possibly notifying any subscribed observers.
    //!
    //! If the new value compares equal to the existing value, this method has no
    //! effect. The comparison is performed using the EqualityComparator.
    //!
    //! \param new_value The new value to set.
    //! \tparam ValueType_ The new value's actual type. Must be convertible to
    //!                    the value's ValueType.
    //! \see subject::notify()
    template <typename ValueType_>
    auto set(ValueType_ && new_value) -> void;

    //! Set a new value. Will just call set().
    //!
    //! \see set()
    template <typename ValueType_>
    auto operator=(ValueType_ && new_value) -> value &;

public:
    //! Observable values are **not** copy-constructible.
    value(value const &) =delete;

    //! Observable values are **not** copy-assignable.
    auto operator=(value const &) -> value & =delete;

    //! Observable values are move-constructible.
    value(value &&)
        noexcept(std::is_nothrow_move_constructible<ValueType>::value) =default;

    //! Observable values are move-assignable.
    auto operator=(value &&)
        noexcept(std::is_nothrow_move_assignable<ValueType>::value)
        -> value & =default;

private:
    using void_subject = subject<void()>;
    using value_subject = subject<void(ValueType const &)>;

    template <typename Callable>
    auto subscribe_impl(Callable && observer) ->
        std::enable_if_t<detail::is_compatible_with_subject<Callable,
                                                            void_subject>::value,
                         unique_subscription>
    {
        return void_observers_.subscribe(std::forward<Callable>(observer));
    }

    template <typename Callable>
    auto subscribe_impl(Callable && observer) ->
        std::enable_if_t<detail::is_compatible_with_subject<Callable,
                                                            value_subject>::value,
                         unique_subscription>
    {
        return value_observers_.subscribe(std::forward<Callable>(observer));
    }

private:
    ValueType value_;
    void_subject void_observers_;
    value_subject value_observers_;
};

//! Observable values provide a way to get notified when a value-type changes.
//!
//! \see value<ValueType, EqualityComparator>
//!
//! This specialization is exactly the same as the main value specialization, but
//! its setters are only accessible from inside the EnclosingType.
//!
//! \tparam ValueType The value-type that will be stored inside the observable.
//! \tparam EqualityComparator A comparator to use when checking if new values
//!                            are different than the stored value.
//! \tparam EnclosingType A type that will have access to the value's setters.
template <typename ValueType, typename EqualityComparator, typename EnclosingType>
class value<ValueType, EqualityComparator, EnclosingType> : public value<ValueType, EqualityComparator>
{
public:
    using value<ValueType, EqualityComparator>::value;

private:
    using value<ValueType, EqualityComparator>::set;
    using value<ValueType, EqualityComparator>::operator=;

    friend EnclosingType;
};

//! Convenience alias.
template <typename ValueType,
          typename EnclosingType,
          typename EqualityComparator=std::equal_to<>>
using property = value<ValueType, EqualityComparator, EnclosingType>;

// Implementation

template <typename ValueType, typename EqualityComparator>
template <typename ValueType_>
inline value<ValueType, EqualityComparator>::value(ValueType_ && initial_value) :
    value_ { std::forward<ValueType_>(initial_value) }
{
}

template <typename ValueType, typename EqualityComparator>
inline value<ValueType, EqualityComparator>::operator ValueType const &() const noexcept
{
    return value_;
}

template <typename ValueType, typename EqualityComparator>
inline auto value<ValueType, EqualityComparator>::get() const noexcept -> ValueType const &
{
    return value_;
}

template <typename ValueType, typename EqualityComparator>
template <typename ValueType_>
inline auto value<ValueType, EqualityComparator>::set(ValueType_ && new_value) -> void
{
    thread_local EqualityComparator const eq { };

    if(eq(new_value, value_))
        return;

    value_ = std::forward<ValueType_>(new_value);
    void_observers_.notify();
    value_observers_.notify(value_);
}

template <typename ValueType, typename EqualityComparator>
template <typename ValueType_>
inline auto value<ValueType, EqualityComparator>::operator=(ValueType_ && new_value) -> value &
{
    set(std::forward<ValueType_>(new_value));
    return *this;
}

template <typename ValueType, typename EqualityComparator>
template <typename Callable>
inline auto value<ValueType, EqualityComparator>::subscribe(Callable && callable) -> unique_subscription
{
    static_assert(detail::is_compatible_with_subject<Callable, void_subject>::value ||
                  detail::is_compatible_with_subject<Callable, value_subject>::value,
                  "Observer is not valid. Please provide a void observer or an "
                  "observer that takes a ValueType as its only argument.");

    return subscribe_impl(std::forward<Callable>(callable));
}

}
