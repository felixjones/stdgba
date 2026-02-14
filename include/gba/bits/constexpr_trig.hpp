#pragma once

#include <utility>

namespace gba::bits {

    // Compute sine and cosine from a 16-bit GBA angle (0..65535 -> 0..2*pi)
    // This is a small, constexpr-friendly implementation using quadrant reduction and
    // a short Taylor series on [0, pi/2]. Returns {sin, cos} as long double values.
    consteval std::pair<long double, long double> sin_cos(unsigned int alpha16) {
        constexpr long double PI = 3.14159265358979323846L;
        constexpr long double PI_OVER_32768 = PI / 32768.0L; // theta = alpha16 * PI_OVER_32768

        alpha16 &= 0xFFFFu;

        const unsigned int quadrant = (alpha16 >> 14) & 3u; // 0..3
        const unsigned int r = alpha16 & 0x3FFFu;           // remainder within quadrant (0..16383)

        // t in [0, PI/2)
        const long double t = static_cast<long double>(r) * PI_OVER_32768;
        const long double x = t;
        const long double x2 = x * x;

        // sin(t) approx: x - x^3/6 + x^5/120 - x^7/5040
        long double s = x;
        long double term = x;
        term *= -x2; // -x^3
        s += term / 6.0L;
        term *= -x2; // x^5
        s += term / 120.0L;
        term *= -x2; // -x^7
        s += term / 5040.0L;

        // cos(t) approx: 1 - x^2/2 + x^4/24 - x^6/720
        long double c = 1.0L;
        long double termc = 1.0L;
        termc *= -x2; // -x^2
        c += termc / 2.0L;
        termc *= -x2; // x^4
        c += termc / 24.0L;
        termc *= -x2; // -x^6
        c += termc / 720.0L;

        long double sin_res, cos_res;
        switch (quadrant) {
            case 0:
                sin_res = s;
                cos_res = c;
                break;
            case 1:
                sin_res = c;
                cos_res = -s;
                break;
            case 2:
                sin_res = -s;
                cos_res = -c;
                break;
            default:
                sin_res = -c;
                cos_res = s;
                break; // quadrant 3
        }
        return {sin_res, cos_res};
    }

    // Compute tangent from a 16-bit GBA angle
    consteval long double tan(unsigned int alpha16) {
        const auto [sine, cosine] = sin_cos(alpha16);
        if (cosine == 0.0L) {
            return (sine >= 0.0L) ? 1e20L : -1e20L; // large value for undefined tangent
        }
        return sine / cosine;
    }

} // namespace gba::bits
