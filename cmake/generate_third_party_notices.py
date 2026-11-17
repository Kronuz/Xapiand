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
"""Writes THIRD-PARTY-NOTICES from the fetched dependency sources.

usage: generate_third_party_notices.py DEPS_DIR OUTPUT

DEPS_DIR is a configured build's _deps directory. Each notice is copied from
the dependency's own license file, or from the leading comment of the header
that carries it, so the file tracks repins: rerun it at every release."""

import re
import sys
from pathlib import Path

# (name, SPDX license, source dir, mode, path): mode "file" copies the license
# file, "header" copies the leading comment of a source file, "lua" copies the
# license block of lua.h, "name" lists the copyright lines only (no notice
# required in a binary).
BUNDLED = [
    ("rapidjson", "MIT", "rapidjson-src", "file", "license.txt"),
    ("cppcodec", "MIT", "cppcodec-src", "file", "LICENSE"),
    ("CLI11", "BSD-3-Clause", "cli11-src", "file", "LICENSE"),
    ("lz4 (lib/, with xxHash)", "BSD-2-Clause", "lz4-src", "file", "lib/LICENSE"),
    ("prometheus-cpp", "MIT", "prometheus_cpp-src", "file", "LICENSE"),
    ("Lua", "MIT", "lua-src", "lua", "src/lua.h"),
    ("libyaml", "MIT", "libyaml-src", "file", "License"),
    ("Snowball (via snowball)", "BSD-3-Clause", "snowball-src", "file", "LICENSE-SNOWBALL"),
    ("Unicode Character Database tables (via unicode)", "Unicode-3.0", "unicode-src", "file", "LICENSE-UNICODE"),
    ("http-parser, llhttp and nginx's parser (via http-parser)", "MIT", "http_parser-src", "file", "LICENSE"),
    ("uint128_t (via uinteger-t)", "MIT", "uinteger_t-src", "file", "LICENSE"),
    ("Lucene's ReqOptSumScorer (via prospect)", "Apache-2.0", "prospect-src", "file", "NOTICE"),
    ("asio", "BSL-1.0", "asio-src", "name", "asio/include/asio.hpp"),
    ("static string (via static-string)", "BSL-1.0", "static_string-src", "name", "LICENSE"),
    ("msgpack-c and Boost.Preprocessor (via msgpack)", "BSL-1.0", "msgpack-src", "name", "LICENSE_1_0.txt"),
]

# Third-party code embedded in Kronuz libraries, whose notice sits in a source header only.
HEADERS = [
    ("memory pool (allocators)", "BSD-2-Clause", "allocators-src", "allocators.h"),
    ("xxHash (compressors)", "BSD-2-Clause", "compressors-src", "xxh32_stream.h"),
    ("Better Enums (enum-reflection)", "BSD-2-Clause", "enum_reflection-src", "enum.h"),
    ("furan's parser nodes (boolean-parser)", "MIT", "boolean_parser-src", "AndNode.h"),
    ("crossguid (cuuid)", "MIT", "cuuid-src", "cuuid.hh"),
    ("Daniel Kirchner's hashes (hashes)", "MIT", "hashes-src", "hashes.hh"),
    ("MD5 (md5)", "Zlib", "md5-src", "md5.h"),
    ("SHA-256 (sha256)", "Zlib", "sha256-src", "sha256.h"),
    ("callable_traits (scheduler)", "MIT", "scheduler-src", "callable_traits.hh"),
]


# Static notices for data files kept in the tree.
MIME_TYPES = """mime.types is a media type table in nginx's format. Its grouping and most of its
lines derive from h5bp's server-configs-nginx (MIT), itself derived from nginx's
conf/mime.types (BSD-2-Clause); the yaml, ndjson and msgpack entries are Xapiand's own.

h5bp server-configs-nginx (https://github.com/h5bp/server-configs-nginx), MIT:

Copyright (c) H5BP

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

nginx (https://github.com/nginx/nginx), BSD-2-Clause:

Copyright (C) 2002-2021 Igor Sysoev
Copyright (C) 2011-2026 Nginx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE."""


KITWARE = 'Copyright 2000-2026 Kitware, Inc. and Contributors\n\nRedistribution and use in source and binary forms, with or without\nmodification, are permitted provided that the following conditions\nare met:\n\n* Redistributions of source code must retain the above copyright\n  notice, this list of conditions and the following disclaimer.\n\n* Redistributions in binary form must reproduce the above copyright\n  notice, this list of conditions and the following disclaimer in the\n  documentation and/or other materials provided with the distribution.\n\n* Neither the name of Kitware, Inc. nor the names of Contributors\n  may be used to endorse or promote products derived from this\n  software without specific prior written permission.\n\nTHIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\nLIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\nA PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT\nHOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\nSPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT\nLIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\nDATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY\nTHEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\nOF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.'
SAVOY = 'Copyright (c) 2005, Jacques Savoy\nAll rights reserved.\n\nRedistribution and use in source and binary forms, with or without\nmodification, are permitted provided that the following conditions are met:\n\nRedistributions of source code must retain the above copyright notice, this\nlist of conditions and the following disclaimer. Redistributions in binary\nform must reproduce the above copyright notice, this list of conditions and\nthe following disclaimer in the documentation and/or other materials\nprovided with the distribution. Neither the name of the author nor the names\nof its contributors may be used to endorse or promote products derived from\nthis software without specific prior written permission.\n\nTHIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"\nAND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE\nIMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE\nARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE\nLIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR\nCONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF\nSUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS\nINTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN\nCONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)\nARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE\nPOSSIBILITY OF SUCH DAMAGE.'
SNOWBALL = 'Copyright (c) 2001, Dr Martin Porter,\nCopyright (c) 2002, Richard Boulton.\nAll rights reserved.\n\nRedistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:\n\n1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.\n\n2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.\n\n3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.\n\nTHIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.'
ISO = 'The MIT License (MIT)\n\nCopyright (c) 2020 Gene Diaz\n\nPermission is hereby granted, free of charge, to any person obtaining a copy\nof this software and associated documentation files (the "Software"), to deal\nin the Software without restriction, including without limitation the rights\nto use, copy, modify, merge, publish, distribute, sublicense, and/or sell\ncopies of the Software, and to permit persons to whom the Software is\nfurnished to do so, subject to the following conditions:\n\nThe above copyright notice and this permission notice shall be included in all\ncopies or substantial portions of the Software.\n\nTHE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\nIMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\nFITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\nAUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\nLIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\nOUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\nSOFTWARE.'


# Sources of the stop word lists in stopwords/ (audit A17): each list is kept with the credit of the
# upstream it matches. Texts are the upstreams' own.
STOPWORD_SOURCES = [
    ("stopwords/{ca,eu,hy,tr}.txt (Apache Lucene)", "Apache-2.0", """ca, eu and hy are Lucene's analysis-common stopwords.txt files for those languages, and tr is
Lucene's Turkish list (LUCENE-559, merged with the list of "Information Retrieval on Turkish
Texts", JASIST 2008) plus 70 words with no recorded source. Lucene's ca list comes from
http://github.com/vcl/cue.language (Apache 2 licensed); its eu and hy lists are example sets.

Apache Lucene
Copyright 2001-2025 The Apache Software Foundation

This product includes software developed at
The Apache Software Foundation (http://www.apache.org/).

The Apache License, Version 2.0 follows.

@APACHE@"""),
    ("stopwords/ro.txt (Jacques Savoy, via Apache Lucene)", "BSD-3-Clause", """ro is Jacques Savoy's Romanian stop word list (http://members.unine.ch/jacques.savoy/clef/index.html)
as carried by Apache Lucene, whose NOTICE says it is "BSD-licensed created by Jacques Savoy"; the
file here adds words not in that list. Savoy's license, as reproduced in Lucene's sources:

"""+SAVOY+""""""),
    ("stopwords/{no,de,hu,it,nl,ru,sv}.txt (Snowball)", "BSD-3-Clause", """no is Snowball's Norwegian list, word for word. de, hu, it, nl, ru and sv contain Snowball's list
(https://snowballstem.org/algorithms/<language>/stop.txt) and add words with no recorded source
(most are in the stopwords-iso aggregate, see below).

"""+SNOWBALL+""""""),
    ("stopwords/{da,es,fi,fr,pt}.txt (stopwords-iso)", "MIT", """These lists are contained in the stopwords-iso collection (https://github.com/stopwords-iso), an
aggregate of web sources listed in its CREDITS.md; no single upstream list was identified, and
the individual sources' terms are not stated there.

"""+ISO+""""""),
    ("stopwords/en.txt (SMART)", "attribution only", """The English list is the SMART system's stop list, by Gerard Salton and Chris Buckley (SMART
information retrieval system, Cornell University, 1971), distributed with the SMART package
(copies cite ftp://ftp.cs.cornell.edu/pub/smart/english.stop). No license text accompanies it, so it
is kept with this attribution."""),
]


def leading_comment(text):
    m = re.match(r"\s*/\*+(.*?)\*+/", text, re.S)
    if not m:
        return None
    lines = [re.sub(r"^\s*\* ?", "", ln).rstrip() for ln in m.group(1).split("\n")]
    return "\n".join(lines).strip("\n")


def copyright_lines(text):
    keep = (ln.strip().lstrip("/ ") for ln in text.split("\n") if re.match(r"^[/\s]*copyright", ln, re.I))
    return "\n".join(keep)


def main(deps, out):
    deps = Path(deps)
    root = Path(__file__).resolve().parent.parent
    o = ["Third-party notices for Xapiand", "=" * 30, "",
         "Xapiand is MIT licensed (LICENSE). The binary and the packages bundle the",
         "components below, fetched at the pins in CMakeLists.txt. This file is",
         "generated by cmake/generate_third_party_notices.py from each component's own",
         "license file; the Kronuz libraries it names are MIT licensed under the same",
         "notice as Xapiand.", ""]

    def section(title, spdx, body):
        o.extend([title, "-" * len(title), "License: " + spdx, "", body.strip("\n"), "", ""])

    for name, spdx, d, mode, path in BUNDLED:
        base = deps / d
        if mode == "file":
            section(name, spdx, (base / path).read_text(encoding="utf-8"))
        elif mode == "lua":
            t = (base / path).read_text(encoding="utf-8")
            m = re.search(r"/\*{20,}\n(\* Copyright.*?)\n\*{20,}/", t, re.S)
            section(name, spdx, re.sub(r"(?m)^\* ?", "", m.group(1)))
        else:
            if d == "asio-src":
                body = copyright_lines((base / path).read_text(encoding="utf-8")) + "\nBoost Software License, Version 1.0 (asio/LICENSE_1_0.txt)."
            elif d == "msgpack-src":
                body = ("Copyright (C) 2008-2015 FURUHASHI Sadayuki and KONDO Takatoshi (msgpack-c); "
                        "Copyright (C) 2001 Housemarque Oy (http://www.housemarque.com), Rene Rivera 2008-2014 (Boost.Preprocessor)\n"
                        "Boost Software License, Version 1.0 (LICENSE_1_0.txt).")
            else:
                body = copyright_lines((base / path).read_text(encoding="utf-8")) + "\nBoost Software License, Version 1.0."
            section(name, spdx, body)

    o.extend(["Third-party code embedded in Kronuz libraries", "=" * 44, "",
              "These libraries keep the notice in a source header only; each is reproduced here.", ""])
    for name, spdx, d, path in HEADERS:
        c = leading_comment((deps / d / path).read_text(encoding="utf-8"))
        section(name, spdx, c or "(no leading comment in %s/%s)" % (d, path))

    o.extend(["Xapiand's own tree", "=" * 18, ""])
    section("y2j (src/y2j)", "MIT", (root / "src/y2j/LICENSE").read_text(encoding="utf-8"))
    section("src/length.{cc,h} and their Python port", "MIT",
            "Copyright (c) 2006-2012 Olly Betts, who granted the MIT license for src/length.{cc,h}.\n"
            "contrib/python/xapiand-py/xapiand/utils/serialize.py is a port of the same routines and "
            "carries his copyright line (it is not in any package).")
    section("cmake/Modules/FindGTest.cmake and FindPackageTools.cmake (CMake)", "BSD-3-Clause", KITWARE)
    section("mime.types (h5bp server-configs-nginx, from nginx)", "MIT AND BSD-2-Clause", MIME_TYPES)
    for name, spdx, body in STOPWORD_SOURCES:
        section(name, spdx, body.replace("@APACHE@", (deps / "prospect-src/LICENSES/Apache-2.0.txt").read_text(encoding="utf-8")))
    o.append("Docs and clients: the documentation and the Python client (contrib/python/xapiand-py,\n"
             "not in any package) are heavily based upon Elasticsearch's, licensed under the\n"
             "Apache License, Version 2.0 (Copyright (c) 2017 Elasticsearch).\n")
    Path(out).write_text("\n".join(o) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
