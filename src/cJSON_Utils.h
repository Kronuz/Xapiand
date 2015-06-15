/*
 Copyright (c) 2015 Kitware, Inc.

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 */

#ifndef __Xapiand__cJSON_Utils__
#define __Xapiand__cJSON_Utils__

#include "cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

// Implement RFC6901 (https://tools.ietf.org/html/rfc6901) JSON Pointer spec.
cJSON *cJSONUtils_GetPointer(cJSON *object,const char *pointer);

// Implement RFC6902 (https://tools.ietf.org/html/rfc6902) JSON Patch spec.
cJSON* cJSONUtils_GeneratePatches(cJSON *from,cJSON *to);
void cJSONUtils_AddPatchToArray(cJSON *array,const char *op,const char *path,cJSON *val);	// Utility for generating patch array entries.
int cJSONUtils_ApplyPatches(cJSON *object,cJSON *patches);	// Returns 0 for success.

// Note that ApplyPatches is NOT atomic on failure. To implement an atomic ApplyPatches, use:
//int cJSONUtils_AtomicApplyPatches(cJSON **object, cJSON *patches)
//{
//	cJSON *modme=cJSON_Duplicate(*object,1);
//	int error=cJSONUtils_ApplyPatches(modme,patches);
//	if (!error)	{cJSON_Delete(*object);*object=modme;}
//	else		cJSON_Delete(modme);
//	return error;
//}
// Code not added to library since this strategy is a LOT slower.

char *cJSONUtils_FindPointerFromObjectTo(cJSON *object,cJSON *target);	// Given a root object and a target object, construct a pointer from one to the other.

#ifdef __cplusplus
}
#endif

#endif /* defined(__Xapiand__cJSON_Utils__) */
