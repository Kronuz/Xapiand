/** @file version.h
 * @brief Define preprocessor symbols for the library version
 */
// Copyright (C) 2002,2004,2005,2006,2007,2008,2009,2010,2011,2012,2013,2015,2016,2017,2018 Olly Betts
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

#ifndef XAPIAN_INCLUDED_VERSION_H
#define XAPIAN_INCLUDED_VERSION_H

/// The library was compiled with GCC's -fvisibility=hidden option.
#define XAPIAN_ENABLE_VISIBILITY

/// The version of Xapian as a C string literal.
#define XAPIAN_VERSION "1.5.0"

/** The major component of the Xapian version.
 * E.g. for Xapian 1.0.14 this would be: 1
 */
#define XAPIAN_MAJOR_VERSION 1

/** The minor component of the Xapian version.
 * E.g. for Xapian 1.0.14 this would be: 0
 */
#define XAPIAN_MINOR_VERSION 5

/** The revision component of the Xapian version.
 * E.g. for Xapian 1.0.14 this would be: 14
 */
#define XAPIAN_REVISION 0

/// Base (signed) type for Xapian::docid and related types.
#define XAPIAN_DOCID_BASE_TYPE int

/// Base (signed) type for Xapian::termcount and related types.
#define XAPIAN_TERMCOUNT_BASE_TYPE int

/// Base (signed) type for Xapian::termpos.
#define XAPIAN_TERMPOS_BASE_TYPE int

/// Type for returning total document length.
#define XAPIAN_TOTALLENGTH_TYPE unsigned long

/// Underlying type for Xapian::rev.
#define XAPIAN_REVISION_TYPE unsigned long

/// XAPIAN_HAS_GLASS_BACKEND Defined if the glass backend is enabled.
#define XAPIAN_HAS_GLASS_BACKEND 1

/// XAPIAN_HAS_HONEY_BACKEND Defined if the honey backend is enabled.
/* #undef XAPIAN_HAS_HONEY_BACKEND */

/// XAPIAN_HAS_INMEMORY_BACKEND Defined if the inmemory backend is enabled.
#define XAPIAN_HAS_INMEMORY_BACKEND 1

/// XAPIAN_HAS_REMOTE_BACKEND Defined if the remote backend is enabled.
#define XAPIAN_HAS_REMOTE_BACKEND 1

/// XAPIAN_AT_LEAST(A,B,C) checks for xapian-core >= A.B.C - use like so:
///
/// @code
/// #if XAPIAN_AT_LEAST(1,4,2)
/// /* Code needing features needing Xapian >= 1.4.2. */
/// #endif
/// @endcode
///
/// Added in Xapian 1.4.2.
#define XAPIAN_AT_LEAST(A,B,C) \
 (XAPIAN_MAJOR_VERSION > (A) || \
 (XAPIAN_MAJOR_VERSION == (A) && \
 (XAPIAN_MINOR_VERSION > (B) || \
 (XAPIAN_MINOR_VERSION == (B) && XAPIAN_REVISION >= (C)))))

#endif /* XAPIAN_INCLUDED_VERSION_H */
