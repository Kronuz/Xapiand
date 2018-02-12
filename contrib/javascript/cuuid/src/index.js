/*
 * Copyright (C) 2015-2018 dubalu.com LLC. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

const UUIDv1 = require('uuid/v1');
const MersenneTwister = require('mersenne-twister');
const op64 = require('./int64.js');
const xor64 = op64.xor64;
const or64 = op64.or64;
const and64 = op64.and64;
const add64 = op64.add64;
const sub64 = op64.sub64;
const mul64 = op64.mul64;
const div64 = op64.div64;
const shl64 = op64.shl64;
const shr64 = op64.shr64;
const shr128 = op64.shr128;
const bytesUtils = require('./utilsUUID');
const bytesToUUID = bytesUtils.bytesToUUID;
const UUIDtoBytes = bytesUtils.UUIDtoBytes;
const bs59 = require('./bx');
const repr = require('./repr.js');
const tobytes = repr.tobytes;

function ExceptionUUID(mensaje, uuid='') {
   this.mensaje = uuid && uuid.length ? mensaje + ' ' + uuid : mensaje;
   this.name = "ExceptionUUID";
}

const UUID_TIME_INITIAL = [31830073, 3355707392];
const UUID_MIN_SERIALISED_LENGTH = 2;
const UUID_MAX_SERIALISED_LENGTH = 17;
const UUID_LENGTH = 36;

const TIME_BITS = 60;
const VERSION_BITS = 64 - TIME_BITS;
const COMPACTED_BITS = 1;
const SALT_BITS = 7;
const CLOCK_BITS = 14;
const NODE_BITS = 48;
const PADDING_BITS = 64 - COMPACTED_BITS - SALT_BITS - CLOCK_BITS;
const PADDING1_BITS = 64 - COMPACTED_BITS - NODE_BITS - CLOCK_BITS;
const VARIANT_BITS = 2;

const TIME_MASK = [0x0fffffff, 0xffffffff];
const SALT_MASK = ((1 << SALT_BITS) - 1);
const CLOCK_MASK = ((1 << CLOCK_BITS) - 1);
const COMPACTED_MASK = ((1 << COMPACTED_BITS) - 1);
const NODE_MASK = [0xffff, 0xffffffff];
const VERSION_MASK = ((1 << VERSION_BITS) - 1);
const VARIANT_MASK = ((1 << VARIANT_BITS) - 1);

const VL = [
    [[0x1c, 0xfc], [0x1c, 0xfc]],
    [[0x18, 0xfc], [0x18, 0xfc]],
    [[0x14, 0xfc], [0x14, 0xfc]],
    [[0x10, 0xfc], [0x10, 0xfc]],
    [[0x04, 0xfc], [0x40, 0xc0]],
    [[0x0a, 0xfe], [0xa0, 0xe0]],
    [[0x08, 0xfe], [0x80, 0xe0]],
    [[0x02, 0xff], [0x20, 0xf0]],
    [[0x03, 0xff], [0x30, 0xf0]],
    [[0x0c, 0xff], [0xc0, 0xf0]],
    [[0x0d, 0xff], [0xd0, 0xf0]],
    [[0x0e, 0xff], [0xe0, 0xf0]],
    [[0x0f, 0xff], [0xf0, 0xf0]],
];

const toType = function(obj) {
  return ({}).toString.call(obj).match(/\s([a-zA-Z]+)/)[1].toLowerCase()
}

const _fnv_1a = num => {
    // calculate FNV-1a hash fnv = 0xcbf29ce484222325
    let fnv = [0xcbf29ce4, 0x84222325];
    while (num[1] || num[0]) {
        fnv = xor64(fnv , and64(num, [0, 0xff]));
        fnv = mul64([0x100, 0x000001b3], fnv);
        num = shr64(num, 8);
    }
    return fnv;
}

const xor_fold = (num, bits) => {
    // xor-fold to n bits:
    let folded = [0x0, 0x0];
    while(num[1] || num[0]) {
        folded = xor64(num, folded);
        num = shr64(num, bits);
    }
    return folded;
}

class UUID {
    constructor(uuid) {
        const bytes = UUIDtoBytes(uuid);
        this.bytes = bytes;

        // time_low
        let tl = bytes[0];
        tl = (tl << 8) | bytes[1];
        tl = (tl << 8) | bytes[2];
        tl = (tl << 8) | bytes[3];
        this.time_low = tl >>> 0;

        // time_mid
        let tm = bytes[4];
        tm = (tm << 8) | bytes[5];
        this.time_mid = tm;

        // time_hi_version
        let thav = bytes[6];
        thav = (thav << 8) | bytes[7];
        this.time_hi_version = thav;

        // clock_seq_hi_variant
        this.clock_seq_hi_variant = bytes[8];

        // clock_seq_low
        this.clock_seq_low = bytes[9];

        // node
        const node = bytes.slice(-6);
        const l = node.slice(-4);
        let node_lower = l[0];
        node_lower = (node_lower << 8) | l[1];
        node_lower = (node_lower << 8) | l[2];
        node_lower = (node_lower << 8) | l[3];
        const h = node.slice(0,2);
        let node_high = h[0];
        node_high = (node_high << 8) | h[1];
        node_high &= 0xffff;
        this.node = [node_high, node_lower >>> 0];

        // compacted_node
        this.compacted_node = [];
        // compacted_clock
        this.compacted_clock = [];
        // compacted_time
        this.compacted_time = [];
    }

    _uuid_buff_from_this() {
        let i = 0;
        let uuid_buff = [];
        /* time low */
        uuid_buff[i++] = this.time_low >>> 24 & 0xff;
        uuid_buff[i++] = this.time_low >>> 16 & 0xff;
        uuid_buff[i++] = this.time_low >>> 8 & 0xff;
        uuid_buff[i++] = this.time_low & 0xff;
        /* time mid */
        uuid_buff[i++] = this.time_mid >>> 8 & 0xff;
        uuid_buff[i++] = this.time_mid & 0xff;
        /* time_high_and_version */
        uuid_buff[i++] = this.time_hi_version >>> 8 & 0xf | 0x10;
        uuid_buff[i++] = this.time_hi_version & 0xff;
        /* clock_seq_hi_and_reserved */
        uuid_buff[i++] = this.clock_seq_hi_variant | 0x80;
        /* clock_seq_low */
        uuid_buff[i++] = this.clock_seq_low & 0xff;
        /* node */
        let n = this.node;
        for (let j = 5; j >= 0; j--) {
            uuid_buff[i + j] = n[1] & 0xff;
            n = shr64(n, 8);
        }
        return bytesToUUID(uuid_buff);
    }

    static _uuid_buff_from_args(time, clock, node) {
        let i = 0;
        let uuid_buff = [];
        /* time low */
        uuid_buff[i++] = time[1] >>> 24 & 0xff;
        uuid_buff[i++] = time[1] >>> 16 & 0xff;
        uuid_buff[i++] = time[1] >>> 8 & 0xff;
        uuid_buff[i++] = time[1] & 0xff;
        /* time mid */
        uuid_buff[i++] = time[0] >>> 8 & 0xff;
        uuid_buff[i++] = time[0] & 0xff;
        /* time_high_and_version */
        uuid_buff[i++] = time[0] >>> 24 & 0xf | 0x10;
        uuid_buff[i++] = time[0] >>> 16 & 0xff;
        /* clock_seq_hi_and_reserved */
        uuid_buff[i++] = clock[1] >>> 8 | 0x80;
        /* clock_seq_low */
        uuid_buff[i++] = clock[1] & 0xff;
        /* node */
        let n = node;
        for (let j = 5; j >= 0; j--) {
            uuid_buff[i + j] = n[1] & 0xff;
            n = shr64(n, 8);
        }
        return bytesToUUID(uuid_buff);
    }

    to_string() {
        if (arguments.length == 0) {
            return this._uuid_buff_from_this();
        } else if (arguments.length == 3) {
            return UUID._uuid_buff_from_args(arguments[0], arguments[1], arguments[2]);
        } else {
            throw new RangeError('Parameter must be: time, clock and node');
        }
    }

    get_version() {
        return this.bytes[6] >> 4;
    }

    get_time() {
        return [((this.time_hi_version & 0xfff) << 16) | this.time_mid, this.time_low];
    }

    get_clock_seq() {
         return ((this.clock_seq_hi_variant & 0x3f) << 8 ) | this.clock_seq_low;
    }

    static _calculate_node(time, clock, salt) {
        if (!time[0] && !time[1] && !clock[0] && !clock[1] && !salt[0] && !salt[1]) {
            return [0x0100, 0x00000000];
        }

        let seed = [0, 0];
        seed = xor64(seed, _fnv_1a(time));
        seed = xor64(seed, _fnv_1a(clock));
        seed = xor64(seed, _fnv_1a(salt));
        const g = new MersenneTwister(seed[1]);
        let node = [];
        node[0] = g.random_int();
        node[1] = g.random_int();
        node = and64(node, and64(NODE_MASK, [0xffffffff, ~SALT_MASK]));
        node = or64(node, salt);
        node = or64(node, [0x100, 0x00000000]);  // set multicast bit;
        return node;
    }

    serialise() {
        const version = this.bytes[6] >> 4;
        const variant = this.bytes[8] & 0xc0;
        if (variant === 0x80 && version === 1) {
            const node = this.node;
            let time = this.get_time();
            let compacted_time = [0, 0];
            if (time[0] || time[1] ) {
                compacted_time = and64(sub64(time, UUID_TIME_INITIAL), TIME_MASK);
            }
            const compacted_time_clock = and64(compacted_time, [0, CLOCK_MASK])
            const clock = [0, this.get_clock_seq() & CLOCK_MASK];
            compacted_time = shr64(compacted_time, CLOCK_BITS);
            this.compacted_clock = xor64(clock, compacted_time_clock);
            const s = and64(node, [0x0100, 0x00000000]);
            let salt;
            if (s[1] || s[0]) {
                salt = and64(node, [0x0, SALT_MASK]);
            } else {
                salt = _fnv_1a(node);
                salt = xor_fold(salt, SALT_BITS);
                salt = and64(salt, [0x0, SALT_MASK]);
            }
            const compacted_node = UUID._calculate_node(compacted_time, this.compacted_clock, salt);
            const compacted = node[1] === compacted_node[1] && node[0] === compacted_node[0];
            this.compacted_node = compacted_node;

            let meat_high;
            let meat_low;
            if (compacted) {
                let clock_salt_cmp = this.compacted_clock[1];
                clock_salt_cmp <<= SALT_BITS;
                clock_salt_cmp |= salt[1];
                clock_salt_cmp <<= COMPACTED_BITS;
                clock_salt_cmp |= 1;
                clock_salt_cmp &= 0x3fffff;

                let m3 = 0, m2 = 0, m1 = compacted_time[0], m0 = compacted_time[1];
                m2 <<= 22;
                m2 |= (m1 >>> 10);
                m1 <<= 22;
                m1 |= (m0 >>> 10);
                m0 <<= 22;
                m0 |= clock_salt_cmp;

                meat_high = [m3, m2];
                meat_low = [m1, m0];

                // set compacted_time
                this.compacted_time = and64(add64(shl64(compacted_time, CLOCK_BITS), UUID_TIME_INITIAL), TIME_MASK);
            } else {
                if (!s[1] && !s[0]) {
                    if (time[0] || time[1]) {
                        time = and64(sub64(time, UUID_TIME_INITIAL), TIME_MASK);
                    }
                }
                meat_high = time;
                meat_low = clock;
                const node_bits = NODE_BITS / 2;
                meat_low = shl64(shl64(meat_low, node_bits), node_bits); // shl64 shift only 32 bits less or equal
                meat_low = or64(meat_low, node);
                meat_low = shl64(meat_low, COMPACTED_BITS);
                /* meat_low is 63 bits, we need mix with meat_high to make meat_low 64 bits */
                meat_low[0] = (((meat_high[1] & 1) << 31) | (meat_low[0] & 0x7fffffff)) >>> 0;
                meat_high = shr64(meat_high, 1);
            }
            let bytes_ = [];
            while (meat_low[0] || meat_low[1] || bytes_.length < 4) {
                bytes_.push(and64(meat_low, [0, 0xff])[1]);
                meat_low = shr64(meat_low, 8);
            }

            while (meat_high[0] || meat_high[1] || bytes_.length < 4) {
                bytes_.push(and64(meat_high, [0, 0xff])[1]);
                meat_high = shr64(meat_high, 8);
            }
            const length = bytes_.length - 4;
            const last = bytes_.length - 1;
            if (bytes_[last] & VL[length][0][1]) {
                if (bytes_[last] & VL[length][1][1]) {
                    bytes_.push(VL[length + 1][0][0]);
                } else {
                    bytes_[last] |= VL[length][1][0];
                }
            } else {
                bytes_[last] |= VL[length][0][0];
            }
            return bytes_.map(val => String.fromCharCode(val)).reverse().join('');
        } else {
            this.compacted_node = [0, 0];
            this.compacted_time = [0, 0];
            this.compacted_clock = [0, 0];
            return String.fromCharCode(0x01) + this.bytes.map(val => String.fromCharCode(val)).join('');
        }
    }

    static _decode(encoded, count=undefined) {
        if (encoded.length >= 7 &&  encoded[0] == '~') {
            const serialised = bs59.b59decode(encoded);
            if (UUID._is_serialised(serialised, count)) {
                return serialised;
            }
        }
        const u = new UUID(encoded);
        return u.serialise();
    }

    static _unserialise_condensed(bytes_) {
        const size = bytes_.length;
        let length = size;
        let byte0 = bytes_.charCodeAt(0);
        const q = byte0 & 0xf0 ? 1 : 0;
        let i;
        for (i = 0; i < 13; i++) {
            if (VL[i][q][0] === (byte0 & VL[i][q][1])) {
                length = i + 4;
                break;
            }
        }
        if (size < length) {
            throw new ExceptionUUID("Bad encoded uuid");
        }

        let list_bytes_ = bytes_.slice(0, length).split('');
        byte0 &= ~VL[i][q][1];
        list_bytes_[0] = String.fromCharCode(byte0);

        let meat = [0, 0, 0, 0];
        let cnt = 0, shrbits = 8, empty32 = 3;
        for (i = list_bytes_.length-1; i >= 0; i--) {
            if (cnt === 4) {
                if (meat[2]) { meat[3] = meat[2]; }
                if (meat[1]) { meat[2] = meat[1]; }
                if (meat[0]) { meat[1] = meat[0]; }
                meat[0] = 0;
                cnt = 0;
                shrbits = 8;
                empty32--;
            }
            if (cnt) {
                meat[0] = list_bytes_[i].charCodeAt(0) << shrbits | meat[0];
                shrbits += 8;
            } else {
                meat[0] = list_bytes_[i].charCodeAt(0);
            }
            cnt++;
            meat[0] >>>= 0;
        }

        /* Compact 128 bits */
        if (empty32) {
            while (empty32) {
                if (meat[2] && !meat[3]) { meat[3] = meat[2]; meat[2] = 0; }
                if (meat[1] && !meat[2]) { meat[2] = meat[1]; meat[1] = 0; }
                if (meat[0] && !meat[1]) { meat[1] = meat[0]; meat[0] = 0; }
                empty32--;
            }
        }

        const compacted = meat[3] & 1;

        const c0 = meat[0] & COMPACTED_BITS;
        meat[0] >>>= COMPACTED_BITS;

        const c1 = meat[1] & COMPACTED_BITS;
        meat[1] >>>= COMPACTED_BITS;
        meat[1] = ((c0 << (32 - COMPACTED_BITS)) | meat[1]) >>> 0;

        const c2 = meat[2] & COMPACTED_BITS;
        meat[2] >>>= COMPACTED_BITS;
        meat[2] = ((c1 << (32 - COMPACTED_BITS)) | meat[2]) >>> 0;

        meat[3] >>>= COMPACTED_BITS;
        meat[3] = ((c2 << (32 - COMPACTED_BITS)) | meat[3]) >>> 0;

        let clock, time, node;

        if (compacted) {
            let meat_low = [meat[2], meat[3]];
            const salt = and64(meat_low, [0, SALT_MASK]);
            meat_low = shr64(meat_low, SALT_BITS);
            clock = and64(meat_low, [0, CLOCK_MASK]);
            meat_low = shr64(meat_low, CLOCK_BITS);

            let m3 = meat[0];
            let m2 = meat[1];
            let m1 = meat[2];
            let m0 = meat[3];
            const bitshr = SALT_BITS + CLOCK_BITS;
            const bitrest = 32 - bitshr;
            let m = m1 & 0x1fffff;
            m1 = ((m2 & 0x1fffff) << bitrest) | m1 >>> bitshr;
            m0 = m << bitrest | m0 >>> bitshr;
            time = and64([m1, m0], TIME_MASK);
            node = UUID._calculate_node(time, clock, salt);
        } else {
            node = and64([meat[2], meat[3]], NODE_MASK);
            /* shift right NODE bits */
            let highm1 = meat[1] >>> 16;
            let lowm1= meat[1] & 0xffff;
            let highm2 = meat[2] >>> 16;
            let lowm2 = 0, highm3 = 0

            meat[1] = meat[0] >>> 16;
            meat[2] = ((meat[0] & 0xffff) << 16 | highm1) >>> 0;
            meat[3] = (lowm1 << 16 | highm2) >>> 0;
            clock = and64([meat[2], meat[3]], [0, CLOCK_MASK]);

            /* shift right CLOCK bits */
            highm1 = meat[1] >>> CLOCK_BITS;
            lowm1 = meat[1] & 0x3fff;
            highm2 = meat[2] >>> CLOCK_BITS;
            lowm2 = meat[2] & 0x3fff;
            highm3 = meat[3] >>> CLOCK_BITS;

            meat[1] = highm1;
            meat[2] = (lowm1 << (32 - CLOCK_BITS) | highm2) >>> 0;
            meat[3] = (lowm2 << (32 - CLOCK_BITS) | highm3) >>> 0;
            time = and64([meat[2], meat[3]], TIME_MASK);
        }

        if (time[0] || time[1]) {
            if (compacted) {
                time = add64(shl64(time, CLOCK_BITS), UUID_TIME_INITIAL);
            } else {
                const mbit = and64(node, [0x0100, 0x00000000]);
                if (!mbit[0] && !mbit[1]) {
                    time = add64(time, UUID_TIME_INITIAL);
                }
            }
        }
        return [UUID._uuid_buff_from_args(time, clock, node), length];
    }

    static unserialise(serialised) {
        const uuid = UUID._unserialise(serialised);
        if (uuid[1] > serialised.length) {
            throw new ExceptionUUID("Invalid serialised uuid", serialised);
        }
        return uuid[0];
    }

    static _unserialise_full(bytes_) {
        if (bytes_.length < 17) {
            throw new ExceptionUUID("Bad encoded uuid");
        }
        const some = bytes_.slice(1, 17).split('').map(val => val.charCodeAt(0));
        return [bytesToUUID(bytes_.slice(1, 17).split('').map(val => val.charCodeAt(0))), 17];
    }

    static _unserialise(bytes_) {
        if (bytes_ === undefined || bytes_.length < 2) {
            throw new ExceptionUUID("Bad encoded uuid");
        }

        if (bytes_.length && bytes_.charCodeAt(0) === 1) {
            return UUID._unserialise_full(bytes_);
        } else {
            return UUID._unserialise_condensed(bytes_);
        }
    }

    get_compacted_node() {
        if (this.compacted_node.length) {
            return this.compacted_node;
        } else {
            this.serialise();
            return this.compacted_node;
        }
    }

    iscompact() {
        return this.compacted_node === this.node;
    }

    compact_crush() {
        const _compacted_node = this.get_compacted_node()
        if (_compacted_node[0] || _compacted_node[1]) {
            return this.to_string(this.compacted_time, this.compacted_clock, this.compacted_node);
        }
        return this.to_string();
    }

    static _is_serialised(serialised, count=undefined) {
        let offset = 0;
        while (serialised.length) {
            if (count !== undefined) {
                if (!count) {
                    return false;
                }
                count -= 1;
            }
            let length;
            const size = serialised.length;
            if (serialised.length < 2) {
                return false;
            }
            let byte0 = serialised[0].charCodeAt(0);
            if (byte0 === 1) {
                length = 17;
            } else {
                length = serialised.length;
                const q = byte0 & 0xf0 ? 1 : 0;
                for (let i = 0; i < 13; i++) {
                    if (VL[i][q][0] === (byte0 & VL[i][q][1])) {
                        length = i + 4;
                        break;
                    }
                }
            }
            if (size < length) {
                return false;
            }
            offset = length;
            serialised = serialised.slice(offset);
        }
        return true;
    }

    encode(encoding='encoded') {
        return encode(this.serialise(), encoding);
    }

    static new_uuid(data=undefined, compacted=undefined) {
        if (data) {
            if (data.length > 128) {
                throw new ExceptionUUID("UUIDs can only store as much as 15 bytes", data);
            }

            let _data = data;
            const typ = toType(data);
            if (typ === 'string') {
                _data = data.slice(0, data.length).split('');
            } else if (typ !== 'array') {
                throw new ExceptionUUID("Bad data sended", uuid_);
            }

            let i;
            let meat = [0, 0, 0, 0];
            let cnt = 0, shrbits = 8, empty32 = 4;
            for (i = _data.length-1; i >= 0; i--) {
                if (cnt === 4) {
                    if (meat[2]) { meat[3] = meat[2]; }
                    if (meat[1]) { meat[2] = meat[1]; }
                    if (meat[0]) { meat[1] = meat[0]; }
                    meat[0] = 0;
                    cnt = 0;
                    shrbits = 8;
                    empty32--;
                }
                if (cnt) {
                    meat[0] = _data[i].charCodeAt(0) << shrbits | meat[0];
                    shrbits += 8;
                } else {
                    meat[0] = _data[i].charCodeAt(0);
                }
                cnt++;
                meat[0] >>>= 0;
            }

            /* Compact 128 bits */
            if (empty32) {
                while (empty32) {
                    if (meat[2] && !meat[3]) { meat[3] = meat[2]; meat[2] = 0; }
                    if (meat[1] && !meat[2]) { meat[2] = meat[1]; meat[1] = 0; }
                    if (meat[0] && !meat[1]) { meat[1] = meat[0]; meat[0] = 0; }
                    empty32--;
                }
            }

            let cp_meat = Array().concat(meat);
            let highm3 = cp_meat[3] >>> 31;
            let highm2 = cp_meat[2] >>> 31;
            let highm1 = cp_meat[1] >>> 31;
            let lowm2 = cp_meat[2] & 0x7fffffff;
            let lowm1 = cp_meat[1] & 0x7fffffff;
            let lowm0 = cp_meat[0] & 0x7fffffff;
            cp_meat[3] <<= 1;
            cp_meat[2] = ((lowm2 << 1) | highm3) >>> 0;
            cp_meat[1] = ((lowm1 << 1) | highm2) >>> 0;
            cp_meat[0] = lowm0 >>> 0;

            let n1 = [cp_meat[2], cp_meat[3]];
            n1 = and64(n1, [0xfe00, 0x00000000]);
            let n2 = [meat[2], meat[3]];
            n2 = and64(n2, [0x00ff, 0xffffffff]);
            let node = or64(n1, n2);
            node = or64(node, [0x0100, 0x00000000]);  // Multicast bit set

            /* shift right 47 bits */
            meat = shr128([0, meat[0], meat[1], meat[2]], (47 - 32));

            let clock_seq_low = meat[3] & 0xff;
            meat = shr128(meat, 8);

            let clock_seq_hi_variant = meat[3] & 0x3f;
            meat = shr128(meat, 6);

            let time_low = [];
            time_low[0] = meat[3] & 0xff;
            meat = shr128(meat, 8);
            time_low[1] = meat[3] & 0xff;
            meat = shr128(meat, 8);
            time_low[2] = meat[3] & 0xff;
            meat = shr128(meat, 8);
            time_low[3] = meat[3] & 0xff;
            meat = shr128(meat, 8);

            let time_mid = [];
            time_mid[0] = meat[3] & 0xff;
            meat = shr128(meat, 8);
            time_mid[1] = meat[3] & 0xff;
            meat = shr128(meat, 8);

            let time_high_and_version = [];
            time_high_and_version[0] = meat[3] & 0xff;
            meat = shr128(meat, 8);
            time_high_and_version[1] = meat[3] & 0xf | 0x10;

            i = 0;
            let uuid_buff = [];
            /* time low */
            uuid_buff[i++] = time_low[3];
            uuid_buff[i++] = time_low[2];
            uuid_buff[i++] = time_low[1];
            uuid_buff[i++] = time_low[0];
            /* time mid */
            uuid_buff[i++] = time_mid[1];
            uuid_buff[i++] = time_mid[0];
            /* time_high_and_version */
            uuid_buff[i++] = time_high_and_version[1];
            uuid_buff[i++] = time_high_and_version[0];;
            /* clock_seq_hi_and_reserved */
            uuid_buff[i++] = clock_seq_hi_variant | 0x80;
            /* clock_seq_low */
            uuid_buff[i++] = clock_seq_low;
            /* node */
            let n = node;
            for (let j = 5; j >= 0; j--) {
                uuid_buff[i + j] = n[1] & 0xff;
                n = shr64(n, 8);
            }
            return bytesToUUID(uuid_buff);
        }
        let _uuid = UUIDv1();
        if (compacted || compacted === undefined) {
            return new UUID(_uuid).compact_crush();
        }
        return _uuid;
    }

    data() {
        const version = this.get_version();
        const variant = this.clock_seq_hi_variant & 0x80;
        let n = and64(this.node, [0x0100, 0x00000000]);
        let high_data_bits, low_data_bits;
        if (variant === 0x80 && version === 1 && (n[0] || n[1])) {
            high_data_bits = [0, 0];
            high_data_bits[0] <<= 12;
            high_data_bits[0] |= this.time_hi_version & 0xfff;
            high_data_bits[0] <<= 16;
            high_data_bits[0] |= this.time_mid & 0xffff;
            high_data_bits[1] = this.time_low;
        }
        low_data_bits = [0, 0];
        low_data_bits[0] <<= 6;
        low_data_bits[0] |= this.clock_seq_hi_variant & 0x3f;
        low_data_bits[0] <<= 8;
        low_data_bits[0] |= this.clock_seq_low & 0xff;
        low_data_bits[0] <<= 15;
        const n0 = shr64(and64(this.node, [0xfe00, 0x00000000]), 1);
        const n1 = and64(this.node, [0x00ff, 0xffffffff]);
        n = or64(n0, n1);
        low_data_bits[0] |= n[0];
        low_data_bits[1] = n[1];

        let i;
        let data = []
        for (i = 0; i < 4; i++) {
            data.push(String.fromCharCode(low_data_bits[1] & 0xff));
            low_data_bits[1] >>>= 8;
        }

        low_data_bits[0] = (high_data_bits[1] & 0x07) << 29 | low_data_bits[0];
        high_data_bits = shr64(high_data_bits, 3);

        for (i = 0; i < 4; i++) {
            data.push(String.fromCharCode(low_data_bits[0] & 0xff));
            low_data_bits[0] >>>= 8;
        }

        for (let j = 1; j >= 0; j--) {
            for (i = 0; i < 4; i++) {
                const ch = high_data_bits[j] & 0xff;
                if (ch) {
                    data.push(String.fromCharCode(ch));
                    high_data_bits[j] >>>= 8;
                } else {
                    break;
                }
            }
        }
        return data.reverse().join('');
    }
}

const unserialise = (bytes_) => {
    let uuids = [];
    while (bytes_.length) {
        uuid = UUID._unserialise(bytes_);
        uuids.push(uuid[0]);
        bytes_ = bytes_.slice(uuid[1]);
    }
    return uuids;
}

module.exports.encode = (serialised, rep='encoded') => {
    const typ = toType(serialised);
    if (typ === 'string') {
        if (rep === 'guid') {
            return unserialise(serialised).map(v => `{${v}}`).join(';');
        } else if (rep === 'urn') {
            return 'urn:uuid:' + unserialise(serialised).join(';');
        } else if (rep === 'encoded') {
            const last = serialised.length - 1;
            if (serialised[0].charCodeAt(0) != 1 && ((serialised[last].charCodeAt(0) & 1) || (serialised.length >= 6 && serialised[last-5].charCodeAt(0) & 2))) {
                return '~' + bs59.b59encode(tobytes(serialised));
            }
        }
        return unserialise(serialised).join(';');
    }
    throw new ExceptionUUID("Invalid serialised UUID: ", serialised);
}

module.exports.decode = (encoded) => {
    let typ = toType(encoded);
    if (typ == 'string') {
        if (encoded.length > 2) {
            if (encoded[0] == '{' && encoded[encoded.length-1] == '}') {
                encoded = encoded.slice(1, encoded.length-1);
            } else if (encoded.startsWith('urn:uuid:')) {
                encoded = encoded.slice(9);
            }
            if (encoded.length) {
                encoded = encoded.split(';');
                typ = toType(encoded);
            }
        }
    }
    if (typ === 'array') {
        let serialised = encoded.map(u => { return UUID._decode(u); });
        return serialised.join('');
    }
    throw new ExceptionUUID("Invalid encoded UUID: ", encoded);
}

module.exports.encode_uuid = (uuid) => {
    if (uuid === undefined) {
        throw new ExceptionUUID("Cannot encode undefined");
    }
    if (!(uuid instanceof UUID)) {
        uuid = new UUID(uuid);
    }
    return uuid.encode()
}

module.exports.decode_uuid = (code) => {
    return UUID.unserialise(decode(code));
}

module.exports.UUID = UUID;
module.exports.unserialise = unserialise;
