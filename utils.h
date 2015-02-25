#ifndef XAPIAND_INCLUDED_UTILS_H
#define XAPIAND_INCLUDED_UTILS_H

#include <string>

std::string repr_string(const std::string &string);

void log(void *obj, const char *format, ...);

#endif /* XAPIAND_INCLUDED_UTILS_H */
