#include <gba/bits/link/irq_snapshot_queue.hpp>
#include <gba/testing>

int main() {
    gba::bits::link::irq_snapshot_queue<unsigned int, 2> queue;

    gba::test.is_false(queue.pop().has_value());
    gba::test.is_true(queue.push_from_irq(10));
    gba::test.is_true(queue.push_from_irq(20));
    gba::test.is_false(queue.push_from_irq(30));
    gba::test.is_true(queue.consume_overflow());
    gba::test.is_false(queue.consume_overflow());
    gba::test.eq(*queue.pop(), 10u);
    gba::test.eq(*queue.pop(), 20u);
    gba::test.is_false(queue.pop().has_value());

    gba::test.is_true(queue.push_from_irq(40));
    queue.clear();
    gba::test.is_false(queue.pop().has_value());

    return gba::test.finish();
}
