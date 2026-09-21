#include <gba/bits/link/error.hpp>
#include <gba/testing>

int main() {
    using gba::bits::link::link_error;
    using gba::bits::link::link_error_state;

    link_error_state errors;
    gba::test.is_false(errors.any());
    gba::test.eq(errors.value(), link_error::none);

    errors.set(link_error::receive_overflow);
    errors.set(link_error::integrity_failure);
    gba::test.is_true(errors.any());
    gba::test.is_true(errors.has(link_error::receive_overflow));
    gba::test.is_true(errors.has(link_error::integrity_failure));
    gba::test.is_false(errors.has(link_error::decode_failure));

    errors.clear(link_error::receive_overflow);
    gba::test.is_false(errors.has(link_error::receive_overflow));
    gba::test.is_true(errors.has(link_error::integrity_failure));

    errors.clear();
    gba::test.is_false(errors.any());

    return gba::test.finish();
}
