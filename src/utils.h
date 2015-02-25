#ifndef XAPIAND_INCLUDED_UTILS_H
#define XAPIAND_INCLUDED_UTILS_H

#include <string>

void print_string(const std::string &string);
void fprint_string(FILE * file, const std::string &string);

void log(void *obj, const char *format, ...);

#endif /* XAPIAND_INCLUDED_UTILS_H */
