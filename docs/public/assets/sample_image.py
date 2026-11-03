#!/usr/bin/env python3
# Copyright © 2026 Germán Méndez Bravo (Kronuz)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
"""Generates the docs' sample.png and sample.pdf (a generated interference
pattern, MIT like the rest of the repository). sample.jpg is made from the PNG
with: sips -s format jpeg -s formatOptions 85 sample.png --out sample.jpg"""

import math
import struct
import sys
import zlib

N = 256


def pixels():
    rows = []
    for y in range(N):
        row = bytearray()
        for x in range(N):
            u, v = x / N - 0.5, y / N - 0.5
            a = math.sin(24 * math.hypot(u + 0.2, v)) + math.sin(24 * math.hypot(u - 0.2, v + 0.1))
            t = (a + 2) / 4
            row += bytes((int(255 * t), int(255 * (1 - t) * (0.5 + x / (2 * N))), int(255 * (0.3 + 0.7 * y / N))))
        rows.append(bytes(row))
    return rows


def png(rows):
    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    raw = b"".join(b"\0" + r for r in rows)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", N, N, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def pdf(rows):
    img = zlib.compress(b"".join(rows), 9)
    content = b"q 400 0 0 400 106 196 cm /Im0 Do Q"
    objs = [
        b"<< /Type /Catalog /Pages 2 0 R >>",
        b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        b"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] /Contents 4 0 R /Resources << /XObject << /Im0 5 0 R >> >> >>",
        b"<< /Length %d >>\nstream\n" % len(content) + content + b"\nendstream",
        b"<< /Type /XObject /Subtype /Image /Width %d /Height %d /ColorSpace /DeviceRGB /BitsPerComponent 8 "
        b"/Filter /FlateDecode /Length %d >>\nstream\n" % (N, N, len(img)) + img + b"\nendstream",
    ]
    out = bytearray(b"%PDF-1.4\n")
    offs = []
    for i, o in enumerate(objs, 1):
        offs.append(len(out))
        out += b"%d 0 obj\n" % i + o + b"\nendobj\n"
    xref = len(out)
    out += b"xref\n0 %d\n0000000000 65535 f \n" % (len(objs) + 1)
    for o in offs:
        out += b"%010d 00000 n \n" % o
    out += b"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (len(objs) + 1, xref)
    return bytes(out)


if __name__ == "__main__":
    d = sys.argv[1] if len(sys.argv) > 1 else "."
    r = pixels()
    open(d + "/sample.png", "wb").write(png(r))
    open(d + "/sample.pdf", "wb").write(pdf(r))
