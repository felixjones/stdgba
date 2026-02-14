#pragma once

#include <cstdint>
#include <functional>
#include <utility>

namespace gba {
    struct irq;
}

namespace gba::bits {

    template<typename... Args>
    struct handler {
        constexpr handler() noexcept : m_callable{nullptr} {};

        template<typename T>
        explicit(false) handler(T&& t) noexcept; // NOLINT(*-forwarding-reference-overload, *-explicit-constructor)

        handler(const handler& other) noexcept;

        handler(handler&& other) noexcept;

        constexpr ~handler() noexcept;

        handler& operator=(const handler& other) noexcept;

        handler& operator=(handler&& other) noexcept;

        void operator()(Args... args) const noexcept {
            m_callable->invoke(std::forward<Args>(args)...);
        }

        constexpr bool operator==(const handler& other) const noexcept {
            return m_callable == other.m_callable;
        }

    private:
        static constexpr auto storage_size = 12u;

        using storage_type = std::array<std::byte, storage_size>;

        struct callable_base {
            callable_base() = default;
            virtual ~callable_base() = default;
            virtual void invoke(Args&&... args) = 0;
            virtual callable_base* clone(storage_type& storage) = 0;
            virtual callable_base* move_to(storage_type& storage) = 0;
        };

        template<typename Callable>
        struct callable final : callable_base {
            explicit callable(Callable callable) noexcept : m_callable{std::move(callable)} {}
            void invoke(Args&&... args) override { m_callable(std::forward<Args>(args)...); }

            callable_base* clone(storage_type& storage) override;
            callable_base* move_to(storage_type& storage) override;

            Callable m_callable;
        };

        static_assert(sizeof(callable<void(*)(Args...)>) <= storage_size);

        callable_base* m_callable;
        storage_type m_storage{};

        [[nodiscard]]
        constexpr void* storage() noexcept { return std::addressof(m_storage); }
    };

    static_assert(sizeof(handler<short>) == 16u);

    struct isr {
        using value_type = handler<irq>;

        const isr& operator=(const value_type& value) const noexcept; // NOLINT(*-unconventional-assign-operator)

        explicit operator bool() const noexcept { return has_value(); }

        [[nodiscard]] bool has_value() const noexcept;
        [[nodiscard]] const value_type& value() const;

        void swap(value_type& value) const noexcept;
        void reset() const noexcept;

        bool operator==(const value_type& value) const noexcept;
    };

}

#include "handler.tpp"
