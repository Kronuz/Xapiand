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


module.exports.repr = (p) => {
    const len = p.length;
    const hexprefix = '\\x';
    let result = '';
    let i = 0;
    while (i < len) {
        const v = p.charCodeAt(i);
        if (v >= 32 && v <= 126) {
            result = result.concat(p[i]);
        } else {
            switch(v) {
                case 9:
                    result = result.concat('\\t');
                    break;
                case 10:
                    result = result.concat('\\n');
                    break;
                case 13:
                    result = result.concat('\\r');
                    break;
                case 92:
                    result = result.concat('\\');
                    break;
                default:
                    result = result.concat(hexprefix + v.toString(16));
            }
        }
        i += 1;
    }
    return result;
}

module.exports.tobytes = (a) => {
    let buff = [];
    for (let i = 0; i < a.length; i++) {
        buff.push(a[i].charCodeAt(0));
    }
    return buff;
}
