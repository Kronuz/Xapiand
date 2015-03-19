/*
 * Copyright deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "pthread.h"
#include "utils.h"


pthread_mutex_t qmtx = PTHREAD_MUTEX_INITIALIZER;


std::string repr(const std::string &string)
{
	return repr(string.c_str(), string.size());
}


std::string repr(const char * p, size_t size)
{
	char *buff = new char[size * 4 + 1];
	char *d = buff;
	const char *p_end = p + size;
	while (p != p_end) {
		char c = *p++;
		if (c == 9) {
			*d++ = '\\';
			*d++ = 't';
		} else if (c == 10) {
			*d++ = '\\';
			*d++ = 'n';
		} else if (c == 13) {
			*d++ = '\\';
			*d++ = 'r';
		} else if (c == '\'') {
			*d++ = '\\';
			*d++ = '\'';
		} else if (c >= ' ' && c <= '~') {
			*d++ = c;
		} else {
			*d++ = '\\';
			*d++ = 'x';
			sprintf(d, "%02x", (unsigned char)c);
			d += 2;
		}
//		 printf("%02x: %ld < %ld\n", (unsigned char)c, (unsigned long)(d - buff), (unsigned long)(size * 4 + 1));
	}
	*d = '\0';
	std::string ret(buff);
	delete [] buff;
	return ret;
}


void log(void *obj, const char *format, ...)
{
	pthread_mutex_lock(&qmtx);

	FILE * file = stderr;
	fprintf(file, "tid(0x%lx): 0x%lx - ", (unsigned long)pthread_self(), (unsigned long)obj);
	va_list argptr;
	va_start(argptr, format);
	vfprintf(file, format, argptr);
	va_end(argptr);

	pthread_mutex_unlock(&qmtx);
}

const char HEX2DEC[256] =
{
    /*       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
    /* 0 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 1 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 2 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 3 */  0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,
    
    /* 4 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 5 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 6 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 7 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    
    /* 8 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* 9 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* A */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* B */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    
    /* C */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* D */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* E */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
    /* F */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
};

std::string urldecode(const char *src, size_t size)
{
    // Note from RFC1630:  "Sequences which start with a percent sign
    // but are not followed by two hexadecimal characters (0-9, A-F) are reserved
    // for future extension"
    
    const char * SRC_END = src + size;
    const char * SRC_LAST_DEC = SRC_END - 2;   // last decodable '%'
    
    char * const pStart = new char[size];
    char * pEnd = pStart;
    
    while (src < SRC_LAST_DEC)
    {
        if (*src == '%')
        {
            char dec1, dec2;
            if (-1 != (dec1 = HEX2DEC[*(src + 1)])
                && -1 != (dec2 = HEX2DEC[*(src + 2)]))
            {
                *pEnd++ = (dec1 << 4) + dec2;
                src += 3;
                continue;
            }
        }
        
        *pEnd++ = *src++;
    }
    
    // the last 2- chars
    while (src < SRC_END)
    *pEnd++ = *src++;
    
    std::string sResult(pStart, pEnd);
    delete [] pStart;
    std::replace( sResult.begin(), sResult.end(), '+', ' ');
    return sResult;
}

int url_qs(const char *name, const char *qs, parser_query *par)
{
    
    const char *n1, *n0;
    const char *v0 = NULL;
    
    if(par->offset == NULL) {
        n0 = n1 = qs;
    } else {
        n0 = n1 = par->offset + par -> length + 1;
    }
    
    while (1) {
        switch (*n1) {
            case '=' :
            v0 = n1;
            case '\0':
            case '&' :
            case ';' :
            if(strlen(name) == n1 - n0 && strncmp(n0, name, n1 - n0) == 0) {
                if (v0) {
                    const char *v1 = v0 + 1;
                    while (1) {
                        switch(*v1) {
                            case '\0':
                            case '&' :
                            case ';' :
                            par->offset = v0 + 1;
                            par->length = v1 - v0 - 1;
                            return 0;
                        }
                        v1++;
                    }
                } else {
                    par->offset = n1 + 1;
                    par->length = 0;
                    return 0;
                }
            } else if (!*n1) {
                return -1;
            } else if (*n1 != '=') {
                n0 = n1 + 1;
                v0 = NULL;
            }
        }
        n1++;
    }
    return -1;
}

int url_path(const char** n1, parser_url_path *par)
{
    const char *n0, *r, *p = NULL;
    size_t cmd_size = 0;
    int state = 0;
    n0 = *n1;
    
    bool other_slash = false;
    par->off_host = NULL;
    par->len_host = 0;
    par->off_command = NULL;
    par->len_command = 0;
    
    while (1) {
        switch(**n1) {
            case '\0':
                if (n0 == *n1) return -1;
                if (p) {
                    r = p + 1;
                    while(*r) {
                        switch (*r) {
                            case '/':
                                r++;
                                continue;
                                break;
                                
                            default:
                                cmd_size++;
                                r++;
                                break;
                        }
                    }
                    par->off_command = p + 1;
                    par->len_command = cmd_size;
                }
            case ',':
                if (!p) p = *n1;
                switch (state) {
                    case 0:
                    case 1:
                        par->off_path = n0;
                        par->len_path = p - n0;
                        if (**n1) (*n1)++;
                        return 0;
                    case 2:
                        par->off_host = n0;
                        par->len_host = p - n0;
                        if (**n1) (*n1)++;
                        return 0;
                }
                p = NULL;
                other_slash = false;
                break;
                
            case ':':
                switch (state) {
                    case 0:
                        par->off_namespace = n0;
                        par->len_namespace = *n1 - n0;
                        state = 1;
                        n0 = *n1 + 1;
                        break;
                    default:
                        state = -1;
                }
                p = NULL;
                other_slash = false;
                break;
                
            case '@':
                switch (state) {
                    case 0:
                        par->off_path = n0;
                        par->len_path = *n1 - n0;
                        state = 2;
                        n0 = *n1 + 1;
                        break;
                    case 1:
                        par->off_path = n0;
                        par->len_path = *n1 - n0;
                        state = 2;
                        n0 = *n1 + 1;
                        break;
                    default:
                        state = -1;
                }
                p = NULL;
                other_slash = false;
                break;
                
                
            case '/':
                if(*(*n1 + 1) && !p && !other_slash) {
                    p = *n1;
                    other_slash = true;
                }else if(*(*n1 + 1) && *(*n1 + 1) != '/') {
                    p = *n1;
                    other_slash = true;
                }
                
        }
        (*n1)++;
    }
    return -1;
}

int look_cmd(const char *cmd)
{
    if (strcasecmp(cmd,"_search")==0){ return(command_search);}
    if (strcasecmp(cmd,"_count")==0){ return(command_count);}
    if (strcasecmp(cmd,"_facets")==0){ return(command_facets);}
    if (strcasecmp(cmd,"_similar")==0){ return(command_similar);}
    return(identifier);
}
