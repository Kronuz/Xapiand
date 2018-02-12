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


module.exports.mul64 = (lhs, rhs) => {

  const l0 = lhs[1] & 0xffff;
  const l1 = (lhs[1] >> 16) & 0xffff;
  const l2 = lhs[0] & 0xffff;
  const l3 = (lhs[0] >> 16) & 0xffff;

  const r0 = rhs[1] & 0xffff;
  const r1 = (rhs[1] >> 16) & 0xffff;
  const r2 = rhs[0] & 0xffff;
  const r3 = (rhs[0] >> 16) & 0xffff;

  let c;
  let t0 = 0, t1 = 0, t2 = 0, t3 = 0;

  c = 0;
  c += t0 + r0 * l0; t0 = c & 0xffff; c >>= 16;
  c += t1 + r1 * l0; t1 = c & 0xffff; c >>= 16;
  c += t2 + r2 * l0; t2 = c & 0xffff; c >>= 16;
  c += t3 + r3 * l0; t3 = c & 0xffff; c >>= 16;

  c = 0;
  c += t1 + r0 * l1; t1 = c & 0xffff; c >>= 16;
  c += t2 + r1 * l1; t2 = c & 0xffff; c >>= 16;
  c += t3 + r2 * l1; t3 = c & 0xffff; c >>= 16;

  c = 0;
  c += t2 + r0 * l2; t2 = c & 0xffff; c >>= 16;
  c += t3 + r1 * l2; t3 = c & 0xffff; c >>= 16;

  c = 0;
  c += t3 + r0 * l3; t3 = c & 0xffff; c >>= 16;

  return [ ((t3 << 16) | t2) >>> 0, ((t1 << 16) | t0) >>> 0 ];
}

module.exports.add64 = (lhs, rhs) => {
  const l0 = lhs[1] & 0xffff;
  const l1 = (lhs[1] >> 16) & 0xffff;
  const l2 = lhs[0] & 0xffff;
  const l3 = (lhs[0] >> 16) & 0xffff;

  const r0 = rhs[1] & 0xffff;
  const r1 = (rhs[1] >> 16) & 0xffff;
  const r2 = rhs[0] & 0xffff;
  const r3 = (rhs[0] >> 16) & 0xffff;

  let c;
  let t0 = 0, t1 = 0, t2 = 0, t3 = 0;

  c = 0;
  c += l0 + r0; t0 = c & 0xffff; c >>= 16;
  c += l1 + r1; t1 = c & 0xffff; c >>= 16;
  c += l2 + r2; t2 = c & 0xffff; c >>= 16;
  c += l3 + r3; t3 = c & 0xffff; c >>= 16;

  return [ ((t3 << 16) | t2) >>> 0, ((t1 << 16) | t0) >>> 0 ];
}

module.exports.sub64 = (lhs, rhs) => {
  const l0 = lhs[1] & 0xffff;
  const l1 = (lhs[1] >> 16) & 0xffff;
  const l2 = lhs[0] & 0xffff;
  const l3 = (lhs[0] >> 16) & 0xffff;

  const r0 = rhs[1] & 0xffff;
  const r1 = (rhs[1] >> 16) & 0xffff;
  const r2 = rhs[0] & 0xffff;
  const r3 = (rhs[0] >> 16) & 0xffff;

  let b = 0;
  let t0 = 0, t1 = 0, t2 = 0, t3 = 0;

  b = l0 - r0 - b; b < 0 ? ( t0 = b + 65536, b = 1 ) : ( t0 = b & 0xffff, b = 0 );
  b = l1 - r1 - b; b < 0 ? ( t1 = b + 65536, b = 1 ) : ( t1 = b & 0xffff, b = 0 );
  b = l2 - r2 - b; b < 0 ? ( t2 = b + 65536, b = 1 ) : ( t2 = b & 0xffff, b = 0 );
  b = l3 - r3 - b; b < 0 ? ( t3 = b + 65536, b = 1 ) : ( t3 = b & 0xffff, b = 0 );
  return [ ((t3 << 16) | t2) >>> 0, ((t1 << 16) | t0) >>> 0 ];
}


module.exports.div64 = (n, d) => {
  const n0 = n[1] & 0xffff;
  const n1 = (n[1] >> 16) & 0xffff;
  const n2 = n[0] & 0xffff;
  const n3 = (n[0] >> 16) & 0xffff;
  d = d & 0xffff;

  let q0 = 0, q1 = 0, q2 = 0, q3 = 0;
  let r = 0;

  n = (r << 16 | n3); q3 = n / d; r = n % d;
  n = (r << 16 | n2); q2 = n / d; r = n % d;
  n = (r << 16 | n1); q1 = n / d; r = n % d;
  n = (r << 16 | n0); q0 = n / d; r = n % d;

  return [ ((q3 << 16) | q2) >>> 0, ((q1 << 16) | q0) >>> 0 ];
}

module.exports._add64 = (lhs, rhs) => {
  const l0 = (lhs[1] & 0xffffffff) >>> 0;
  const l1 = (lhs[0] & 0xffffffff) >>> 0;

  const r0 = (rhs[1] & 0xffffffff) >>> 0;
  const r1 = (rhs[0] & 0xffffffff) >>> 0;

  let c;
  let t0 = 0, t1 = 0;

  c = 0;
  c += l0 + r0; t0 = c & 0xffffffff; c >>= 32;
  c += l1 + r1; t1 = c & 0xffffffff; c >>= 32;

  return [ t1 >>> 0, t0 >>> 0 ];
}

module.exports.shr64 = (n, bits) => {
  const n0 = (n[1] & 0xffffffff) >>> 0;
  const n1 = (n[0] & 0xffffffff) >>> 0;

  let t0 = 0, t1 = 0;

  t0 = ((n1 << (32 - bits)) & 0xffffffff) | n0 >>> bits;
  t1 = n1 >>> bits;

  return [ t1 >>> 0, t0 >>> 0 ];
}

module.exports.shl64 = (n, bits) => {
  const n0 = (n[1] & 0xffffffff) >>> 0;
  const n1 = (n[0] & 0xffffffff) >>> 0;

  let t0 = 0, t1 = 0;

  t1 = n1 << bits | ((n0 >>> (32 - bits))  & 0xffffffff);
  t0 = n0 << bits;

  return [ t1 >>> 0, t0 >>> 0 ];
}

module.exports.xor64 = (lhs, rhs) => {
  const l0 = (lhs[1] & 0xffffffff) >>> 0;
  const l1 = (lhs[0] & 0xffffffff) >>> 0;

  const r0 = (rhs[1] & 0xffffffff) >>> 0;
  const r1 = (rhs[0] & 0xffffffff) >>> 0;

  return [ (r1 ^ l1) >>> 0 , (r0 ^ l0) >>> 0 ];
}

module.exports.and64 = (lhs, rhs) => {
  const l0 = (lhs[1] & 0xffffffff) >>> 0;
  const l1 = (lhs[0] & 0xffffffff) >>> 0;

  const r0 = (rhs[1] & 0xffffffff) >>> 0;
  const r1 = (rhs[0] & 0xffffffff) >>> 0;

  return [ (r1 & l1) >>> 0 , (r0 & l0) >>> 0 ];
}

module.exports.or64 = (lhs, rhs) => {
  const l0 = (lhs[1] & 0xffffffff) >>> 0;
  const l1 = (lhs[0] & 0xffffffff) >>> 0;

  const r0 = (rhs[1] & 0xffffffff) >>> 0;
  const r1 = (rhs[0] & 0xffffffff) >>> 0;

  return [ (r1 | l1) >>> 0 , (r0 | l0) >>> 0 ];
}


module.exports.shr128 = (n, bits) => { // assumes bits < 32
  const n3 = (n[0] & 0xffffffff) >>> 0;
  const n2 = (n[1] & 0xffffffff) >>> 0;
  const n1 = (n[2] & 0xffffffff) >>> 0;
  const n0 = (n[3] & 0xffffffff) >>> 0;

  let t0 = 0, t1 = 0, t3 = 0, t4 = 0;

  t0 = ((n1 << (32 - bits)) & 0xffffffff) | n0 >>> bits;
  t1 = ((n2 << (32 - bits)) & 0xffffffff) | n1 >>> bits;
  t2 = ((n3 << (32 - bits)) & 0xffffffff) | n2 >>> bits;
  t3 = n3 >>> bits;

  return [ t3 >>> 0, t2 >>> 0 , t1 >>> 0, t0 >>> 0 ];
}