/** @file sortable-serialise.cc
 * @brief Serialise floating point values to string which sort the same way.
 */
/* Copyright (C) 2007,2009,2015,2016 Olly Betts
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#pragma once


namespace Xapian {

	size_t sortable_serialise_(long double value, char * buf);

	long double sortable_unserialise_long(const std::string & value);

	inline std::string sortable_serialise_long(long double value) {
    	char buf[18];
    	return std::string(buf, sortable_serialise_(value, buf));
	}

	/// Get a number from the character at a given position in a string, returning
	/// 0 if the string isn't long enough.
	static inline unsigned char
	numfromstr(const std::string & str, std::string::size_type pos)
	{
		return (pos < str.size()) ? static_cast<unsigned char>(str[pos]) : '\0';
	}
}