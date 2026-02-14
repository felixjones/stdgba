#pragma once

#include <algorithm>
#include <memory>
#include <vector>

namespace gba::bits {

    template<typename T>
    struct min_heap {
        std::vector<std::unique_ptr<T>> heap;

        template<typename... Args>
        consteval void emplace(Args&&... args) {
            heap.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
            std::push_heap(heap.begin(), heap.end(), [](const auto& a, const auto& b) { return *a > *b; });
        }

        consteval std::unique_ptr<T> pop() {
            std::pop_heap(heap.begin(), heap.end(), [](const auto& a, const auto& b) { return *a > *b; });
            auto node = std::move(heap.back());
            heap.pop_back();
            return node;
        }

        consteval const auto& front() const { return heap.front(); }

        [[nodiscard]]
        consteval bool empty() const {
            return heap.empty();
        }

        [[nodiscard]]
        consteval std::size_t size() const {
            return heap.size();
        }
    };

    struct huffman_node {
        consteval huffman_node(const std::uint8_t s, const std::size_t f) : symbol{s}, frequency{f} {}

        consteval huffman_node(std::unique_ptr<huffman_node> l, std::unique_ptr<huffman_node> r)
            : symbol{}, frequency{l->frequency + r->frequency}, left{std::move(l)}, right{std::move(r)} {}

        consteval bool operator>(const huffman_node& other) const {
            if (frequency == other.frequency) {
                return symbol < other.symbol;
            }
            return frequency > other.frequency;
        }

        [[nodiscard]]
        consteval bool is_leaf() const {
            return left == nullptr && right == nullptr;
        }

        static consteval void traverse(const huffman_node& node, auto callable) {
            callable(node);
            if (!node.left->is_leaf()) {
                traverse(*node.left, callable);
            }
            if (!node.right->is_leaf()) {
                traverse(*node.right, callable);
            }
        }

        static consteval void to_keys(const huffman_node& node, auto callable, const std::vector<bool>& code = {}) {
            if (node.is_leaf()) {
                callable(node.symbol, code);
                return;
            }

            std::vector<bool> codeLeft = code;
            codeLeft.push_back(false);
            to_keys(*node.left, callable, codeLeft);

            std::vector<bool> codeRight = code;
            codeRight.push_back(true);
            to_keys(*node.right, callable, codeRight);
        }

        std::uint8_t symbol;
        std::size_t frequency;
        std::unique_ptr<huffman_node> left;
        std::unique_ptr<huffman_node> right;
    };

} // namespace gba::bits
