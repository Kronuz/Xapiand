#ifndef XAPIAND_INCLUDED_URI_H
#define XAPIAND_INCLUDED_URI_H

#include <iostream>
#include <string>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>


static inline char * normalize_path(const char * src, char * dst)
{
	int levels = 0;
	char * ret = dst;
	for (int i = 0; *src && i < PATH_MAX; i++) {
		char ch = *src++;
		if (ch == '.' && (levels || dst == ret || *(dst - 1) == '/' )) {
			*dst++ = ch;
			levels++;
		} else if (ch == '/') {
			while (levels && dst > ret) {
				if (*--dst == '/') levels -= 1;
			}
			if (dst == ret || *(dst - 1) != '/') {
				*dst++ = ch;
			}
		} else {
			*dst++ = ch;
			levels = 0;
		}
	}
	*dst++ = '\0';
	return ret;
}


class Endpoint {
	static inline std::string slice_after(std::string &subject, std::string delimiter) {
		size_t delimiter_location = subject.find(delimiter);
		size_t delimiter_length = delimiter.length();
		std::string output = "";
		if (delimiter_location < std::string::npos) {
			size_t start = delimiter_location + delimiter_length;
			output = subject.substr(start, subject.length() - start);
			subject = subject.substr(0, delimiter_location);
		}
		return output;
	}

	static inline std::string slice_before(std::string &subject, std::string delimiter) {
		size_t delimiter_location = subject.find(delimiter);
		size_t delimiter_length = delimiter.length();
		std::string output = "";
		if (delimiter_location < std::string::npos) {
			size_t start = delimiter_location + delimiter_length;
			output = subject.substr(0, delimiter_location);
			subject = subject.substr(start, subject.length() - start);
		}
		return output;
	}

public:
	int port;
	std::string protocol, user, password, host, path, search;

	Endpoint(const std::string &uri, const std::string &base_, int port_) {
		std::string in(uri);
		std::string base;
		char actualpath[PATH_MAX + 1];
		if (base.empty()) {
			getcwd(actualpath, PATH_MAX);
			base.assign(actualpath);
		} else {
			base = base_;
		}
		protocol = slice_before(in, "://");
		if (protocol.empty()) {
			protocol = "file";
		}
		search = slice_after(in, "?");
		path = slice_after(in, "/");
		std::string userpass = slice_before(in, "@");
		password = slice_after(userpass, ":");
		user = userpass;
		std::string portstring = slice_after(in, ":");
		port = atoi(portstring.c_str());
		if (protocol.empty() || protocol == "file") {
			path = in + "/" + path;
			port = 0;
			search = "";
			password = "";
			user = "";
		} else {
			host = in;
			if (!port) port = port_;
		}
		path = actualpath + path;
		normalize_path(path.c_str(), actualpath);
		path = actualpath;
		if (path.substr(0, base.size()) == base) {
			path.erase(0, base.size());
		} else {
			path = "";
		}
	}

	std::string as_string() const {
		std::string ret;
		if (path.empty()) {
			return ret;
		}
		ret += protocol + "://";
		if (!user.empty() || !password.empty()) {
			ret += user;
			if (!password.empty()) {
				ret += ":" + password;
			}
			ret += "@";
		}
		ret += host;
		if (port > 0) {
			char port_[100];
			sprintf(port_, "%d", port);
			ret += ":";
			ret += port_;
		}
		ret += path;
		if (!search.empty()) {
			ret += "?" + search;
		}
		return ret;
	}

	bool operator< (const Endpoint & other) const
	{
		return as_string() < other.as_string();
	}
};

class Endpoints : public std::vector<Endpoint> {
public:
	size_t hash(bool writable) const {
		std::vector<Endpoint> copy;
		copy.assign(begin(), end());
		std::sort(copy.begin(), copy.end());
		std::string es = std::string(writable ? "1" : "0");
		std::vector<Endpoint>::const_iterator j(copy.begin());
		for (; j != copy.end(); j++) {
			es += ":";
			es += (*j).as_string().c_str();
		}
		std::hash<std::string> hash_fn;
		return hash_fn(es);
	}
};

#endif /* XAPIAND_INCLUDED_URI_H */
