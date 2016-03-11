#!/bin/sh

if [ -d "/usr/local/share/aclocal/" ]; then
	# Try to get xapian-1.3.m4 in case xapian has been installed in /usr/local/
	cp /usr/local/share/aclocal/xapian-1.3.m4 m4/xapian-1.3.m4
fi

aclocal -I m4 --install
autoreconf --force --install --verbose
