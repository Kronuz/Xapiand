/** @file dbfactory_remote.cc
 * @brief Database factories for remote databases.
 */
/* Copyright (C) 2006,2007,2008,2010,2011,2014 Olly Betts
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#include "xapian/dbfactory.h"

#include "xapian/common/debuglog.h"
#include "xapian/net/progclient.h"
#include "xapian/net/remotetcpclient.h"

#include <string>

using namespace std;

namespace Xapian {

Database
Remote::open(const string &host, unsigned int port, unsigned timeout_,
	     unsigned connect_timeout, int flags, const string &dir)
{
    LOGCALL_STATIC(API, Database, "Remote::open", host | port | timeout_ | connect_timeout | flags | dir);
    RETURN(Database(new RemoteTcpClient(host, port, timeout_ * 1e-3,
					connect_timeout * 1e-3, false, flags, dir)));
}

Database
Remote::open(const string &host, unsigned int port, unsigned timeout_,
	     unsigned connect_timeout)
{
	RETURN(Remote::open(host, port, timeout_, connect_timeout, 0, string()));
}

WritableDatabase
Remote::open_writable(const string &host, unsigned int port,
		      unsigned timeout_, unsigned connect_timeout,
		      int flags, const string &dir)
{
    LOGCALL_STATIC(API, WritableDatabase, "Remote::open_writable", host | port | timeout_ | connect_timeout | flags | dir);
    RETURN(WritableDatabase(new RemoteTcpClient(host, port, timeout_ * 1e-3,
						connect_timeout * 1e-3, true,
						flags, dir)));
}

WritableDatabase
Remote::open_writable(const string &host, unsigned int port,
		      unsigned timeout_, unsigned connect_timeout,
		      int flags)
{
	RETURN(Remote::open_writable(host, port, timeout_, connect_timeout, flags, string()));
}

Database
Remote::open(const string &program, const string &args,
	     unsigned timeout_, int flags, const string &dir)
{
    LOGCALL_STATIC(API, Database, "Remote::open", program | args | timeout_ | flags | dir);
    RETURN(Database(new ProgClient(program, args, timeout_ * 1e-3, false, flags, dir)));
}

Database
Remote::open(const string &program, const string &args,
	     unsigned timeout_)
{
	RETURN(Remote::open(program, args, timeout_, 0, string()));
}

WritableDatabase
Remote::open_writable(const string &program, const string &args,
		      unsigned timeout_, int flags, const string &dir)
{
    LOGCALL_STATIC(API, WritableDatabase, "Remote::open_writable", program | args | timeout_ | flags | dir);
    RETURN(WritableDatabase(new ProgClient(program, args,
					   timeout_ * 1e-3, true, flags, dir)));
}

WritableDatabase
Remote::open_writable(const string &program, const string &args,
		      unsigned timeout_, int flags)
{
	RETURN(open_writable(program, args, timeout_, flags, string()));
}

}
