// The pinned replies of requests(), in order: status and leading bytes of the body.
// Known defects, each pinned with its current reply (a fix changes the pin):
//   H1 a non-positive or fractional _weight, _position or _slot is a 500, not a 400
//   H2 an array as the _value of an object-typed field is a 500 ("MAP cannot have numeric keys")
//   H3 a string as _schema is a 500 ("Schema metadata is corrupt"), not a 400
//   H4 _namespace with nested objects is a 500 ("Schema has an unsupported type: integer")
//   H5 PATCH replace on a missing path answers with the internal "unordered_map::at" message (pinned up to the library's text)
//   Hash-derived "_slot" numbers are cut from the pins: only the keys around them are defined
const std::vector<Pin> pins = {
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_weight' must be a positive integer or a not-empty array of positive integers\"}"},  // PUT /pr/1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_weight' must be a positive integer or a not-empty array of positive integers\"}"},  // PUT /pr/2
	{204, ""},  // PUT /pr/3
	{500, "{\"status\":500,\"type\":\"Internal Server Error\",\"message\":\"Negative value of NEGATIVE_INTEGER is not a valid POSITIVE_INTEGER\"}"},  // PUT /pr/4  HUNT H1
	{500, "{\"status\":500,\"type\":\"Internal Server Error\",\"message\":\"Value of FLOAT is not a valid POSITIVE_INTEGER\"}"},  // PUT /pr/5  HUNT H1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_weight' must be a positive integer or a not-empty array of positive integers\"}"},  // PUT /pr/6
	{204, ""},  // PUT /pr/7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_weight' must be a positive integer or a not-empty array of positive integers\"}"},  // PUT /pr/8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_weight' must be a positive integer or a not-empty array of positive integers\"}"},  // PUT /pr/9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_weight' must be a positive integer or a not-empty array of positive integers\"}"},  // PUT /pr/10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_position' must be a positive integer or a not-empty array of positive integers\"}"},  // PUT /pr/11
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_position' must be a positive integer or a not-empty array of positive integers\"}"},  // PUT /pr/12
	{204, ""},  // PUT /pr/13
	{500, "{\"status\":500,\"type\":\"Internal Server Error\",\"message\":\"Negative value of NEGATIVE_INTEGER is not a valid POSITIVE_INTEGER\"}"},  // PUT /pr/14  HUNT H1
	{500, "{\"status\":500,\"type\":\"Internal Server Error\",\"message\":\"Value of FLOAT is not a valid POSITIVE_INTEGER\"}"},  // PUT /pr/15  HUNT H1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_position' must be a positive integer or a not-empty array of positive integers\"}"},  // PUT /pr/16
	{204, ""},  // PUT /pr/17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_position' must be a positive integer or a not-empty array of positive integers\"}"},  // PUT /pr/18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_position' must be a positive integer or a not-empty array of positive integers\"}"},  // PUT /pr/19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_position' must be a positive integer or a not-empty array of positive integers\"}"},  // PUT /pr/20
	{204, ""},  // PUT /pr/21
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_spelling' must be a boolean or a not-empty array of booleans\"}"},  // PUT /pr/22
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_spelling' must be a boolean or a not-empty array of booleans\"}"},  // PUT /pr/23
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_spelling' must be a boolean or a not-empty array of booleans\"}"},  // PUT /pr/24
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_spelling' must be a boolean or a not-empty array of booleans\"}"},  // PUT /pr/25
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_spelling' must be a boolean or a not-empty array of booleans\"}"},  // PUT /pr/26
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_spelling' must be a positive integer or a not-empty array of positive integers\"}"},  // PUT /pr/27
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_spelling' must be a positive integer or a not-empty array of positive integers\"}"},  // PUT /pr/28
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_spelling' must be a boolean or a not-empty array of booleans\"}"},  // PUT /pr/29
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_spelling' must be a boolean or a not-empty array of booleans\"}"},  // PUT /pr/30
	{204, ""},  // PUT /pr/31
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_positions' must be a boolean or a not-empty array of booleans\"}"},  // PUT /pr/32
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_positions' must be a boolean or a not-empty array of booleans\"}"},  // PUT /pr/33
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_positions' must be a boolean or a not-empty array of booleans\"}"},  // PUT /pr/34
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_positions' must be a boolean or a not-empty array of booleans\"}"},  // PUT /pr/35
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_positions' must be a boolean or a not-empty array of booleans\"}"},  // PUT /pr/36
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_positions' must be a boolean or a not-empty array of booleans\"}"},  // PUT /pr/37
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_positions' must be a boolean or a not-empty array of booleans\"}"},  // PUT /pr/38
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_positions' must be a boolean or a not-empty array of booleans\"}"},  // PUT /pr/39
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_positions' must be a boolean or a not-empty array of booleans\"}"},  // PUT /pr/40
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_language' must be string\"}"},  // PUT /pr/41
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: It is not allowed to change '_language' [  ->  's'] in 'f'\"}"},  // PUT /pr/42
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_language' must be string\"}"},  // PUT /pr/43
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_language' must be string\"}"},  // PUT /pr/44
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_language' must be string\"}"},  // PUT /pr/45
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_language' must be string\"}"},  // PUT /pr/46
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_language' must be string\"}"},  // PUT /pr/47
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_language' must be string\"}"},  // PUT /pr/48
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_language' must be string\"}"},  // PUT /pr/49
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_language' must be string\"}"},  // PUT /pr/50
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: It is not allowed to change '_ngram' [false  ->  true] in 'f'\"}"},  // PUT /pr/51
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_ngram' must be string\"}"},  // PUT /pr/52
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_ngram' must be string\"}"},  // PUT /pr/53
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_ngram' must be string\"}"},  // PUT /pr/54
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_ngram' must be string\"}"},  // PUT /pr/55
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_ngram' must be string\"}"},  // PUT /pr/56
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_ngram' must be string\"}"},  // PUT /pr/57
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_ngram' must be string\"}"},  // PUT /pr/58
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_ngram' must be string\"}"},  // PUT /pr/59
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_ngram' must be string\"}"},  // PUT /pr/60
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: It is not allowed to change '_cjk_ngram' [false  ->  true] in 'f'\"}"},  // PUT /pr/61
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_cjk_ngram' must be string\"}"},  // PUT /pr/62
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_cjk_ngram' must be string\"}"},  // PUT /pr/63
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_cjk_ngram' must be string\"}"},  // PUT /pr/64
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_cjk_ngram' must be string\"}"},  // PUT /pr/65
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_cjk_ngram' must be string\"}"},  // PUT /pr/66
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_cjk_ngram' must be string\"}"},  // PUT /pr/67
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_cjk_ngram' must be string\"}"},  // PUT /pr/68
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_cjk_ngram' must be string\"}"},  // PUT /pr/69
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_cjk_ngram' must be string\"}"},  // PUT /pr/70
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_accuracy' must be array\"}"},  // PUT /pr/71
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_accuracy' must be array\"}"},  // PUT /pr/72
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_accuracy' must be array\"}"},  // PUT /pr/73
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_accuracy' must be array\"}"},  // PUT /pr/74
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_accuracy' must be array\"}"},  // PUT /pr/75
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: '_accuracy' is not allowed in text type fields\"}"},  // PUT /pr/76
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: '_accuracy' is not allowed in text type fields\"}"},  // PUT /pr/77
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: '_accuracy' is not allowed in text type fields\"}"},  // PUT /pr/78
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_accuracy' must be array\"}"},  // PUT /pr/79
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_accuracy' must be array\"}"},  // PUT /pr/80
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_accuracy_prefix' in 'f' is not valid\"}"},  // PUT /pr/81
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_accuracy_prefix' in 'f' is not valid\"}"},  // PUT /pr/82
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_accuracy_prefix' in 'f' is not valid\"}"},  // PUT /pr/83
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_accuracy_prefix' in 'f' is not valid\"}"},  // PUT /pr/84
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_accuracy_prefix' in 'f' is not valid\"}"},  // PUT /pr/85
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_accuracy_prefix' in 'f' is not valid\"}"},  // PUT /pr/86
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_accuracy_prefix' in 'f' is not valid\"}"},  // PUT /pr/87
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_accuracy_prefix' in 'f' is not valid\"}"},  // PUT /pr/88
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_accuracy_prefix' in 'f' is not valid\"}"},  // PUT /pr/89
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_accuracy_prefix' in 'f' is not valid\"}"},  // PUT /pr/90
	{204, ""},  // PUT /pr/91
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_store' must be boolean\"}"},  // PUT /pr/92
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_store' must be boolean\"}"},  // PUT /pr/93
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_store' must be boolean\"}"},  // PUT /pr/94
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_store' must be boolean\"}"},  // PUT /pr/95
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_store' must be boolean\"}"},  // PUT /pr/96
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_store' must be boolean\"}"},  // PUT /pr/97
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_store' must be boolean\"}"},  // PUT /pr/98
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_store' must be boolean\"}"},  // PUT /pr/99
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_store' must be boolean\"}"},  // PUT /pr/100
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_type' must be string\"}"},  // PUT /pr/101
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: It is not allowed to change '_type' ['text'  ->  's'] in 'f'\"}"},  // PUT /pr/102
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_type' must be string\"}"},  // PUT /pr/103
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_type' must be string\"}"},  // PUT /pr/104
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_type' must be string\"}"},  // PUT /pr/105
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_type' must be string\"}"},  // PUT /pr/106
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_type' must be string\"}"},  // PUT /pr/107
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_type' must be string\"}"},  // PUT /pr/108
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_type' must be string\"}"},  // PUT /pr/109
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_type' must be string\"}"},  // PUT /pr/110
	{204, ""},  // PUT /pr/111
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_dynamic' must be boolean\"}"},  // PUT /pr/112
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_dynamic' must be boolean\"}"},  // PUT /pr/113
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_dynamic' must be boolean\"}"},  // PUT /pr/114
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_dynamic' must be boolean\"}"},  // PUT /pr/115
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_dynamic' must be boolean\"}"},  // PUT /pr/116
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_dynamic' must be boolean\"}"},  // PUT /pr/117
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_dynamic' must be boolean\"}"},  // PUT /pr/118
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_dynamic' must be boolean\"}"},  // PUT /pr/119
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_dynamic' must be boolean\"}"},  // PUT /pr/120
	{204, ""},  // PUT /pr/121
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_strict' must be boolean\"}"},  // PUT /pr/122
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_strict' must be boolean\"}"},  // PUT /pr/123
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_strict' must be boolean\"}"},  // PUT /pr/124
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_strict' must be boolean\"}"},  // PUT /pr/125
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_strict' must be boolean\"}"},  // PUT /pr/126
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_strict' must be boolean\"}"},  // PUT /pr/127
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_strict' must be boolean\"}"},  // PUT /pr/128
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_strict' must be boolean\"}"},  // PUT /pr/129
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_strict' must be boolean\"}"},  // PUT /pr/130
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_slot' must be integer\"}"},  // PUT /pr/131
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_slot' must be integer\"}"},  // PUT /pr/132
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: It is not allowed to change '_slot' [1009059176  ->  7] in 'f'\"}"},  // PUT /pr/133
	{500, "{\"status\":500,\"type\":\"Internal Server Error\",\"message\":\"Negative value of NEGATIVE_INTEGER is not a valid POSITIVE_INTEGER\"}"},  // PUT /pr/134  HUNT H1
	{500, "{\"status\":500,\"type\":\"Internal Server Error\",\"message\":\"Value of FLOAT is not a valid POSITIVE_INTEGER\"}"},  // PUT /pr/135  HUNT H1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_slot' must be integer\"}"},  // PUT /pr/136
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_slot' must be integer\"}"},  // PUT /pr/137
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_slot' must be integer\"}"},  // PUT /pr/138
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_slot' must be integer\"}"},  // PUT /pr/139
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_slot' must be integer\"}"},  // PUT /pr/140
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_index' must be string\"}"},  // PUT /pr/141
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 's' not supported, '_index' must be one of none, field_terms, field_values, field_terms,field_values, field_values,field_terms, field, field_"},  // PUT /pr/142
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_index' must be string\"}"},  // PUT /pr/143
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_index' must be string\"}"},  // PUT /pr/144
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_index' must be string\"}"},  // PUT /pr/145
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_index' must be string\"}"},  // PUT /pr/146
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_index' must be string\"}"},  // PUT /pr/147
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_index' must be string\"}"},  // PUT /pr/148
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_index' must be string\"}"},  // PUT /pr/149
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_index' must be string\"}"},  // PUT /pr/150
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_prefix' in 'f' is not valid\"}"},  // PUT /pr/151
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_prefix' in 'f' is not valid\"}"},  // PUT /pr/152
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_prefix' in 'f' is not valid\"}"},  // PUT /pr/153
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_prefix' in 'f' is not valid\"}"},  // PUT /pr/154
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_prefix' in 'f' is not valid\"}"},  // PUT /pr/155
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_prefix' in 'f' is not valid\"}"},  // PUT /pr/156
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_prefix' in 'f' is not valid\"}"},  // PUT /pr/157
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_prefix' in 'f' is not valid\"}"},  // PUT /pr/158
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_prefix' in 'f' is not valid\"}"},  // PUT /pr/159
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_prefix' in 'f' is not valid\"}"},  // PUT /pr/160
	{204, ""},  // PUT /pr/161
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_recurse' must be boolean\"}"},  // PUT /pr/162
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_recurse' must be boolean\"}"},  // PUT /pr/163
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_recurse' must be boolean\"}"},  // PUT /pr/164
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_recurse' must be boolean\"}"},  // PUT /pr/165
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_recurse' must be boolean\"}"},  // PUT /pr/166
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_recurse' must be boolean\"}"},  // PUT /pr/167
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_recurse' must be boolean\"}"},  // PUT /pr/168
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_recurse' must be boolean\"}"},  // PUT /pr/169
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_recurse' must be boolean\"}"},  // PUT /pr/170
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_ignore' must be an array of strings\"}"},  // PUT /pr/171
	{204, ""},  // PUT /pr/172
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_ignore' must be an array of strings\"}"},  // PUT /pr/173
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_ignore' must be an array of strings\"}"},  // PUT /pr/174
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_ignore' must be an array of strings\"}"},  // PUT /pr/175
	{204, ""},  // PUT /pr/176
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_ignore' must be an array of strings\"}"},  // PUT /pr/177
	{204, ""},  // PUT /pr/178
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_ignore' must be an array of strings\"}"},  // PUT /pr/179
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_ignore' must be an array of strings\"}"},  // PUT /pr/180
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: It is not allowed to change '_namespace' [false  ->  true]\"}"},  // PUT /pr/181
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_namespace' must be boolean\"}"},  // PUT /pr/182
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_namespace' must be boolean\"}"},  // PUT /pr/183
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_namespace' must be boolean\"}"},  // PUT /pr/184
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_namespace' must be boolean\"}"},  // PUT /pr/185
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_namespace' must be boolean\"}"},  // PUT /pr/186
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_namespace' must be boolean\"}"},  // PUT /pr/187
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_namespace' must be boolean\"}"},  // PUT /pr/188
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_namespace' must be boolean\"}"},  // PUT /pr/189
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_namespace' must be boolean\"}"},  // PUT /pr/190
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Cannot use '_partial_paths' without '_namespace'.\"}"},  // PUT /pr/191
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_partial_paths' must be boolean\"}"},  // PUT /pr/192
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_partial_paths' must be boolean\"}"},  // PUT /pr/193
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_partial_paths' must be boolean\"}"},  // PUT /pr/194
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_partial_paths' must be boolean\"}"},  // PUT /pr/195
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_partial_paths' must be boolean\"}"},  // PUT /pr/196
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_partial_paths' must be boolean\"}"},  // PUT /pr/197
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_partial_paths' must be boolean\"}"},  // PUT /pr/198
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_partial_paths' must be boolean\"}"},  // PUT /pr/199
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_partial_paths' must be boolean\"}"},  // PUT /pr/200
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type not supported, '_index_uuid_field' must be one of uuid, uuid_field or both\"}"},  // PUT /pr/201
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 's' not supported, '_index_uuid_field' must be one of uuid, uuid_field or both\"}"},  // PUT /pr/202
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type not supported, '_index_uuid_field' must be one of uuid, uuid_field or both\"}"},  // PUT /pr/203
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type not supported, '_index_uuid_field' must be one of uuid, uuid_field or both\"}"},  // PUT /pr/204
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type not supported, '_index_uuid_field' must be one of uuid, uuid_field or both\"}"},  // PUT /pr/205
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type not supported, '_index_uuid_field' must be one of uuid, uuid_field or both\"}"},  // PUT /pr/206
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type not supported, '_index_uuid_field' must be one of uuid, uuid_field or both\"}"},  // PUT /pr/207
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type not supported, '_index_uuid_field' must be one of uuid, uuid_field or both\"}"},  // PUT /pr/208
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type not supported, '_index_uuid_field' must be one of uuid, uuid_field or both\"}"},  // PUT /pr/209
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type not supported, '_index_uuid_field' must be one of uuid, uuid_field or both\"}"},  // PUT /pr/210
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stop_strategy' must be string\"}"},  // PUT /pr/211
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: It is not allowed to change '_stop_strategy' [stop_stemmed  ->  s] in 'f'\"}"},  // PUT /pr/212
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stop_strategy' must be string\"}"},  // PUT /pr/213
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stop_strategy' must be string\"}"},  // PUT /pr/214
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stop_strategy' must be string\"}"},  // PUT /pr/215
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stop_strategy' must be string\"}"},  // PUT /pr/216
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stop_strategy' must be string\"}"},  // PUT /pr/217
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stop_strategy' must be string\"}"},  // PUT /pr/218
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stop_strategy' must be string\"}"},  // PUT /pr/219
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stop_strategy' must be string\"}"},  // PUT /pr/220
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_strategy' must be string\"}"},  // PUT /pr/221
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: It is not allowed to change '_stem_strategy' ['stem_some'  ->  's'] in 'f'\"}"},  // PUT /pr/222
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_strategy' must be string\"}"},  // PUT /pr/223
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_strategy' must be string\"}"},  // PUT /pr/224
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_strategy' must be string\"}"},  // PUT /pr/225
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_strategy' must be string\"}"},  // PUT /pr/226
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_strategy' must be string\"}"},  // PUT /pr/227
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_strategy' must be string\"}"},  // PUT /pr/228
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_strategy' must be string\"}"},  // PUT /pr/229
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_strategy' must be string\"}"},  // PUT /pr/230
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_language' must be string\"}"},  // PUT /pr/231
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: It is not allowed to change '_stem_language' [''  ->  's'] in 'f'\"}"},  // PUT /pr/232
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_language' must be string\"}"},  // PUT /pr/233
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_language' must be string\"}"},  // PUT /pr/234
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_language' must be string\"}"},  // PUT /pr/235
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_language' must be string\"}"},  // PUT /pr/236
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_language' must be string\"}"},  // PUT /pr/237
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_language' must be string\"}"},  // PUT /pr/238
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_language' must be string\"}"},  // PUT /pr/239
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_stem_language' must be string\"}"},  // PUT /pr/240
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_foreign' must be string\"}"},  // PUT /pr/241
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_foreign' must be a valid endpoint\"}"},  // PUT /pr/242
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_foreign' must be string\"}"},  // PUT /pr/243
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_foreign' must be string\"}"},  // PUT /pr/244
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_foreign' must be string\"}"},  // PUT /pr/245
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_foreign' must be string\"}"},  // PUT /pr/246
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_foreign' must be string\"}"},  // PUT /pr/247
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_foreign' must be string\"}"},  // PUT /pr/248
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_foreign' must be string\"}"},  // PUT /pr/249
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_foreign' must be string\"}"},  // PUT /pr/250
	{204, ""},  // PUT /pr/251
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_date_detection' must be boolean\"}"},  // PUT /pr/252
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_date_detection' must be boolean\"}"},  // PUT /pr/253
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_date_detection' must be boolean\"}"},  // PUT /pr/254
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_date_detection' must be boolean\"}"},  // PUT /pr/255
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_date_detection' must be boolean\"}"},  // PUT /pr/256
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_date_detection' must be boolean\"}"},  // PUT /pr/257
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_date_detection' must be boolean\"}"},  // PUT /pr/258
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_date_detection' must be boolean\"}"},  // PUT /pr/259
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_date_detection' must be boolean\"}"},  // PUT /pr/260
	{204, ""},  // PUT /pr/261
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_datetime_detection' must be boolean\"}"},  // PUT /pr/262
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_datetime_detection' must be boolean\"}"},  // PUT /pr/263
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_datetime_detection' must be boolean\"}"},  // PUT /pr/264
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_datetime_detection' must be boolean\"}"},  // PUT /pr/265
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_datetime_detection' must be boolean\"}"},  // PUT /pr/266
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_datetime_detection' must be boolean\"}"},  // PUT /pr/267
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_datetime_detection' must be boolean\"}"},  // PUT /pr/268
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_datetime_detection' must be boolean\"}"},  // PUT /pr/269
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_datetime_detection' must be boolean\"}"},  // PUT /pr/270
	{204, ""},  // PUT /pr/271
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_time_detection' must be boolean\"}"},  // PUT /pr/272
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_time_detection' must be boolean\"}"},  // PUT /pr/273
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_time_detection' must be boolean\"}"},  // PUT /pr/274
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_time_detection' must be boolean\"}"},  // PUT /pr/275
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_time_detection' must be boolean\"}"},  // PUT /pr/276
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_time_detection' must be boolean\"}"},  // PUT /pr/277
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_time_detection' must be boolean\"}"},  // PUT /pr/278
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_time_detection' must be boolean\"}"},  // PUT /pr/279
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_time_detection' must be boolean\"}"},  // PUT /pr/280
	{204, ""},  // PUT /pr/281
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_timedelta_detection' must be boolean\"}"},  // PUT /pr/282
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_timedelta_detection' must be boolean\"}"},  // PUT /pr/283
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_timedelta_detection' must be boolean\"}"},  // PUT /pr/284
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_timedelta_detection' must be boolean\"}"},  // PUT /pr/285
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_timedelta_detection' must be boolean\"}"},  // PUT /pr/286
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_timedelta_detection' must be boolean\"}"},  // PUT /pr/287
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_timedelta_detection' must be boolean\"}"},  // PUT /pr/288
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_timedelta_detection' must be boolean\"}"},  // PUT /pr/289
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_timedelta_detection' must be boolean\"}"},  // PUT /pr/290
	{204, ""},  // PUT /pr/291
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_numeric_detection' must be boolean\"}"},  // PUT /pr/292
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_numeric_detection' must be boolean\"}"},  // PUT /pr/293
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_numeric_detection' must be boolean\"}"},  // PUT /pr/294
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_numeric_detection' must be boolean\"}"},  // PUT /pr/295
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_numeric_detection' must be boolean\"}"},  // PUT /pr/296
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_numeric_detection' must be boolean\"}"},  // PUT /pr/297
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_numeric_detection' must be boolean\"}"},  // PUT /pr/298
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_numeric_detection' must be boolean\"}"},  // PUT /pr/299
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_numeric_detection' must be boolean\"}"},  // PUT /pr/300
	{204, ""},  // PUT /pr/301
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_geo_detection' must be boolean\"}"},  // PUT /pr/302
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_geo_detection' must be boolean\"}"},  // PUT /pr/303
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_geo_detection' must be boolean\"}"},  // PUT /pr/304
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_geo_detection' must be boolean\"}"},  // PUT /pr/305
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_geo_detection' must be boolean\"}"},  // PUT /pr/306
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_geo_detection' must be boolean\"}"},  // PUT /pr/307
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_geo_detection' must be boolean\"}"},  // PUT /pr/308
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_geo_detection' must be boolean\"}"},  // PUT /pr/309
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_geo_detection' must be boolean\"}"},  // PUT /pr/310
	{204, ""},  // PUT /pr/311
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_detection' must be boolean\"}"},  // PUT /pr/312
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_detection' must be boolean\"}"},  // PUT /pr/313
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_detection' must be boolean\"}"},  // PUT /pr/314
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_detection' must be boolean\"}"},  // PUT /pr/315
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_detection' must be boolean\"}"},  // PUT /pr/316
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_detection' must be boolean\"}"},  // PUT /pr/317
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_detection' must be boolean\"}"},  // PUT /pr/318
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_detection' must be boolean\"}"},  // PUT /pr/319
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_detection' must be boolean\"}"},  // PUT /pr/320
	{204, ""},  // PUT /pr/321
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_text_detection' must be boolean\"}"},  // PUT /pr/322
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_text_detection' must be boolean\"}"},  // PUT /pr/323
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_text_detection' must be boolean\"}"},  // PUT /pr/324
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_text_detection' must be boolean\"}"},  // PUT /pr/325
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_text_detection' must be boolean\"}"},  // PUT /pr/326
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_text_detection' must be boolean\"}"},  // PUT /pr/327
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_text_detection' must be boolean\"}"},  // PUT /pr/328
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_text_detection' must be boolean\"}"},  // PUT /pr/329
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_text_detection' must be boolean\"}"},  // PUT /pr/330
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_term_detection' in 'f' is not valid\"}"},  // PUT /pr/331
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_term_detection' in 'f' is not valid\"}"},  // PUT /pr/332
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_term_detection' in 'f' is not valid\"}"},  // PUT /pr/333
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_term_detection' in 'f' is not valid\"}"},  // PUT /pr/334
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_term_detection' in 'f' is not valid\"}"},  // PUT /pr/335
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_term_detection' in 'f' is not valid\"}"},  // PUT /pr/336
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_term_detection' in 'f' is not valid\"}"},  // PUT /pr/337
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_term_detection' in 'f' is not valid\"}"},  // PUT /pr/338
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_term_detection' in 'f' is not valid\"}"},  // PUT /pr/339
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_term_detection' in 'f' is not valid\"}"},  // PUT /pr/340
	{204, ""},  // PUT /pr/341
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_uuid_detection' must be boolean\"}"},  // PUT /pr/342
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_uuid_detection' must be boolean\"}"},  // PUT /pr/343
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_uuid_detection' must be boolean\"}"},  // PUT /pr/344
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_uuid_detection' must be boolean\"}"},  // PUT /pr/345
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_uuid_detection' must be boolean\"}"},  // PUT /pr/346
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_uuid_detection' must be boolean\"}"},  // PUT /pr/347
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_uuid_detection' must be boolean\"}"},  // PUT /pr/348
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_uuid_detection' must be boolean\"}"},  // PUT /pr/349
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_uuid_detection' must be boolean\"}"},  // PUT /pr/350
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: '_bool_term' only is allowed in keyword type fields\"}"},  // PUT /pr/351
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_term' must be a boolean\"}"},  // PUT /pr/352
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_term' must be a boolean\"}"},  // PUT /pr/353
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_term' must be a boolean\"}"},  // PUT /pr/354
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_term' must be a boolean\"}"},  // PUT /pr/355
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_term' must be a boolean\"}"},  // PUT /pr/356
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_term' must be a boolean\"}"},  // PUT /pr/357
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_term' must be a boolean\"}"},  // PUT /pr/358
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_term' must be a boolean\"}"},  // PUT /pr/359
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_bool_term' must be a boolean\"}"},  // PUT /pr/360
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: true\"}"},  // PUT /pr/361
	{204, ""},  // PUT /pr/362
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: 7\"}"},  // PUT /pr/363
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: -1\"}"},  // PUT /pr/364
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: 1.5\"}"},  // PUT /pr/365
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: []\"}"},  // PUT /pr/366
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: [1]\"}"},  // PUT /pr/367
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: [\\\"a\\\"]\"}"},  // PUT /pr/368
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Expected map with one valid cast\"}"},  // PUT /pr/369
	{204, ""},  // PUT /pr/370
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: '_script' must be string or a valid script object\"}"},  // PUT /pr/371
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Script '03c7c0ace395d80182db07ae2c30f034' syntax error: [string \\\"03c7c0ace395d80182db07ae2c30f034\\\"]:1: syntax error near <eof>\"}"},  // PUT /pr/372
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: '_script' must be string or a valid script object\"}"},  // PUT /pr/373
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: '_script' must be string or a valid script object\"}"},  // PUT /pr/374
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: '_script' must be string or a valid script object\"}"},  // PUT /pr/375
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: '_script' must be string or a valid script object\"}"},  // PUT /pr/376
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: '_script' must be string or a valid script object\"}"},  // PUT /pr/377
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: '_script' must be string or a valid script object\"}"},  // PUT /pr/378
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Script must specify '_name' or '_body'\"}"},  // PUT /pr/379
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: '_script' must be string or a valid script object\"}"},  // PUT /pr/380
	{204, ""},  // PUT /pr/381
	{204, ""},  // PUT /pr/382
	{204, ""},  // PUT /pr/383
	{204, ""},  // PUT /pr/384
	{204, ""},  // PUT /pr/385
	{204, ""},  // PUT /pr/386
	{204, ""},  // PUT /pr/387
	{204, ""},  // PUT /pr/388
	{204, ""},  // PUT /pr/389
	{204, ""},  // PUT /pr/390
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_bogus' in 'f' is not valid\"}"},  // PUT /pr/391
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_bogus' in 'f' is not valid\"}"},  // PUT /pr/392
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_bogus' in 'f' is not valid\"}"},  // PUT /pr/393
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_bogus' in 'f' is not valid\"}"},  // PUT /pr/394
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_bogus' in 'f' is not valid\"}"},  // PUT /pr/395
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_bogus' in 'f' is not valid\"}"},  // PUT /pr/396
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_bogus' in 'f' is not valid\"}"},  // PUT /pr/397
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_bogus' in 'f' is not valid\"}"},  // PUT /pr/398
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_bogus' in 'f' is not valid\"}"},  // PUT /pr/399
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_bogus' in 'f' is not valid\"}"},  // PUT /pr/400
	{204, ""},  // PUT /ty1/
	{204, ""},  // PUT /ty1/s1
	{204, ""},  // PUT /ty1/w1
	{204, ""},  // PUT /ty1/s2
	{204, ""},  // PUT /ty1/w2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got 1.5\"}"},  // PUT /ty1/s3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w3' expects a value of type integer, got 1.5\"}"},  // PUT /ty1/w3
	{204, ""},  // PUT /ty1/s4
	{204, ""},  // PUT /ty1/w4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"abc\\\"\"}"},  // PUT /ty1/s5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w5' expects a value of type integer, got \\\"abc\\\"\"}"},  // PUT /ty1/w5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"2020-01-02\\\"\"}"},  // PUT /ty1/s6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w6' expects a value of type integer, got \\\"2020-01-02\\\"\"}"},  // PUT /ty1/w6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"10:30:00\\\"\"}"},  // PUT /ty1/s7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w7' expects a value of type integer, got \\\"10:30:00\\\"\"}"},  // PUT /ty1/w7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"+01:00:00\\\"\"}"},  // PUT /ty1/s8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w8' expects a value of type integer, got \\\"+01:00:00\\\"\"}"},  // PUT /ty1/w8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"550e8400-e29b-41d4-a716-446655440000\\\"\"}"},  // PUT /ty1/s9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w9' expects a value of type integer, got \\\"550e8400-e29b-41d4-a716-446655440000\\\"\"}"},  // PUT /ty1/w9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"true\\\"\"}"},  // PUT /ty1/s10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w10' expects a value of type integer, got \\\"true\\\"\"}"},  // PUT /ty1/w10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got true\"}"},  // PUT /ty1/s11
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w11' expects a value of type integer, got true\"}"},  // PUT /ty1/w11
	{204, ""},  // PUT /ty1/s12
	{204, ""},  // PUT /ty1/w12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got [1,2]\"}"},  // PUT /ty1/s13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w13' expects a value of type integer, got [1,2]\"}"},  // PUT /ty1/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty1/s14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w14' expects a value of type integer, got [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty1/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty1/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w15' expects a value of type integer, got {\\\"_latitude\\\":1,\\\"_longitude\\\":2}: Unknown cast type _latitude\"}"},  // PUT /ty1/w15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"POINT(1 2)\\\"\"}"},  // PUT /ty1/s16
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w16' expects a value of type integer, got \\\"POINT(1 2)\\\"\"}"},  // PUT /ty1/w16
	{204, ""},  // PUT /ty1/s17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w17' expects a value of type integer, got {}: Expected map with one valid cast\"}"},  // PUT /ty1/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got []\"}"},  // PUT /ty1/s18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w18' expects a value of type integer, got []\"}"},  // PUT /ty1/w18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got 18446744073709551615\"}"},  // PUT /ty1/s19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w19' expects a value of type integer, got 18446744073709551615\"}"},  // PUT /ty1/w19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"Abc Def\\\"\"}"},  // PUT /ty1/s20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w20' expects a value of type integer, got \\\"Abc Def\\\"\"}"},  // PUT /ty1/w20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"2020-01-01T00:00:00Z\\\"\"}"},  // PUT /ty1/s21
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w21' expects a value of type integer, got \\\"2020-01-01T00:00:00Z\\\"\"}"},  // PUT /ty1/w21
	{204, ""},  // PUT /ty2/
	{204, ""},  // PUT /ty2/s1
	{204, ""},  // PUT /ty2/w1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type positive, got -1\"}"},  // PUT /ty2/s2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w2' expects a value of type positive, got -1\"}"},  // PUT /ty2/w2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type positive, got 1.5\"}"},  // PUT /ty2/s3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w3' expects a value of type positive, got 1.5\"}"},  // PUT /ty2/w3
	{204, ""},  // PUT /ty2/s4
	{204, ""},  // PUT /ty2/w4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type positive, got \\\"abc\\\"\"}"},  // PUT /ty2/s5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w5' expects a value of type positive, got \\\"abc\\\"\"}"},  // PUT /ty2/w5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type positive, got \\\"2020-01-02\\\"\"}"},  // PUT /ty2/s6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w6' expects a value of type positive, got \\\"2020-01-02\\\"\"}"},  // PUT /ty2/w6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type positive, got \\\"10:30:00\\\"\"}"},  // PUT /ty2/s7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w7' expects a value of type positive, got \\\"10:30:00\\\"\"}"},  // PUT /ty2/w7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type positive, got \\\"+01:00:00\\\"\"}"},  // PUT /ty2/s8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w8' expects a value of type positive, got \\\"+01:00:00\\\"\"}"},  // PUT /ty2/w8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type positive, got \\\"550e8400-e29b-41d4-a716-446655440000\\\"\"}"},  // PUT /ty2/s9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w9' expects a value of type positive, got \\\"550e8400-e29b-41d4-a716-446655440000\\\"\"}"},  // PUT /ty2/w9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type positive, got \\\"true\\\"\"}"},  // PUT /ty2/s10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w10' expects a value of type positive, got \\\"true\\\"\"}"},  // PUT /ty2/w10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type positive, got true\"}"},  // PUT /ty2/s11
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w11' expects a value of type positive, got true\"}"},  // PUT /ty2/w11
	{204, ""},  // PUT /ty2/s12
	{204, ""},  // PUT /ty2/w12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type positive, got [1,2]\"}"},  // PUT /ty2/s13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w13' expects a value of type positive, got [1,2]\"}"},  // PUT /ty2/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type positive, got [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty2/s14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w14' expects a value of type positive, got [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty2/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty2/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w15' expects a value of type positive, got {\\\"_latitude\\\":1,\\\"_longitude\\\":2}: Unknown cast type _latitude\"}"},  // PUT /ty2/w15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type positive, got \\\"POINT(1 2)\\\"\"}"},  // PUT /ty2/s16
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w16' expects a value of type positive, got \\\"POINT(1 2)\\\"\"}"},  // PUT /ty2/w16
	{204, ""},  // PUT /ty2/s17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w17' expects a value of type positive, got {}: Expected map with one valid cast\"}"},  // PUT /ty2/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type positive, got []\"}"},  // PUT /ty2/s18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w18' expects a value of type positive, got []\"}"},  // PUT /ty2/w18
	{204, ""},  // PUT /ty2/s19
	{204, ""},  // PUT /ty2/w19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type positive, got \\\"Abc Def\\\"\"}"},  // PUT /ty2/s20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w20' expects a value of type positive, got \\\"Abc Def\\\"\"}"},  // PUT /ty2/w20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type positive, got \\\"2020-01-01T00:00:00Z\\\"\"}"},  // PUT /ty2/s21
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w21' expects a value of type positive, got \\\"2020-01-01T00:00:00Z\\\"\"}"},  // PUT /ty2/w21
	{204, ""},  // PUT /ty3/
	{204, ""},  // PUT /ty3/s1
	{204, ""},  // PUT /ty3/w1
	{204, ""},  // PUT /ty3/s2
	{204, ""},  // PUT /ty3/w2
	{204, ""},  // PUT /ty3/s3
	{204, ""},  // PUT /ty3/w3
	{204, ""},  // PUT /ty3/s4
	{204, ""},  // PUT /ty3/w4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type floating, got \\\"abc\\\"\"}"},  // PUT /ty3/s5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w5' expects a value of type floating, got \\\"abc\\\"\"}"},  // PUT /ty3/w5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type floating, got \\\"2020-01-02\\\"\"}"},  // PUT /ty3/s6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w6' expects a value of type floating, got \\\"2020-01-02\\\"\"}"},  // PUT /ty3/w6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type floating, got \\\"10:30:00\\\"\"}"},  // PUT /ty3/s7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w7' expects a value of type floating, got \\\"10:30:00\\\"\"}"},  // PUT /ty3/w7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type floating, got \\\"+01:00:00\\\"\"}"},  // PUT /ty3/s8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w8' expects a value of type floating, got \\\"+01:00:00\\\"\"}"},  // PUT /ty3/w8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type floating, got \\\"550e8400-e29b-41d4-a716-446655440000\\\"\"}"},  // PUT /ty3/s9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w9' expects a value of type floating, got \\\"550e8400-e29b-41d4-a716-446655440000\\\"\"}"},  // PUT /ty3/w9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type floating, got \\\"true\\\"\"}"},  // PUT /ty3/s10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w10' expects a value of type floating, got \\\"true\\\"\"}"},  // PUT /ty3/w10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type floating, got true\"}"},  // PUT /ty3/s11
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w11' expects a value of type floating, got true\"}"},  // PUT /ty3/w11
	{204, ""},  // PUT /ty3/s12
	{204, ""},  // PUT /ty3/w12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type floating, got [1,2]\"}"},  // PUT /ty3/s13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w13' expects a value of type floating, got [1,2]\"}"},  // PUT /ty3/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type floating, got [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty3/s14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w14' expects a value of type floating, got [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty3/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty3/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w15' expects a value of type floating, got {\\\"_latitude\\\":1,\\\"_longitude\\\":2}: Unknown cast type _latitude\"}"},  // PUT /ty3/w15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type floating, got \\\"POINT(1 2)\\\"\"}"},  // PUT /ty3/s16
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w16' expects a value of type floating, got \\\"POINT(1 2)\\\"\"}"},  // PUT /ty3/w16
	{204, ""},  // PUT /ty3/s17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w17' expects a value of type floating, got {}: Expected map with one valid cast\"}"},  // PUT /ty3/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type floating, got []\"}"},  // PUT /ty3/s18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w18' expects a value of type floating, got []\"}"},  // PUT /ty3/w18
	{204, ""},  // PUT /ty3/s19
	{204, ""},  // PUT /ty3/w19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type floating, got \\\"Abc Def\\\"\"}"},  // PUT /ty3/s20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w20' expects a value of type floating, got \\\"Abc Def\\\"\"}"},  // PUT /ty3/w20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type floating, got \\\"2020-01-01T00:00:00Z\\\"\"}"},  // PUT /ty3/s21
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'w21' expects a value of type floating, got \\\"2020-01-01T00:00:00Z\\\"\"}"},  // PUT /ty3/w21
	{204, ""},  // PUT /ty4/
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: boolean is not a integer [5]\"}"},  // PUT /ty4/s1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: boolean is not a integer [5]\"}"},  // PUT /ty4/w1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: boolean is not a integer [-1]\"}"},  // PUT /ty4/s2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: boolean is not a integer [-1]\"}"},  // PUT /ty4/w2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: boolean is not a float\"}"},  // PUT /ty4/s3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: boolean is not a float\"}"},  // PUT /ty4/w3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: '7'\"}"},  // PUT /ty4/s4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: '7'\"}"},  // PUT /ty4/w4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: 'abc'\"}"},  // PUT /ty4/s5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: 'abc'\"}"},  // PUT /ty4/w5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: '2020-01-02'\"}"},  // PUT /ty4/s6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: '2020-01-02'\"}"},  // PUT /ty4/w6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: '10:30:00'\"}"},  // PUT /ty4/s7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: '10:30:00'\"}"},  // PUT /ty4/w7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: '+01:00:00'\"}"},  // PUT /ty4/s8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: '+01:00:00'\"}"},  // PUT /ty4/w8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: '550e8400-e29b-41d4-a716-446655440000'\"}"},  // PUT /ty4/s9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: '550e8400-e29b-41d4-a716-446655440000'\"}"},  // PUT /ty4/w9
	{204, ""},  // PUT /ty4/s10
	{204, ""},  // PUT /ty4/w10
	{204, ""},  // PUT /ty4/s11
	{204, ""},  // PUT /ty4/w11
	{204, ""},  // PUT /ty4/s12
	{204, ""},  // PUT /ty4/w12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: msgpack::type ARRAY is not supported\"}"},  // PUT /ty4/s13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: msgpack::type ARRAY is not supported\"}"},  // PUT /ty4/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: msgpack::type ARRAY is not supported\"}"},  // PUT /ty4/s14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: msgpack::type ARRAY is not supported\"}"},  // PUT /ty4/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty4/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Unknown cast type _latitude\"}"},  // PUT /ty4/w15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: 'POINT(1 2)'\"}"},  // PUT /ty4/s16
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: 'POINT(1 2)'\"}"},  // PUT /ty4/w16
	{204, ""},  // PUT /ty4/s17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Expected map with one valid cast\"}"},  // PUT /ty4/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: msgpack::type ARRAY is not supported\"}"},  // PUT /ty4/s18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: msgpack::type ARRAY is not supported\"}"},  // PUT /ty4/w18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: boolean is not a positive integer [18446744073709551615]\"}"},  // PUT /ty4/s19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: boolean is not a positive integer [18446744073709551615]\"}"},  // PUT /ty4/w19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: 'Abc Def'\"}"},  // PUT /ty4/s20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: 'Abc Def'\"}"},  // PUT /ty4/w20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: '2020-01-01T00:00:00Z'\"}"},  // PUT /ty4/s21
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Boolean format is not valid: '2020-01-01T00:00:00Z'\"}"},  // PUT /ty4/w21
	{204, ""},  // PUT /ty5/
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: 5\"}"},  // PUT /ty5/s1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: 5\"}"},  // PUT /ty5/w1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: -1\"}"},  // PUT /ty5/s2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: -1\"}"},  // PUT /ty5/w2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: 1.5\"}"},  // PUT /ty5/s3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: 1.5\"}"},  // PUT /ty5/w3
	{204, ""},  // PUT /ty5/s4
	{204, ""},  // PUT /ty5/w4
	{204, ""},  // PUT /ty5/s5
	{204, ""},  // PUT /ty5/w5
	{204, ""},  // PUT /ty5/s6
	{204, ""},  // PUT /ty5/w6
	{204, ""},  // PUT /ty5/s7
	{204, ""},  // PUT /ty5/w7
	{204, ""},  // PUT /ty5/s8
	{204, ""},  // PUT /ty5/w8
	{204, ""},  // PUT /ty5/s9
	{204, ""},  // PUT /ty5/w9
	{204, ""},  // PUT /ty5/s10
	{204, ""},  // PUT /ty5/w10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: true\"}"},  // PUT /ty5/s11
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: true\"}"},  // PUT /ty5/w11
	{204, ""},  // PUT /ty5/s12
	{204, ""},  // PUT /ty5/w12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: [1,2]\"}"},  // PUT /ty5/s13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: [1,2]\"}"},  // PUT /ty5/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty5/s14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty5/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty5/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Unknown cast type _latitude\"}"},  // PUT /ty5/w15
	{204, ""},  // PUT /ty5/s16
	{204, ""},  // PUT /ty5/w16
	{204, ""},  // PUT /ty5/s17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Expected map with one valid cast\"}"},  // PUT /ty5/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: []\"}"},  // PUT /ty5/s18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: []\"}"},  // PUT /ty5/w18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: 18446744073709551615\"}"},  // PUT /ty5/s19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: 18446744073709551615\"}"},  // PUT /ty5/w19
	{204, ""},  // PUT /ty5/s20
	{204, ""},  // PUT /ty5/w20
	{204, ""},  // PUT /ty5/s21
	{204, ""},  // PUT /ty5/w21
	{204, ""},  // PUT /ty6/
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: 5\"}"},  // PUT /ty6/s1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: 5\"}"},  // PUT /ty6/w1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: -1\"}"},  // PUT /ty6/s2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: -1\"}"},  // PUT /ty6/w2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: 1.5\"}"},  // PUT /ty6/s3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: 1.5\"}"},  // PUT /ty6/w3
	{204, ""},  // PUT /ty6/s4
	{204, ""},  // PUT /ty6/w4
	{204, ""},  // PUT /ty6/s5
	{204, ""},  // PUT /ty6/w5
	{204, ""},  // PUT /ty6/s6
	{204, ""},  // PUT /ty6/w6
	{204, ""},  // PUT /ty6/s7
	{204, ""},  // PUT /ty6/w7
	{204, ""},  // PUT /ty6/s8
	{204, ""},  // PUT /ty6/w8
	{204, ""},  // PUT /ty6/s9
	{204, ""},  // PUT /ty6/w9
	{204, ""},  // PUT /ty6/s10
	{204, ""},  // PUT /ty6/w10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: true\"}"},  // PUT /ty6/s11
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: true\"}"},  // PUT /ty6/w11
	{204, ""},  // PUT /ty6/s12
	{204, ""},  // PUT /ty6/w12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: [1,2]\"}"},  // PUT /ty6/s13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: [1,2]\"}"},  // PUT /ty6/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty6/s14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty6/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty6/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Unknown cast type _latitude\"}"},  // PUT /ty6/w15
	{204, ""},  // PUT /ty6/s16
	{204, ""},  // PUT /ty6/w16
	{204, ""},  // PUT /ty6/s17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Expected map with one valid cast\"}"},  // PUT /ty6/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: []\"}"},  // PUT /ty6/s18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: []\"}"},  // PUT /ty6/w18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: 18446744073709551615\"}"},  // PUT /ty6/s19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: 18446744073709551615\"}"},  // PUT /ty6/w19
	{204, ""},  // PUT /ty6/s20
	{204, ""},  // PUT /ty6/w20
	{204, ""},  // PUT /ty6/s21
	{204, ""},  // PUT /ty6/w21
	{204, ""},  // PUT /ty7/
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: 5\"}"},  // PUT /ty7/s1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: 5\"}"},  // PUT /ty7/w1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: -1\"}"},  // PUT /ty7/s2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: -1\"}"},  // PUT /ty7/w2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: 1.5\"}"},  // PUT /ty7/s3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: 1.5\"}"},  // PUT /ty7/w3
	{204, ""},  // PUT /ty7/s4
	{204, ""},  // PUT /ty7/w4
	{204, ""},  // PUT /ty7/s5
	{204, ""},  // PUT /ty7/w5
	{204, ""},  // PUT /ty7/s6
	{204, ""},  // PUT /ty7/w6
	{204, ""},  // PUT /ty7/s7
	{204, ""},  // PUT /ty7/w7
	{204, ""},  // PUT /ty7/s8
	{204, ""},  // PUT /ty7/w8
	{204, ""},  // PUT /ty7/s9
	{204, ""},  // PUT /ty7/w9
	{204, ""},  // PUT /ty7/s10
	{204, ""},  // PUT /ty7/w10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: true\"}"},  // PUT /ty7/s11
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: true\"}"},  // PUT /ty7/w11
	{204, ""},  // PUT /ty7/s12
	{204, ""},  // PUT /ty7/w12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: [1,2]\"}"},  // PUT /ty7/s13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: [1,2]\"}"},  // PUT /ty7/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty7/s14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty7/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty7/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Unknown cast type _latitude\"}"},  // PUT /ty7/w15
	{204, ""},  // PUT /ty7/s16
	{204, ""},  // PUT /ty7/w16
	{204, ""},  // PUT /ty7/s17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Expected map with one valid cast\"}"},  // PUT /ty7/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: []\"}"},  // PUT /ty7/s18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: []\"}"},  // PUT /ty7/w18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: 18446744073709551615\"}"},  // PUT /ty7/s19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for text type: 18446744073709551615\"}"},  // PUT /ty7/w19
	{204, ""},  // PUT /ty7/s20
	{204, ""},  // PUT /ty7/w20
	{204, ""},  // PUT /ty7/s21
	{204, ""},  // PUT /ty7/w21
	{204, ""},  // PUT /ty8/
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: 5\"}"},  // PUT /ty8/s1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: 5\"}"},  // PUT /ty8/w1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: -1\"}"},  // PUT /ty8/s2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: -1\"}"},  // PUT /ty8/w2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: 1.5\"}"},  // PUT /ty8/s3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: 1.5\"}"},  // PUT /ty8/w3
	{204, ""},  // PUT /ty8/s4
	{204, ""},  // PUT /ty8/w4
	{204, ""},  // PUT /ty8/s5
	{204, ""},  // PUT /ty8/w5
	{204, ""},  // PUT /ty8/s6
	{204, ""},  // PUT /ty8/w6
	{204, ""},  // PUT /ty8/s7
	{204, ""},  // PUT /ty8/w7
	{204, ""},  // PUT /ty8/s8
	{204, ""},  // PUT /ty8/w8
	{204, ""},  // PUT /ty8/s9
	{204, ""},  // PUT /ty8/w9
	{204, ""},  // PUT /ty8/s10
	{204, ""},  // PUT /ty8/w10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: true\"}"},  // PUT /ty8/s11
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: true\"}"},  // PUT /ty8/w11
	{204, ""},  // PUT /ty8/s12
	{204, ""},  // PUT /ty8/w12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: [1,2]\"}"},  // PUT /ty8/s13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: [1,2]\"}"},  // PUT /ty8/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty8/s14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty8/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty8/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Unknown cast type _latitude\"}"},  // PUT /ty8/w15
	{204, ""},  // PUT /ty8/s16
	{204, ""},  // PUT /ty8/w16
	{204, ""},  // PUT /ty8/s17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Expected map with one valid cast\"}"},  // PUT /ty8/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: []\"}"},  // PUT /ty8/s18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: []\"}"},  // PUT /ty8/w18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: 18446744073709551615\"}"},  // PUT /ty8/s19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for keyword type: 18446744073709551615\"}"},  // PUT /ty8/w19
	{204, ""},  // PUT /ty8/s20
	{204, ""},  // PUT /ty8/w20
	{204, ""},  // PUT /ty8/s21
	{204, ""},  // PUT /ty8/w21
	{204, ""},  // PUT /ty9/
	{204, ""},  // PUT /ty9/s1
	{204, ""},  // PUT /ty9/w1
	{204, ""},  // PUT /ty9/s2
	{204, ""},  // PUT /ty9/w2
	{204, ""},  // PUT /ty9/s3
	{204, ""},  // PUT /ty9/w3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: 7\"}"},  // PUT /ty9/s4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: 7\"}"},  // PUT /ty9/w4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: abc\"}"},  // PUT /ty9/s5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: abc\"}"},  // PUT /ty9/w5
	{204, ""},  // PUT /ty9/s6
	{204, ""},  // PUT /ty9/w6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: 10:30:00\"}"},  // PUT /ty9/s7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: 10:30:00\"}"},  // PUT /ty9/w7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: +01:00:00\"}"},  // PUT /ty9/s8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: +01:00:00\"}"},  // PUT /ty9/w8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: 550e8400-e29b-41d4-a716-446655440000\"}"},  // PUT /ty9/s9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: 550e8400-e29b-41d4-a716-446655440000\"}"},  // PUT /ty9/w9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: true\"}"},  // PUT /ty9/s10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: true\"}"},  // PUT /ty9/w10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Datetime value must be numeric or string\"}"},  // PUT /ty9/s11
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Datetime value must be numeric or string\"}"},  // PUT /ty9/w11
	{204, ""},  // PUT /ty9/s12
	{204, ""},  // PUT /ty9/w12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Datetime value must be numeric or string\"}"},  // PUT /ty9/s13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Datetime value must be numeric or string\"}"},  // PUT /ty9/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Datetime value must be numeric or string\"}"},  // PUT /ty9/s14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Datetime value must be numeric or string\"}"},  // PUT /ty9/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty9/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Unknown cast type _latitude\"}"},  // PUT /ty9/w15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: POINT(1 2)\"}"},  // PUT /ty9/s16
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: POINT(1 2)\"}"},  // PUT /ty9/w16
	{204, ""},  // PUT /ty9/s17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Expected map with one valid cast\"}"},  // PUT /ty9/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Datetime value must be numeric or string\"}"},  // PUT /ty9/s18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Datetime value must be numeric or string\"}"},  // PUT /ty9/w18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Year is out of range\"}"},  // PUT /ty9/s19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Year is out of range\"}"},  // PUT /ty9/w19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: Abc Def\"}"},  // PUT /ty9/s20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: Abc Def\"}"},  // PUT /ty9/w20
	{204, ""},  // PUT /ty9/s21
	{204, ""},  // PUT /ty9/w21
	{204, ""},  // PUT /ty10/
	{204, ""},  // PUT /ty10/s1
	{204, ""},  // PUT /ty10/w1
	{204, ""},  // PUT /ty10/s2
	{204, ""},  // PUT /ty10/w2
	{204, ""},  // PUT /ty10/s3
	{204, ""},  // PUT /ty10/w3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: 7\"}"},  // PUT /ty10/s4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: 7\"}"},  // PUT /ty10/w4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: abc\"}"},  // PUT /ty10/s5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: abc\"}"},  // PUT /ty10/w5
	{204, ""},  // PUT /ty10/s6
	{204, ""},  // PUT /ty10/w6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: 10:30:00\"}"},  // PUT /ty10/s7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: 10:30:00\"}"},  // PUT /ty10/w7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: +01:00:00\"}"},  // PUT /ty10/s8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: +01:00:00\"}"},  // PUT /ty10/w8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: 550e8400-e29b-41d4-a716-446655440000\"}"},  // PUT /ty10/s9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: 550e8400-e29b-41d4-a716-446655440000\"}"},  // PUT /ty10/w9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: true\"}"},  // PUT /ty10/s10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: true\"}"},  // PUT /ty10/w10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Datetime value must be numeric or string\"}"},  // PUT /ty10/s11
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Datetime value must be numeric or string\"}"},  // PUT /ty10/w11
	{204, ""},  // PUT /ty10/s12
	{204, ""},  // PUT /ty10/w12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Datetime value must be numeric or string\"}"},  // PUT /ty10/s13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Datetime value must be numeric or string\"}"},  // PUT /ty10/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Datetime value must be numeric or string\"}"},  // PUT /ty10/s14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Datetime value must be numeric or string\"}"},  // PUT /ty10/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty10/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Unknown cast type _latitude\"}"},  // PUT /ty10/w15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: POINT(1 2)\"}"},  // PUT /ty10/s16
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: POINT(1 2)\"}"},  // PUT /ty10/w16
	{204, ""},  // PUT /ty10/s17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Expected map with one valid cast\"}"},  // PUT /ty10/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Datetime value must be numeric or string\"}"},  // PUT /ty10/s18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Datetime value must be numeric or string\"}"},  // PUT /ty10/w18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Year is out of range\"}"},  // PUT /ty10/s19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Year is out of range\"}"},  // PUT /ty10/w19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: Abc Def\"}"},  // PUT /ty10/s20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in _datetime: Abc Def\"}"},  // PUT /ty10/w20
	{204, ""},  // PUT /ty10/s21
	{204, ""},  // PUT /ty10/w21
	{204, ""},  // PUT /ty11/
	{204, ""},  // PUT /ty11/s1
	{204, ""},  // PUT /ty11/w1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Time: -1 is out of range\"}"},  // PUT /ty11/s2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Time: -1 is out of range\"}"},  // PUT /ty11/w2
	{204, ""},  // PUT /ty11/s3
	{204, ""},  // PUT /ty11/w3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: 7, the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/s4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: 7, the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/w4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: abc, the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/s5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: abc, the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/w5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: 2020-01-02, the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/s6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: 2020-01-02, the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/w6
	{204, ""},  // PUT /ty11/s7
	{204, ""},  // PUT /ty11/w7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: +01:00:00, the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/s8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: +01:00:00, the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/w8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: 550e8400-e29b-41d4-a716-446655440000, the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/s9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: 550e8400-e29b-41d4-a716-446655440000, the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/w9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: true, the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/s10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: true, the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/w10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: BOOLEAN is not time\"}"},  // PUT /ty11/s11
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: BOOLEAN is not time\"}"},  // PUT /ty11/w11
	{204, ""},  // PUT /ty11/s12
	{204, ""},  // PUT /ty11/w12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: ARRAY is not time\"}"},  // PUT /ty11/s13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: ARRAY is not time\"}"},  // PUT /ty11/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: ARRAY is not time\"}"},  // PUT /ty11/s14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: ARRAY is not time\"}"},  // PUT /ty11/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty11/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Unknown cast type _latitude\"}"},  // PUT /ty11/w15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: POINT(1 2), the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/s16
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: POINT(1 2), the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/w16
	{204, ""},  // PUT /ty11/s17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Expected map with one valid cast\"}"},  // PUT /ty11/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: ARRAY is not time\"}"},  // PUT /ty11/s18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: ARRAY is not time\"}"},  // PUT /ty11/w18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Time: 18446744073709551616 is out of range\"}"},  // PUT /ty11/s19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Time: 18446744073709551616 is out of range\"}"},  // PUT /ty11/w19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: Abc Def, the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/s20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: Abc Def, the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/w20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: 2020-01-01T00:00:00Z, the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/s21
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in time: 2020-01-01T00:00:00Z, the format must be '00:00(:00(.0...)([+-]00:00))'\"}"},  // PUT /ty11/w21
	{204, ""},  // PUT /ty12/
	{204, ""},  // PUT /ty12/s1
	{204, ""},  // PUT /ty12/w1
	{204, ""},  // PUT /ty12/s2
	{204, ""},  // PUT /ty12/w2
	{204, ""},  // PUT /ty12/s3
	{204, ""},  // PUT /ty12/w3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: 7, the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/s4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: 7, the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/w4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: abc, the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/s5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: abc, the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/w5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: 2020-01-02, the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/s6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: 2020-01-02, the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/w6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: 10:30:00, the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/s7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: 10:30:00, the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/w7
	{204, ""},  // PUT /ty12/s8
	{204, ""},  // PUT /ty12/w8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: 550e8400-e29b-41d4-a716-446655440000, the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/s9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: 550e8400-e29b-41d4-a716-446655440000, the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/w9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: true, the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/s10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: true, the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/w10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: BOOLEAN is not timedelta\"}"},  // PUT /ty12/s11
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: BOOLEAN is not timedelta\"}"},  // PUT /ty12/w11
	{204, ""},  // PUT /ty12/s12
	{204, ""},  // PUT /ty12/w12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: ARRAY is not timedelta\"}"},  // PUT /ty12/s13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: ARRAY is not timedelta\"}"},  // PUT /ty12/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: ARRAY is not timedelta\"}"},  // PUT /ty12/s14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: ARRAY is not timedelta\"}"},  // PUT /ty12/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty12/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Unknown cast type _latitude\"}"},  // PUT /ty12/w15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: POINT(1 2), the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/s16
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: POINT(1 2), the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/w16
	{204, ""},  // PUT /ty12/s17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Expected map with one valid cast\"}"},  // PUT /ty12/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: ARRAY is not timedelta\"}"},  // PUT /ty12/s18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: ARRAY is not timedelta\"}"},  // PUT /ty12/w18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Timedelta: 18446744073709551616 is out of range\"}"},  // PUT /ty12/s19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Timedelta: 18446744073709551616 is out of range\"}"},  // PUT /ty12/w19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: Abc Def, the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/s20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: Abc Def, the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/w20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: 2020-01-01T00:00:00Z, the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/s21
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Error format in timedelta: 2020-01-01T00:00:00Z, the format must be '[+-]00:00(:00(.0...))'\"}"},  // PUT /ty12/w21
	{204, ""},  // PUT /ty13/
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for uuid type: 5\"}"},  // PUT /ty13/s1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for uuid type: 5\"}"},  // PUT /ty13/w1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for uuid type: -1\"}"},  // PUT /ty13/s2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for uuid type: -1\"}"},  // PUT /ty13/w2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for uuid type: 1.5\"}"},  // PUT /ty13/s3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for uuid type: 1.5\"}"},  // PUT /ty13/w3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid UUID format: '7'\"}"},  // PUT /ty13/s4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid UUID format: '7'\"}"},  // PUT /ty13/w4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid encoded UUID format: 'abc'\"}"},  // PUT /ty13/s5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid encoded UUID format: 'abc'\"}"},  // PUT /ty13/w5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid encoded UUID format: '2020-01-02'\"}"},  // PUT /ty13/s6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid encoded UUID format: '2020-01-02'\"}"},  // PUT /ty13/w6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid encoded UUID format: '10:30:00'\"}"},  // PUT /ty13/s7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid encoded UUID format: '10:30:00'\"}"},  // PUT /ty13/w7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid encoded UUID format: '+01:00:00'\"}"},  // PUT /ty13/s8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid encoded UUID format: '+01:00:00'\"}"},  // PUT /ty13/w8
	{204, ""},  // PUT /ty13/s9
	{204, ""},  // PUT /ty13/w9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid encoded UUID format: 'true'\"}"},  // PUT /ty13/s10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid encoded UUID format: 'true'\"}"},  // PUT /ty13/w10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for uuid type: true\"}"},  // PUT /ty13/s11
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for uuid type: true\"}"},  // PUT /ty13/w11
	{204, ""},  // PUT /ty13/s12
	{204, ""},  // PUT /ty13/w12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for uuid type: [1,2]\"}"},  // PUT /ty13/s13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for uuid type: [1,2]\"}"},  // PUT /ty13/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for uuid type: [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty13/s14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for uuid type: [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty13/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty13/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Unknown cast type _latitude\"}"},  // PUT /ty13/w15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid encoded UUID format: 'POINT(1 2)'\"}"},  // PUT /ty13/s16
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid encoded UUID format: 'POINT(1 2)'\"}"},  // PUT /ty13/w16
	{204, ""},  // PUT /ty13/s17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Expected map with one valid cast\"}"},  // PUT /ty13/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for uuid type: []\"}"},  // PUT /ty13/s18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for uuid type: []\"}"},  // PUT /ty13/w18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for uuid type: 18446744073709551615\"}"},  // PUT /ty13/s19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Format invalid for uuid type: 18446744073709551615\"}"},  // PUT /ty13/w19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid encoded UUID format: 'Abc Def'\"}"},  // PUT /ty13/s20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid encoded UUID format: 'Abc Def'\"}"},  // PUT /ty13/w20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid encoded UUID format: '2020-01-01T00:00:00Z'\"}"},  // PUT /ty13/s21
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid encoded UUID format: '2020-01-01T00:00:00Z'\"}"},  // PUT /ty13/w21
	{204, ""},  // PUT /ty14/
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Object must be string, map or array not NEGATIVE_INTEGER\"}"},  // PUT /ty14/s1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Object must be string, map or array not NEGATIVE_INTEGER\"}"},  // PUT /ty14/w1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Object must be string, map or array not NEGATIVE_INTEGER\"}"},  // PUT /ty14/s2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Object must be string, map or array not NEGATIVE_INTEGER\"}"},  // PUT /ty14/w2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Object must be string, map or array not FLOAT\"}"},  // PUT /ty14/s3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Object must be string, map or array not FLOAT\"}"},  // PUT /ty14/w3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Syntax error in ''7''\"}"},  // PUT /ty14/s4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Syntax error in ''7''\"}"},  // PUT /ty14/w4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Syntax error in ''abc''\"}"},  // PUT /ty14/s5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Syntax error in ''abc''\"}"},  // PUT /ty14/w5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Syntax error in ''2020-01-02''\"}"},  // PUT /ty14/s6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Syntax error in ''2020-01-02''\"}"},  // PUT /ty14/w6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Syntax error in ''10:30:00''\"}"},  // PUT /ty14/s7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Syntax error in ''10:30:00''\"}"},  // PUT /ty14/w7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Syntax error in ''+01:00:00''\"}"},  // PUT /ty14/s8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Syntax error in ''+01:00:00''\"}"},  // PUT /ty14/w8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Syntax error in ''550e8400-e29b-41d4-a716-446655440000''\"}"},  // PUT /ty14/s9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Syntax error in ''550e8400-e29b-41d4-a716-446655440000''\"}"},  // PUT /ty14/w9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Syntax error in ''true''\"}"},  // PUT /ty14/s10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Syntax error in ''true''\"}"},  // PUT /ty14/w10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Object must be string, map or array not BOOLEAN\"}"},  // PUT /ty14/s11
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Object must be string, map or array not BOOLEAN\"}"},  // PUT /ty14/w11
	{204, ""},  // PUT /ty14/s12
	{204, ""},  // PUT /ty14/w12
	{204, ""},  // PUT /ty14/s13
	{204, ""},  // PUT /ty14/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Expected array of [longitude, latitude] or [longitude, latitude, altitude]\"}"},  // PUT /ty14/s14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Expected array of [longitude, latitude] or [longitude, latitude, altitude]\"}"},  // PUT /ty14/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty14/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Unknown cast type _latitude\"}"},  // PUT /ty14/w15
	{204, ""},  // PUT /ty14/s16
	{204, ""},  // PUT /ty14/w16
	{204, ""},  // PUT /ty14/s17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Expected map with one valid cast\"}"},  // PUT /ty14/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: _latitude, _longitude and _altitude must be numeric\"}"},  // PUT /ty14/s18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: _latitude, _longitude and _altitude must be numeric\"}"},  // PUT /ty14/w18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Object must be string, map or array not POSITIVE_INTEGER\"}"},  // PUT /ty14/s19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Object must be string, map or array not POSITIVE_INTEGER\"}"},  // PUT /ty14/w19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Geometry 'Abc' is not supported\"}"},  // PUT /ty14/s20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Geometry 'Abc' is not supported\"}"},  // PUT /ty14/w20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Syntax error in ''2020-01-01T00:00:00Z''\"}"},  // PUT /ty14/s21
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Syntax error in ''2020-01-01T00:00:00Z''\"}"},  // PUT /ty14/w21
	{204, ""},  // PUT /ty15/
	{204, ""},  // PUT /ty16/
	{204, ""},  // PUT /ty16/s1
	{204, ""},  // PUT /ty16/w1
	{204, ""},  // PUT /ty16/s2
	{204, ""},  // PUT /ty16/w2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got 1.5\"}"},  // PUT /ty16/s3
	{204, ""},  // PUT /ty16/w3
	{204, ""},  // PUT /ty16/s4
	{204, ""},  // PUT /ty16/w4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"abc\\\"\"}"},  // PUT /ty16/s5
	{204, ""},  // PUT /ty16/w5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"2020-01-02\\\"\"}"},  // PUT /ty16/s6
	{204, ""},  // PUT /ty16/w6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"10:30:00\\\"\"}"},  // PUT /ty16/s7
	{204, ""},  // PUT /ty16/w7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"+01:00:00\\\"\"}"},  // PUT /ty16/s8
	{204, ""},  // PUT /ty16/w8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"550e8400-e29b-41d4-a716-446655440000\\\"\"}"},  // PUT /ty16/s9
	{204, ""},  // PUT /ty16/w9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"true\\\"\"}"},  // PUT /ty16/s10
	{204, ""},  // PUT /ty16/w10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got true\"}"},  // PUT /ty16/s11
	{204, ""},  // PUT /ty16/w11
	{204, ""},  // PUT /ty16/s12
	{204, ""},  // PUT /ty16/w12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got [1,2]\"}"},  // PUT /ty16/s13
	{204, ""},  // PUT /ty16/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty16/s14
	{204, ""},  // PUT /ty16/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty16/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Unknown cast type '_latitude'\"}"},  // PUT /ty16/w15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"POINT(1 2)\\\"\"}"},  // PUT /ty16/s16
	{204, ""},  // PUT /ty16/w16
	{204, ""},  // PUT /ty16/s17
	{204, ""},  // PUT /ty16/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got []\"}"},  // PUT /ty16/s18
	{204, ""},  // PUT /ty16/w18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got 18446744073709551615\"}"},  // PUT /ty16/s19
	{204, ""},  // PUT /ty16/w19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"Abc Def\\\"\"}"},  // PUT /ty16/s20
	{204, ""},  // PUT /ty16/w20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"2020-01-01T00:00:00Z\\\"\"}"},  // PUT /ty16/s21
	{204, ""},  // PUT /ty16/w21
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 's' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/
	{204, ""},  // PUT /ty17/s1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w1' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w1
	{204, ""},  // PUT /ty17/s2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w2' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got 1.5\"}"},  // PUT /ty17/s3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w3' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w3
	{204, ""},  // PUT /ty17/s4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w4' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"abc\\\"\"}"},  // PUT /ty17/s5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w5' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"2020-01-02\\\"\"}"},  // PUT /ty17/s6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w6' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"10:30:00\\\"\"}"},  // PUT /ty17/s7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w7' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"+01:00:00\\\"\"}"},  // PUT /ty17/s8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w8' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"550e8400-e29b-41d4-a716-446655440000\\\"\"}"},  // PUT /ty17/s9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w9' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"true\\\"\"}"},  // PUT /ty17/s10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w10' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got true\"}"},  // PUT /ty17/s11
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w11' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w11
	{204, ""},  // PUT /ty17/s12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w12' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got [1,2]\"}"},  // PUT /ty17/s13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w13' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty17/s14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w14' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty17/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w15' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"POINT(1 2)\\\"\"}"},  // PUT /ty17/s16
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w16' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w16
	{204, ""},  // PUT /ty17/s17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w17' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got []\"}"},  // PUT /ty17/s18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w18' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got 18446744073709551615\"}"},  // PUT /ty17/s19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w19' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"Abc Def\\\"\"}"},  // PUT /ty17/s20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w20' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"2020-01-01T00:00:00Z\\\"\"}"},  // PUT /ty17/s21
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'w21' is a foreign type and as such it cannot have extra fields\"}"},  // PUT /ty17/w21
	{204, ""},  // PUT /ty18/
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s11
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w11
	{204, ""},  // PUT /ty18/s12
	{204, ""},  // PUT /ty18/w12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty18/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s16
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w16
	{204, ""},  // PUT /ty18/s17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/s21
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type: 0x58 is an unknown type\"}"},  // PUT /ty18/w21
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/
	{204, ""},  // PUT /ty19/s1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w1
	{204, ""},  // PUT /ty19/s2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w2
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got 1.5\"}"},  // PUT /ty19/s3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w3
	{204, ""},  // PUT /ty19/s4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w4
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"abc\\\"\"}"},  // PUT /ty19/s5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"2020-01-02\\\"\"}"},  // PUT /ty19/s6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w6
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"10:30:00\\\"\"}"},  // PUT /ty19/s7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"+01:00:00\\\"\"}"},  // PUT /ty19/s8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w8
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"550e8400-e29b-41d4-a716-446655440000\\\"\"}"},  // PUT /ty19/s9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w9
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"true\\\"\"}"},  // PUT /ty19/s10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got true\"}"},  // PUT /ty19/s11
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w11
	{204, ""},  // PUT /ty19/s12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w12
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got [1,2]\"}"},  // PUT /ty19/s13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w13
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got [\\\"a\\\",\\\"b\\\"]\"}"},  // PUT /ty19/s14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 's' is not valid\"}"},  // PUT /ty19/s15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w15
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"POINT(1 2)\\\"\"}"},  // PUT /ty19/s16
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w16
	{204, ""},  // PUT /ty19/s17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w17
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got []\"}"},  // PUT /ty19/s18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w18
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got 18446744073709551615\"}"},  // PUT /ty19/s19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"Abc Def\\\"\"}"},  // PUT /ty19/s20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w20
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 's' expects a value of type integer, got \\\"2020-01-01T00:00:00Z\\\"\"}"},  // PUT /ty19/s21
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: 'nope' not supported, '_type' must be one of { 'object', 'boolean', 'date', 'datetime', 'floating', 'geo', 'integer', 'positive', 'script', '"},  // PUT /ty19/w21
	{204, ""},  // PUT /inf/1
	{204, ""},  // PUT /inf/2
	{204, ""},  // PUT /inf/3
	{204, ""},  // PUT /inf/4
	{204, ""},  // PUT /inf/5
	{204, ""},  // PUT /inf/6
	{204, ""},  // PUT /inf/7
	{204, ""},  // PUT /inf/8
	{204, ""},  // PUT /inf/9
	{204, ""},  // PUT /inf/10
	{204, ""},  // PUT /inf/11
	{204, ""},  // PUT /inf/12
	{204, ""},  // PUT /inf/13
	{204, ""},  // PUT /inf/14
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_latitude' in 'v"},  // PUT /inf/15: which invalid field it names follows the document's key order, platform-dependent (hunt.md)
	{204, ""},  // PUT /inf/16
	{204, ""},  // PUT /inf/17
	{204, ""},  // PUT /inf/18
	{204, ""},  // PUT /inf/19
	{204, ""},  // PUT /inf/20
	{204, ""},  // PUT /inf/21
	{204, ""},  // PUT /t1/1
	{200, "{\"_settings\":{\"number_of_replicas\":0,\"shards\":[\"loopyield\"]},\"_schema\":{\"_id\":{\"_type\":\"integer\",\"_slot\":0,\"_prefix\":\"Q\"},\"s\":{\"_prefix\":\"s.\",\"_ngram\":false,\"_cjk_ngram\":false,"},  // GET /t1/
	{200, "{\"s\":\"hello world\",\"i\":5,\"f\":1.5,\"b\":true,\"d\":\"2020-01-02T00:00:00\",\"dt\":\"2020-01-02T03:04:05Z\",\"tm\":\"10:30:00\",\"td\":\"+01:30:00\",\"u\":\"550e8400-e29b-41d4-a716-446655440000\",\"n\":null,\"o\":{\"x\":1,\"y\":[1,2"},  // GET /t1/1
	{200, "{\"s\":\"hello world\",\"i\":5,\"f\":1.5,\"b\":true,\"d\":\"2020-01-02T00:00:00\",\"dt\":\"2020-01-02T03:04:05Z\",\"tm\":\"10:30:00\",\"td\":\"+01:30:00\",\"u\":\"550e8400-e29b-41d4-a716-446655440000\",\"n\":null,\"o\":{\"x\":1,\"y\":[1,2"},  // GET /t1/1?volatile
	{200, "{\x0a""  \"_settings\": {\x0a""    \"number_of_replicas\": 0,\x0a""    \"shards\": [\x0a""      \"loopyield\"\x0a""    ]\x0a""  },\x0a""  \"_schema\": {\x0a""    \"_id\": {\x0a""      \"_type\": \"integer\",\x0a""      \"_slot\": 0,\x0a""      \"_prefix\": \"Q\"\x0a""    },\x0a""    \"s\""},  // GET /t1/?pretty
	{200, ""},  // HEAD /t1/1
	{200, "{\"docid\":1,\"version\":1,\"data\":[{\"content_"},  // INFO /t1/1
	{200, "{\"endpoint\":\"t1\",\"uuid\":\""},  // INFO /t1/
	{200, ""},  // COMMIT /t1/
	{200, "{\"errors\":{\"t1\":0}}"},  // CHECK /t1/
	{405, "{\"status\":405,\"type\":\"Method Not Allowed\"}"},  // FLUSH /t1/
	{200, "{\"s\":\"hello world\",\"i\":5,\"f\":1.5,\"b\":t"},  // DUMP /t1/1
	{204, ""},  // PUT /id/1
	{204, ""},  // PUT /id/2
	{204, ""},  // PUT /id/3
	{204, ""},  // PUT /id/4
	{409, "{\"status\":409,\"type\":\"Conflict\",\"message\":\"Conflict: Version mismatch: the document does not exist\",\"_version\":0}"},  // PUT /id/5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Data inconsistency, '_data' must be an array or an object\"}"},  // PUT /id/6
	{204, ""},  // PUT /id/7
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_endpoint' in <root> is not valid\"}"},  // PUT /id/8
	{204, ""},  // PUT /id/9
	{204, ""},  // PUT /id/10
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Script 'eb22d411aa1ac55514c4d46c66eca7b2' syntax error: [string \\\"eb22d411aa1ac55514c4d46c66eca7b2\\\"]:1: unexpected symbol near <eof>\"}"},  // PUT /id/11
	{204, ""},  // PUT /id/12
	{204, ""},  // PUT /id/13
	{204, ""},  // PUT /id/14
	{204, ""},  // PUT /id/15
	{204, ""},  // PUT /id/16
	{204, ""},  // PUT /id/17
	{204, ""},  // PUT /id/18
	{204, ""},  // PUT /id/19
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_a' in <root> is not valid\"}"},  // PUT /id/20
	{204, ""},  // PUT /id/21
	{204, ""},  // PUT /id/22
	{204, ""},  // PUT /id/23
	{204, ""},  // PUT /id/24
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'a' expects a value of type integer, got [{\\\"b\\\":1},{\\\"b\\\":2}]\"}"},  // PUT /id/25
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Type mismatch 'integer' -> 'object'\"}"},  // PUT /id/26
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Indexed object must be a JSON, a MsgPack or a blob, is ARRAY\"}"},  // PUT /id/27
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Indexed object must be a JSON, a MsgPack or a blob, is NEGATIVE_INTEGER\"}"},  // PUT /id/28
	{204, ""},  // PUT /id/29
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: JSON parse error at line 1, col: 2 : Missing a name for object member.\\n1. {\\n    ^\"}"},  // PUT /id/30
	{204, ""},  // PUT /id/1?version=1
	{409, "{\"status\":409,\"type\":\"Conflict\",\"message\":\"Conflict: Version mismatch: the current version is 2\",\"_version\":2}"},  // PUT /id/1?version=7
	{409, "{\"status\":409,\"type\":\"Conflict\",\"message\":\"Conflict: Version mismatch: the current version is 2\",\"_version\":2}"},  // PUT /id/1?version=0
	{201, ""},  // POST /id/
	{201, ""},  // POST /id/
	{200, "{\"a\":2,\"_id\":1,\"_version\":2}"},  // GET /id/1
	{204, ""},  // PATCH /id/1  HUNT H5
	{204, ""},  // PATCH /id/1  HUNT H5
	{204, ""},  // PATCH /id/1  HUNT H5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: In patch replace: "},  // PATCH /id/1  HUNT H5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: In patch op: 'bogus' is not a valid value\"}"},  // PATCH /id/1  HUNT H5
	{204, ""},  // PATCH /id/1  HUNT H5
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: A JSON Patch document MUST be an array of objects\"}"},  // PATCH /id/1  HUNT H5
	{204, ""},  // UPDATE /id/1
	{204, ""},  // UPDATE /id/1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Script error: [string \\\"62eb9771afd478a4418f04183ada5707\\\"]:1: attempt to index a nil value (global 'params')\"}"},  // UPDATE /id/1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_op' in 'v' is not valid\"}"},  // UPDATE /id/1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_op' in 'z' is not valid\"}"},  // UPDATE /id/1
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field '_op' in 'z' is not valid\"}"},  // UPDATE /id/1
	{204, ""},  // UPDATE /id/1
	{404, "{\"status\":404,\"type\":\"Not Found\",\"message\":\"Not Found: Document not found\"}"},  // UPDATE /id/99
	{204, ""},  // UPSERT /id/98
	{204, ""},  // UPSERT /id/98
	{200, "{\"z\":3,\"_id\":98,\"_version\":2}"},  // GET /id/98
	{204, ""},  // DELETE /id/1
	{404, "{\"status\":404,\"type\":\"Not Found\",\"message\":\"Not Found: Document not found\"}"},  // DELETE /id/1
	{404, "{\"status\":404,\"type\":\"Not Found\",\"message\":\"Not Found: Document not found\"}"},  // GET /id/1
	{200, ""},  // HEAD /id/2
	{409, "{\"status\":409,\"type\":\"Conflict\",\"message\":\"Conflict: Version mismatch: the current version is 1\",\"_version\":1}"},  // DELETE /id/2?version=9
	{501, "{\"status\":501,\"type\":\"Not Implemented\"}"},  // DELETE /id/
	{204, ""},  // PUT /sc/  HUNT H3
	{200, "{\"_settings\":{\"number_of_replicas\":0,\"shards\":[\"loopyield\"]},\"_schema\":{\"a\":{\"_prefix\":\"a.\","},  // GET /sc/
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Field 'a' expects a value of type integer, got \\\"x\\\"\"}"},  // PUT /sc/1
	{204, ""},  // PUT /sc/2
	{200, "{\"_settings\":{\"number_of_replicas\":0,\"shards\":[\"loopyield\"]},\"_schema\":{\"a\":{\"_prefix\":\"a.\","},  // GET /sc/
	{204, ""},  // PUT /sc/  HUNT H3
	{500, "{\"status\":500,\"type\":\"Internal Server Error\",\"message\":\"Schema metadata is corrupt: '_foreign' must contain index and docid ['x']\"}"},  // PUT /sc/  HUNT H3
	{400, "{\"status\":400,\"type\":\"Bad Request\",\"message\":\"Bad Request: Invalid schema: Schema field '_foreign' does not exist\"}"},  // PUT /sc/  HUNT H3
	{204, ""},  // PUT /sc/  HUNT H3
	{412, "{\"status\":412,\"type\":\"Precondition Failed\",\"message\":\"Type of field 'zz' is missing\"}"},  // PUT /sc/3
	{204, ""},  // PUT /sc2/
	{412, "{\"status\":412,\"type\":\"Precondition Failed\",\"message\":\"Type of field 'b' is missing\"}"},  // PUT /sc2/1
	{204, ""},  // PUT /sc2/2
	{204, ""},  // PUT /sc3/
	{204, ""},  // PUT /sc3/1
	{200, "{\"a\":1,\"b\":2,\"_id\":1,\"_version\":1}"},  // GET /sc3/1
	{204, ""},  // PUT /sc4/
	{500, "{\"status\":500,\"type\":\"Internal Server Error\",\"message\":\"Schema metadata is corrupt: Schema has an unsupported type: integer\"}"},  // PUT /sc4/1  HUNT H4
	{204, ""},  // PUT /sc5/
	{204, ""},  // PUT /sc5/1
	{200, "{\"skip\":1,\"keep\":2,\"_id\":1,\"_version\":1}"},  // GET /sc5/1
	{204, ""},  // PUT /sc6/
	{204, ""},  // PUT /sc6/1
	{204, ""},  // PUT /sc7/1
	{200, "{\"_settings\":{\"number_of_replicas\":0,\"shards\":[\"loopyield\"]},\"_schema\":{\"_id\":{\"_type\":\"integer\",\"_slot\":0,\"_prefix\":\"Q\"},\"a\":{\"_prefix\":\"a.\","},  // GET /sc7/
	{200, "{\"a\":\"2020-01-01T00:00:00\",\"b\":\"1\",\"c\":\"true\",\"d\":\"550e8400-e29b-41d4-a716-446655440000\",\"e\":\"10:00:00\",\"f\":\"+01:00:00\",\"g\":\"x y z\",\"h\":\"POINT(1 2)\",\"i\":\"2020-01-01T10:00:00\",\"_id\":1,\"_version\":1}"},  // GET /sc7/1
	{204, ""},  // PUT /dl/1
	{204, ""},  // PUT /dl/sub/1
	{204, ""},  // DELETE /dl/
	{404, "{\"status\":404,\"type\":\"Not Found\""},  // GET /dl/1
	{200, "{\"b\":2,\"_id\":1,\"_version\":1}"},  // GET /dl/sub/1
	{404, "{\"status\":404,\"type\":\"Not Found\""},  // DELETE /dl/
	{204, ""},  // PUT /dl/sub/2
	{204, ""},  // DELETE /dl/sub/
	{404, "{\"status\":404,\"type\":\"Not Found\""},  // GET /dl/sub/1
	{204, ""},  // PUT /dl/1
	{200, "{\"a\":3,\"_id\":1,\"_version\":1}"},  // GET /dl/1
};
