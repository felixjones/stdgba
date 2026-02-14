#pragma once

#include <array>
#include <cstddef>
#include <new>
#include <type_traits>
#include <utility>

namespace gba {
    struct irq;
} // namespace gba

namespace gba::bits {

    /**
     * @brief Type-erased callable wrapper optimized for embedded systems.
     *
     * Uses function pointers instead of virtual dispatch for lower overhead.
     * Supports small buffer optimization for callables up to 12 bytes.
     *
     * @tparam Args The argument types for the callable.
     */
    template<typename... Args>
    struct handler {
        constexpr handler() noexcept : m_invoke{nullptr}, m_ops{nullptr}, m_storage{} {}

        template<typename T>
            requires(!std::is_same_v<std::decay_t<T>, handler>)
        explicit(false) handler(T&& t) noexcept;

        handler(const handler& other) noexcept;
        handler(handler&& other) noexcept;

        constexpr ~handler() noexcept {
            if (m_ops) {
                m_ops->destroy(storage());
            }
        }

        handler& operator=(const handler& other) noexcept;
        handler& operator=(handler&& other) noexcept;

        void operator()(Args... args) const noexcept {
            m_invoke(const_cast<void*>(static_cast<const void*>(storage())), std::forward<Args>(args)...);
        }

        constexpr bool operator==(const handler& other) const noexcept {
            return m_invoke == other.m_invoke && m_ops == other.m_ops;
        }

    private:
        static constexpr std::size_t storage_size = 12u;
        using storage_type = std::array<std::byte, storage_size>;

        // Function pointer types for type-erased operations
        using invoke_fn = void(*)(void*, Args...);

        struct ops_table {
            void (*destroy)(void*) noexcept;
            void (*copy)(void* dst, const void* src);
            void (*move)(void* dst, void* src) noexcept;
        };

        template<typename Callable>
        static constexpr ops_table make_ops_table() noexcept {
            return {
                // destroy
                [](void* p) noexcept {
                    static_cast<Callable*>(p)->~Callable();
                },
                // copy
                [](void* dst, const void* src) {
                    new (dst) Callable(*static_cast<const Callable*>(src));
                },
                // move
                [](void* dst, void* src) noexcept {
                    new (dst) Callable(std::move(*static_cast<Callable*>(src)));
                }
            };
        }

        template<typename Callable>
        static void invoke_impl(void* ctx, Args... args) {
            (*static_cast<Callable*>(ctx))(std::forward<Args>(args)...);
        }

        template<typename Callable>
        static constexpr const ops_table* get_ops() noexcept {
            static constexpr ops_table table = make_ops_table<Callable>();
            return &table;
        }

        invoke_fn m_invoke;
        const ops_table* m_ops;
        storage_type m_storage;

        [[nodiscard]] constexpr void* storage() noexcept { return m_storage.data(); }
        [[nodiscard]] constexpr const void* storage() const noexcept { return m_storage.data(); }
    };

    static_assert(sizeof(handler<short>) == 20u); // invoke ptr + ops ptr + 12 bytes storage

    // Implementation

    template<typename... Args>
    template<typename T>
        requires(!std::is_same_v<std::decay_t<T>, handler<Args...>>)
    handler<Args...>::handler(T&& t) noexcept {
        using Callable = std::decay_t<T>;
        static_assert(sizeof(Callable) <= storage_size, "Callable too large for small buffer optimization");
        static_assert(std::is_nothrow_move_constructible_v<Callable>, "Callable must be nothrow move constructible");

        new (storage()) Callable(std::forward<T>(t));
        m_invoke = &invoke_impl<Callable>;
        m_ops = get_ops<Callable>();
    }

    template<typename... Args>
    handler<Args...>::handler(const handler& other) noexcept : m_invoke{other.m_invoke}, m_ops{other.m_ops}, m_storage{} {
        if (m_ops) {
            m_ops->copy(storage(), other.storage());
        }
    }

    template<typename... Args>
    handler<Args...>::handler(handler&& other) noexcept : m_invoke{other.m_invoke}, m_ops{other.m_ops}, m_storage{} {
        if (m_ops) {
            m_ops->move(storage(), other.storage());
            other.m_ops->destroy(other.storage());
            other.m_invoke = nullptr;
            other.m_ops = nullptr;
        }
    }

    template<typename... Args>
    handler<Args...>& handler<Args...>::operator=(const handler& other) noexcept {
        if (this != &other) {
            if (m_ops) {
                m_ops->destroy(storage());
            }
            m_invoke = other.m_invoke;
            m_ops = other.m_ops;
            if (m_ops) {
                m_ops->copy(storage(), other.storage());
            }
        }
        return *this;
    }

    template<typename... Args>
    handler<Args...>& handler<Args...>::operator=(handler&& other) noexcept {
        if (this != &other) {
            if (m_ops) {
                m_ops->destroy(storage());
            }
            m_invoke = other.m_invoke;
            m_ops = other.m_ops;
            if (m_ops) {
                m_ops->move(storage(), other.storage());
                other.m_ops->destroy(other.storage());
                other.m_invoke = nullptr;
                other.m_ops = nullptr;
            }
        }
        return *this;
    }

    struct isr {
        using value_type = handler<irq>;

        const isr& operator=(const value_type& value) const noexcept;

        explicit operator bool() const noexcept { return has_value(); }

        [[nodiscard]] bool has_value() const noexcept;
        [[nodiscard]] const value_type& value() const;

        void swap(value_type& value) const noexcept;
        void reset() const noexcept;

        bool operator==(const value_type& value) const noexcept;
    };

} // namespace gba::bits
