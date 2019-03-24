/*
 * Copyright (c) 2015-2019 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

#include "phf.hh"                                 // for phf::*
#include "database/data.h"                        // for ct_type_t
#include "string.hh"                              // for string::lower


const ct_type_t&
mime_type(std::string_view extension)
{
	constexpr static auto _ = phf::make_phf({
		hhl("html"),
		hhl("htm"),
		hhl("shtml"),
		hhl("css"),
		hhl("xml"),
		hhl("gif"),
		hhl("jpeg"),
		hhl("jpg"),
		hhl("js"),
		hhl("atom"),
		hhl("rss"),
		hhl("mml"),
		hhl("txt"),
		hhl("jad"),
		hhl("wml"),
		hhl("htc"),
		hhl("png"),
		hhl("svg"),
		hhl("svgz"),
		hhl("tif"),
		hhl("tiff"),
		hhl("wbmp"),
		hhl("webp"),
		hhl("ico"),
		hhl("jng"),
		hhl("bmp"),
		hhl("woff"),
		hhl("woff2"),
		hhl("jar"),
		hhl("war"),
		hhl("ear"),
		hhl("json"),
		hhl("hqx"),
		hhl("doc"),
		hhl("pdf"),
		hhl("ps"),
		hhl("eps"),
		hhl("ai"),
		hhl("rtf"),
		hhl("m3u8"),
		hhl("kml"),
		hhl("kmz"),
		hhl("xls"),
		hhl("eot"),
		hhl("ppt"),
		hhl("odg"),
		hhl("odp"),
		hhl("ods"),
		hhl("odt"),
		hhl("pptx"),
		hhl("xlsx"),
		hhl("docx"),
		hhl("wmlc"),
		hhl("7z"),
		hhl("cco"),
		hhl("jardiff"),
		hhl("jnlp"),
		hhl("run"),
		hhl("pl"),
		hhl("pm"),
		hhl("prc"),
		hhl("pdb"),
		hhl("rar"),
		hhl("rpm"),
		hhl("sea"),
		hhl("swf"),
		hhl("sit"),
		hhl("tcl"),
		hhl("tk"),
		hhl("der"),
		hhl("pem"),
		hhl("crt"),
		hhl("xpi"),
		hhl("xhtml"),
		hhl("xspf"),
		hhl("zip"),
		hhl("bin"),
		hhl("exe"),
		hhl("dll"),
		hhl("deb"),
		hhl("dmg"),
		hhl("iso"),
		hhl("img"),
		hhl("msi"),
		hhl("msp"),
		hhl("msm"),
		hhl("mid"),
		hhl("midi"),
		hhl("kar"),
		hhl("mp3"),
		hhl("ogg"),
		hhl("m4a"),
		hhl("ra"),
		hhl("3gpp"),
		hhl("3gp"),
		hhl("ts"),
		hhl("mp4"),
		hhl("mpeg"),
		hhl("mpg"),
		hhl("mov"),
		hhl("webm"),
		hhl("flv"),
		hhl("m4v"),
		hhl("mng"),
		hhl("asx"),
		hhl("asf"),
		hhl("wmv"),
		hhl("avi"),
	});

	switch (_.fhhl(string::lower(extension))) {
		case _.fhhl("html"):
		case _.fhhl("htm"):
		case _.fhhl("shtml"): {
			static ct_type_t ct_type("text/html");
			return ct_type;
		}
		case _.fhhl("css"): {
			static ct_type_t ct_type("text/css");
			return ct_type;
		}
		case _.fhhl("xml"): {
			static ct_type_t ct_type("text/xml");
			return ct_type;
		}
		case _.fhhl("gif"): {
			static ct_type_t ct_type("image/gif");
			return ct_type;
		}
		case _.fhhl("jpeg"):
		case _.fhhl("jpg"): {
			static ct_type_t ct_type("image/jpeg");
			return ct_type;
		}
		case _.fhhl("js"): {
			static ct_type_t ct_type("application/javascript");
			return ct_type;
		}
		case _.fhhl("atom"): {
			static ct_type_t ct_type("application/atom+xml");
			return ct_type;
		}
		case _.fhhl("rss"): {
			static ct_type_t ct_type("application/rss+xml");
			return ct_type;
		}
		case _.fhhl("mml"): {
			static ct_type_t ct_type("text/mathml");
			return ct_type;
		}
		case _.fhhl("txt"): {
			static ct_type_t ct_type("text/plain");
			return ct_type;
		}
		case _.fhhl("jad"): {
			static ct_type_t ct_type("text/vnd.sun.j2me.app-descriptor");
			return ct_type;
		}
		case _.fhhl("wml"): {
			static ct_type_t ct_type("text/vnd.wap.wml");
			return ct_type;
		}
		case _.fhhl("htc"): {
			static ct_type_t ct_type("text/x-component");
			return ct_type;
		}
		case _.fhhl("png"): {
			static ct_type_t ct_type("image/png");
			return ct_type;
		}
		case _.fhhl("svg"):
		case _.fhhl("svgz"): {
			static ct_type_t ct_type("image/svg+xml");
			return ct_type;
		}
		case _.fhhl("tif"):
		case _.fhhl("tiff"): {
			static ct_type_t ct_type("image/tiff");
			return ct_type;
		}
		case _.fhhl("wbmp"): {
			static ct_type_t ct_type("image/vnd.wap.wbmp");
			return ct_type;
		}
		case _.fhhl("webp"): {
			static ct_type_t ct_type("image/webp");
			return ct_type;
		}
		case _.fhhl("ico"): {
			static ct_type_t ct_type("image/x-icon");
			return ct_type;
		}
		case _.fhhl("jng"): {
			static ct_type_t ct_type("image/x-jng");
			return ct_type;
		}
		case _.fhhl("bmp"): {
			static ct_type_t ct_type("image/x-ms-bmp");
			return ct_type;
		}
		case _.fhhl("woff"): {
			static ct_type_t ct_type("font/woff");
			return ct_type;
		}
		case _.fhhl("woff2"): {
			static ct_type_t ct_type("font/woff2");
			return ct_type;
		}
		case _.fhhl("jar"):
		case _.fhhl("war"):
		case _.fhhl("ear"): {
			static ct_type_t ct_type("application/java-archive");
			return ct_type;
		}
		case _.fhhl("json"): {
			static ct_type_t ct_type("application/json");
			return ct_type;
		}
		case _.fhhl("hqx"): {
			static ct_type_t ct_type("application/mac-binhex40");
			return ct_type;
		}
		case _.fhhl("doc"): {
			static ct_type_t ct_type("application/msword");
			return ct_type;
		}
		case _.fhhl("pdf"): {
			static ct_type_t ct_type("application/pdf");
			return ct_type;
		}
		case _.fhhl("ps"):
		case _.fhhl("eps"):
		case _.fhhl("ai"): {
			static ct_type_t ct_type("application/postscript");
			return ct_type;
		}
		case _.fhhl("rtf"): {
			static ct_type_t ct_type("application/rtf");
			return ct_type;
		}
		case _.fhhl("m3u8"): {
			static ct_type_t ct_type("application/vnd.apple.mpegurl");
			return ct_type;
		}
		case _.fhhl("kml"): {
			static ct_type_t ct_type("application/vnd.google-earth.kml+xml");
			return ct_type;
		}
		case _.fhhl("kmz"): {
			static ct_type_t ct_type("application/vnd.google-earth.kmz");
			return ct_type;
		}
		case _.fhhl("xls"): {
			static ct_type_t ct_type("application/vnd.ms-excel");
			return ct_type;
		}
		case _.fhhl("eot"): {
			static ct_type_t ct_type("application/vnd.ms-fontobject");
			return ct_type;
		}
		case _.fhhl("ppt"): {
			static ct_type_t ct_type("application/vnd.ms-powerpoint");
			return ct_type;
		}
		case _.fhhl("odg"): {
			static ct_type_t ct_type("application/vnd.oasis.opendocument.graphics");
			return ct_type;
		}
		case _.fhhl("odp"): {
			static ct_type_t ct_type("application/vnd.oasis.opendocument.presentation");
			return ct_type;
		}
		case _.fhhl("ods"): {
			static ct_type_t ct_type("application/vnd.oasis.opendocument.spreadsheet");
			return ct_type;
		}
		case _.fhhl("odt"): {
			static ct_type_t ct_type("application/vnd.oasis.opendocument.text");
			return ct_type;
		}
		case _.fhhl("pptx"): {
			static ct_type_t ct_type("application/vnd.openxmlformats-officedocument.presentationml.presentation");
			return ct_type;
		}
		case _.fhhl("xlsx"): {
			static ct_type_t ct_type("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet");
			return ct_type;
		}
		case _.fhhl("docx"): {
			static ct_type_t ct_type("application/vnd.openxmlformats-officedocument.wordprocessingml.document");
			return ct_type;
		}
		case _.fhhl("wmlc"): {
			static ct_type_t ct_type("application/vnd.wap.wmlc");
			return ct_type;
		}
		case _.fhhl("7z"): {
			static ct_type_t ct_type("application/x-7z-compressed");
			return ct_type;
		}
		case _.fhhl("cco"): {
			static ct_type_t ct_type("application/x-cocoa");
			return ct_type;
		}
		case _.fhhl("jardiff"): {
			static ct_type_t ct_type("application/x-java-archive-diff");
			return ct_type;
		}
		case _.fhhl("jnlp"): {
			static ct_type_t ct_type("application/x-java-jnlp-file");
			return ct_type;
		}
		case _.fhhl("run"): {
			static ct_type_t ct_type("application/x-makeself");
			return ct_type;
		}
		case _.fhhl("pl"):
		case _.fhhl("pm"): {
			static ct_type_t ct_type("application/x-perl");
			return ct_type;
		}
		case _.fhhl("prc"):
		case _.fhhl("pdb"): {
			static ct_type_t ct_type("application/x-pilot");
			return ct_type;
		}
		case _.fhhl("rar"): {
			static ct_type_t ct_type("application/x-rar-compressed");
			return ct_type;
		}
		case _.fhhl("rpm"): {
			static ct_type_t ct_type("application/x-redhat-package-manager");
			return ct_type;
		}
		case _.fhhl("sea"): {
			static ct_type_t ct_type("application/x-sea");
			return ct_type;
		}
		case _.fhhl("swf"): {
			static ct_type_t ct_type("application/x-shockwave-flash");
			return ct_type;
		}
		case _.fhhl("sit"): {
			static ct_type_t ct_type("application/x-stuffit");
			return ct_type;
		}
		case _.fhhl("tcl"):
		case _.fhhl("tk"): {
			static ct_type_t ct_type("application/x-tcl");
			return ct_type;
		}
		case _.fhhl("der"):
		case _.fhhl("pem"):
		case _.fhhl("crt"): {
			static ct_type_t ct_type("application/x-x509-ca-cert");
			return ct_type;
		}
		case _.fhhl("xpi"): {
			static ct_type_t ct_type("application/x-xpinstall");
			return ct_type;
		}
		case _.fhhl("xhtml"): {
			static ct_type_t ct_type("application/xhtml+xml");
			return ct_type;
		}
		case _.fhhl("xspf"): {
			static ct_type_t ct_type("application/xspf+xml");
			return ct_type;
		}
		case _.fhhl("zip"): {
			static ct_type_t ct_type("application/zip");
			return ct_type;
		}
		case _.fhhl("bin"):
		case _.fhhl("exe"):
		case _.fhhl("dll"): {
			static ct_type_t ct_type("application/octet-stream");
			return ct_type;
		}
		case _.fhhl("deb"): {
			static ct_type_t ct_type("application/octet-stream");
			return ct_type;
		}
		case _.fhhl("dmg"): {
			static ct_type_t ct_type("application/octet-stream");
			return ct_type;
		}
		case _.fhhl("iso"):
		case _.fhhl("img"): {
			static ct_type_t ct_type("application/octet-stream");
			return ct_type;
		}
		case _.fhhl("msi"):
		case _.fhhl("msp"):
		case _.fhhl("msm"): {
			static ct_type_t ct_type("application/octet-stream");
			return ct_type;
		}
		case _.fhhl("mid"):
		case _.fhhl("midi"):
		case _.fhhl("kar"): {
			static ct_type_t ct_type("audio/midi");
			return ct_type;
		}
		case _.fhhl("mp3"): {
			static ct_type_t ct_type("audio/mpeg");
			return ct_type;
		}
		case _.fhhl("ogg"): {
			static ct_type_t ct_type("audio/ogg");
			return ct_type;
		}
		case _.fhhl("m4a"): {
			static ct_type_t ct_type("audio/x-m4a");
			return ct_type;
		}
		case _.fhhl("ra"): {
			static ct_type_t ct_type("audio/x-realaudio");
			return ct_type;
		}
		case _.fhhl("3gpp"):
		case _.fhhl("3gp"): {
			static ct_type_t ct_type("video/3gpp");
			return ct_type;
		}
		case _.fhhl("ts"): {
			static ct_type_t ct_type("video/mp2t");
			return ct_type;
		}
		case _.fhhl("mp4"): {
			static ct_type_t ct_type("video/mp4");
			return ct_type;
		}
		case _.fhhl("mpeg"):
		case _.fhhl("mpg"): {
			static ct_type_t ct_type("video/mpeg");
			return ct_type;
		}
		case _.fhhl("mov"): {
			static ct_type_t ct_type("video/quicktime");
			return ct_type;
		}
		case _.fhhl("webm"): {
			static ct_type_t ct_type("video/webm");
			return ct_type;
		}
		case _.fhhl("flv"): {
			static ct_type_t ct_type("video/x-flv");
			return ct_type;
		}
		case _.fhhl("m4v"): {
			static ct_type_t ct_type("video/x-m4v");
			return ct_type;
		}
		case _.fhhl("mng"): {
			static ct_type_t ct_type("video/x-mng");
			return ct_type;
		}
		case _.fhhl("asx"):
		case _.fhhl("asf"): {
			static ct_type_t ct_type("video/x-ms-asf");
			return ct_type;
		}
		case _.fhhl("wmv"): {
			static ct_type_t ct_type("video/x-ms-wmv");
			return ct_type;
		}
		case _.fhhl("avi"): {
			static ct_type_t ct_type("video/x-msvideo");
			return ct_type;
		}
	}
	static ct_type_t ct_type;
	return ct_type;
}
