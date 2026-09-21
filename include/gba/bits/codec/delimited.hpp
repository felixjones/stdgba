/// @file bits/codec/delimited.hpp
/// @brief Fixed-capacity, predicate-terminated sequence codec (the basis for varints).
#pragma once

#include <gba/bits/codec/core.hpp>

#include <array>
#include <cstddef>
#include <optional>

namespace gba::codec {

    /// @brief A fixed-capacity, structural sequence: `Capacity` slots of `T` plus a running count.
    ///
    /// Used as `delimited_codec`'s `value_type` so that predicate-terminated
    /// sequences (in particular varints) never allocate and stay usable as a
    /// non-type template parameter when `T` is itself structural.
    template<typename T, std::size_t Capacity>
    struct bounded_sequence {
        using value_type = T;

        std::array<T, Capacity> storage{};
        std::size_t count = 0;

        static constexpr std::size_t capacity() noexcept { return Capacity; }
        [[nodiscard]] constexpr std::size_t size() const noexcept { return count; }
        [[nodiscard]] constexpr bool empty() const noexcept { return count == 0; }
        [[nodiscard]] constexpr bool full() const noexcept { return count == Capacity; }

        constexpr bool push_back(const T& value) noexcept {
            if (full()) {
                return false;
            }
            storage[count++] = value;
            return true;
        }

        constexpr T& operator[](std::size_t index) noexcept { return storage[index]; }
        constexpr const T& operator[](std::size_t index) const noexcept { return storage[index]; }

        constexpr auto begin() noexcept { return storage.begin(); }
        constexpr auto end() noexcept { return storage.begin() + count; }
        constexpr auto begin() const noexcept { return storage.begin(); }
        constexpr auto end() const noexcept { return storage.begin() + count; }

        friend constexpr bool operator==(const bounded_sequence& a, const bounded_sequence& b) noexcept {
            if (a.count != b.count) {
                return false;
            }
            for (std::size_t i = 0; i < a.count; ++i) {
                if (!(a.storage[i] == b.storage[i])) {
                    return false;
                }
            }
            return true;
        }
    };

    /// @brief Decodes elements one at a time until `Predicate` reports the
    /// most recently decoded element as the last one (inclusive), or
    /// `MaxElements` is reached first.
    ///
    /// `Predicate` is `bool(const ElemCodec::value_type&)`. Never allocates:
    /// the result is a `bounded_sequence<ElemCodec::value_type, MaxElements>`.
    /// This is the combinator `unsigned_varint_codec`/`signed_varint_codec`
    /// are built from.
    template<Codec ElemCodec, typename Predicate, std::size_t MaxElements>
    struct delimited_codec {
        using value_type = bounded_sequence<typename ElemCodec::value_type, MaxElements>;

        ElemCodec element{};
        Predicate predicate{};

        struct decoder {
            ElemCodec element;
            Predicate predicate;
            value_type result{};
            std::optional<typename ElemCodec::decoder> current{};
            bool finished = false;
            bool element_invalid = false;
            bool overflowed = false;

            constexpr decoder(const ElemCodec& e, const Predicate& p) : element(e), predicate(p) {
                current.emplace(element.make_decoder());
                advance();
            }

            constexpr void advance() noexcept {
                while (!finished && !element_invalid && !overflowed && current.has_value() &&
                       current->state() == step::done) {
                    const auto v = current->value();
                    if (!result.push_back(v)) {
                        overflowed = true;
                        return;
                    }
                    if (predicate(v)) {
                        finished = true;
                        return;
                    }
                    current.emplace(element.make_decoder());
                }
            }

            [[nodiscard]] constexpr step state() const noexcept {
                if (element_invalid || overflowed) {
                    return step::error;
                }
                if (finished) {
                    return step::done;
                }
                return (current.has_value() && current->state() == step::error) ? step::error : step::more;
            }

            constexpr step feed(std::byte b) noexcept {
                if (element_invalid || overflowed || finished) {
                    return step::error;
                }
                const auto s = current->feed(b);
                if (s == step::error) {
                    element_invalid = true;
                    return step::error;
                }
                if (s == step::done) {
                    advance();
                }
                return state();
            }

            constexpr value_type value() const noexcept { return result; }

            [[nodiscard]] constexpr error error_reason() const noexcept {
                return overflowed ? error::overflow : error::invalid;
            }
        };

        struct encoder {
            ElemCodec element;
            value_type values;
            std::size_t index = 0;
            std::optional<typename ElemCodec::encoder> current{};

            constexpr encoder(const ElemCodec& e, const value_type& v) : element(e), values(v) { activate(); }

            constexpr void activate() noexcept {
                if (index < values.size()) {
                    current.emplace(element.make_encoder(values[index]));
                }
            }

            constexpr std::optional<std::byte> next() noexcept {
                while (index < values.size()) {
                    if (const auto b = current->next()) {
                        return b;
                    }
                    ++index;
                    activate();
                }
                return std::nullopt;
            }
        };

        constexpr decoder make_decoder() const noexcept { return decoder(element, predicate); }
        constexpr encoder make_encoder(const value_type& value) const noexcept { return encoder(element, value); }
    };

    /// @brief Build a `delimited_codec<ElemCodec, Predicate, MaxElements>` from an element codec and a
    /// last-element predicate.
    template<std::size_t MaxElements, Codec ElemCodec, typename Predicate>
    constexpr auto delimited(ElemCodec element, Predicate predicate) {
        return delimited_codec<ElemCodec, Predicate, MaxElements>{element, predicate};
    }

} // namespace gba::codec
