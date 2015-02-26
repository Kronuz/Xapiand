#ifndef XAPIAND_INCLUDED_UTILS_H
#define XAPIAND_INCLUDED_UTILS_H

#include <string>

void log(void *obj, const char *fmt, ...);

std::string repr(const char *p, size_t size);
std::string repr(const std::string &string);

#define LOG(...) log(__VA_ARGS__)
#define LOG_EV(...) log(__VA_ARGS__)
#define LOG_CONN(...) log(__VA_ARGS__)
#define LOG_OBJ(...) log(__VA_ARGS__)
#define LOG_ERR(...) log(__VA_ARGS__)
#define LOG_HTTP_PROTO(...) log(__VA_ARGS__)
#define LOG_HTTP_PROTO_PARSER(...)
#define LOG_BINARY_PROTO(...) log(__VA_ARGS__)
#define LOG_DATABASE(...)

#endif /* XAPIAND_INCLUDED_UTILS_H */
