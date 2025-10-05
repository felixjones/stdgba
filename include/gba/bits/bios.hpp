#pragma once

namespace gba::bits {

    consteval short arc_tan(const int i) {
        // From https://github.com/mgba-emu/mgba/blob/8c2e2e1d4628e8794fa547a9acce5d18fe8f585a/src/gba/bios.c#L285
        /* Copyright (c) 2013-2015 Jeffrey Pfau
         *
         * This Source Code Form is subject to the terms of the Mozilla Public
         * License, v. 2.0. If a copy of the MPL was not distributed with this
         * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
        const int a = -(i * i >> 14);
        int b = 0x00A9;
        b = (b * a >> 14) + 0x0390;
        b = (b * a >> 14) + 0x091C;
        b = (b * a >> 14) + 0x0FB6;
        b = (b * a >> 14) + 0x16AA;
        b = (b * a >> 14) + 0x2081;
        b = (b * a >> 14) + 0x3651;
        b = (b * a >> 14) + 0xA2F9;
        return i * b >> 16;
    }

    consteval short arc_tan2(const int x, const int y) {
        // From https://github.com/mgba-emu/mgba/blob/8c2e2e1d4628e8794fa547a9acce5d18fe8f585a/src/gba/bios.c#L313
        /* Copyright (c) 2013-2015 Jeffrey Pfau
         *
         * This Source Code Form is subject to the terms of the Mozilla Public
         * License, v. 2.0. If a copy of the MPL was not distributed with this
        * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
        if (!y) {
            if (x >= 0) {
                return 0;
            }
            return 0x8000;
        }
        if (!x) {
            if (y >= 0) {
                return 0x4000;
            }
            return 0xC000;
        }
        if (y >= 0) {
            if (x >= 0) {
                if (x >= y) {
                    return arc_tan((y << 14) / x);
                }
            } else if (-x >= y) {
                return arc_tan((y << 14) / x) + 0x8000;
            }
            return 0x4000 - arc_tan((x << 14) / y);
        }
        if (x <= 0) {
            if (-x > -y) {
                return arc_tan((y << 14) / x) + 0x8000;
            }
        } else if (x >= -y) {
            return arc_tan((y << 14) / x) + 0x10000;
        }
        return 0xC000 - arc_tan((x << 14) / y);
    }

}
