// base-x encoding
// Forked from https://github.com/cryptocoinjs/bs58
// Originally written by Mike Hearn for BitcoinJ
// Copyright (c) 2011 Google Inc
// Ported to JavaScript by Stefan Thomas
// Merged Buffer refactorings from base58-native by Stephen Pair
// Copyright (c) 2013 BitPay Inc
// Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.

function base (ALPHABET, TRANSLATE) {
  var ALPHABET_CHR = []
  var ALPHABET_MAP = []
  var BASE = ALPHABET.length
  var LEADER = ALPHABET.charAt(0)

  for (var c = 0; c < 256; ++c) {
      ALPHABET_CHR[c] = 0;
      ALPHABET_MAP[c] = BASE;
  }

  // pre-compute lookup table
  for (var z = 0; z < ALPHABET.length; z++) {
    var x = ALPHABET.charAt(z).charCodeAt(0)
    ALPHABET_CHR[z] = x
    if (ALPHABET_MAP[x] !== BASE) throw new TypeError(x + ' is ambiguous')
    ALPHABET_MAP[x] = z
  }

  var x_ = -1
  for (let a of TRANSLATE) {
    var o = a.charCodeAt(0)
    var i = ALPHABET_MAP[o]
    if (i < BASE) {
      x_ = i
    } else {
      ALPHABET_MAP[o] = x_
    }
  }

  function encode (source) {
    if (source.length === 0) return ''

    var digits = [0]
    for (var i = 0; i < source.length; ++i) {
      for (var j = 0, carry = source[i]; j < digits.length; ++j) {
        carry += digits[j] << 8
        digits[j] = carry % BASE
        carry = (carry / BASE) | 0
      }

      while (carry > 0) {
        digits.push(carry % BASE)
        carry = (carry / BASE) | 0
      }
    }

    var string = ''

    // calculate checksum
    var sum_chk = 0
    for (var p = 0; p < digits.length; p++) sum_chk += digits[p]
    sum_chk += digits.length + Math.floor(digits.length / BASE)
    sum_chk = sum_chk % BASE
    sum_chk = (BASE - (sum_chk % BASE)) % BASE

    // deal with leading zeros
    for (var k = 0; source[k] === 0 && k < source.length - 1; ++k) string += LEADER
    // convert digits to a string
    for (var q = digits.length - 1; q >= 0; --q) string += ALPHABET[ALPHABET_MAP[ALPHABET_CHR[digits[q]]]]

    return string + ALPHABET[ALPHABET_MAP[ALPHABET_CHR[sum_chk]]]
  }

  function decodeUnsafe (string_) {
    var string = string_
    if (typeof string !== 'string') throw new TypeError('Expected String')
    if (string.length === 0) return ''

    var bytes = [0]
    var sum_chk = 0
    var sumsz = 0

    var chk = 0
    while(true) {
      chk = ALPHABET_MAP[string.slice(-1).charCodeAt(0)]
      string = string.slice(0, string.length-1)
      if (chk < 0) continue
      if (chk > BASE) throw new TypeError('Invalid character')
      break;
    }

    for (var i = 0; i < string.length; i++) {
      var value = ALPHABET_MAP[string[i].charCodeAt(0)]
      if (value === -1) continue
      if (value === undefined) return

      for (var j = 0, carry = value; j < bytes.length; ++j) {
        carry += bytes[j] * BASE
        bytes[j] = carry & 0xff
        carry >>= 8
      }

      while (carry > 0) {
        bytes.push(carry & 0xff)
        carry >>= 8
      }
      sum_chk += value
      sumsz += 1
    }

    sum_chk += sumsz + Math.floor(sumsz / BASE)
    sum_chk = sum_chk % BASE

    sum_chk += chk
    if (sum_chk % BASE) throw new TypeError('Invalid checksum')

    // deal with leading zeros
    for (var k = 0; string[k] === LEADER && k < string.length - 1; ++k) {
      bytes.push(0)
    }
    return bytes.reverse().map(val => String.fromCharCode(val)).join('');
  }

  function decode (string) {
    var buffer = decodeUnsafe(string)
    if (buffer) return buffer

    throw new Error('Non-base' + BASE + ' character')
  }

  return {
    encode: encode,
    decodeUnsafe: decodeUnsafe,
    decode: decode
  }
}

const BASE59 = 'zGLUAC2EwdDRrkWBatmscxyYlg6jhP7K53TibenZpMVuvoO9H4XSQq8FfJN'
const TRANSLATE59 = '~l1IO0'
const bs59 = base(BASE59, TRANSLATE59)

module.exports.b59encode = bs59.encode
module.exports.b59decode = bs59.decode
