#pragma once

#include "handler.hpp"

namespace gba::bits {

    template<typename ... Args>
    handler<Args...>& handler<Args...>::operator=(const handler& other) noexcept {
        if (this == &other) [[unlikely]] {
            return *this;
        }

        if (m_callable == storage()) {
            m_callable->~callable_base();
        } else {
            delete m_callable;
        }

        if (other.m_callable) {
            m_callable = other.m_callable->clone(m_storage);
        } else {
            m_callable = nullptr;
        }
        return *this;
    }

    template<typename ... Args>
    handler<Args...>& handler<Args...>::operator=(handler&& other) noexcept {
        if (this == &other) [[unlikely]] {
            return *this;
        }

        if (m_callable == storage()) {
            m_callable->~callable_base();
        } else {
            delete m_callable;
        }

        if (other.m_callable == other.storage()) {
            m_callable = other.m_callable->move_to(m_storage);
            std::exchange(other.m_callable, nullptr)->~callable_base();
        } else if (other.m_callable) {
            m_callable = std::exchange(other.m_callable, nullptr);
        } else {
            m_callable = nullptr;
        }
        return *this;
    }

    template<typename ... Args>
    template<typename T>
    handler<Args...>::handler(T&& t) noexcept { // NOLINT(*-forwarding-reference-overload)
        if constexpr (sizeof(*new callable(t)) <= storage_size) {
            m_callable = new(storage()) callable(t);
        } else {
            m_callable = new callable(t);
        }
    }

    template<typename ... Args>
    handler<Args...>::handler(const handler& other) noexcept {
        if (other.m_callable) {
            m_callable = other.m_callable->clone(m_storage);
        } else {
            m_callable = nullptr;
        }
    }

    template<typename ... Args>
    handler<Args...>::handler(handler&& other) noexcept {
        if (other.m_callable == other.storage()) {
            m_callable = other.m_callable->move_to(m_storage);
            std::exchange(other.m_callable, nullptr)->~callable_base();
        } else {
            m_callable = std::exchange(other.m_callable, nullptr);
        }
    }

    template<typename ... Args>
    constexpr handler<Args...>::~handler() noexcept {
        if (m_callable == storage()) {
            m_callable->~callable_base();
        } else {
            delete m_callable;
        }
    }

    template<typename ... Args>
    template<typename Callable>
    typename handler<Args...>::callable_base* handler<Args...>::callable<Callable>::clone(storage_type& storage) {
        if constexpr (sizeof(*this) <= sizeof(storage)) {
            return new(std::addressof(storage)) callable(m_callable);
        } else {
            return new callable(m_callable);
        }
    }

    template<typename ... Args>
    template<typename Callable>
    typename handler<Args...>::callable_base* handler<Args...>::callable<Callable>::move_to(storage_type& storage) {
        if constexpr (sizeof(*this) <= sizeof(storage)) {
            return new(std::addressof(storage)) callable(std::move(m_callable));
        } else {
            return nullptr; // should not be called
        }
    }

}
