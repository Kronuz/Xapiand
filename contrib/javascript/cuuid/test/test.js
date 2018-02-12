/*
 * Dubalu Framework
 * ~~~~~~~~~~~~~~~~
 *
 * :author: Dubalu Framework Team. See AUTHORS.
 * :copyright: Copyright (C) 2015-2018 dubalu.com LLC. All rights reserved.
 * :license: See LICENSE for more details.
 *
 */

var assert = require('assert');

var UUID = require('../');
var repr = require('../repr');

uuids = [
	// Full:
	['5759b016-10c0-4526-a981-47d6d19f6fb4', ['5759b016-10c0-4526-a981-47d6d19f6fb4'], '5759b016-10c0-4526-a981-47d6d19f6fb4', '\\x1WY\\xb0\\x16\\x10\\xc0E&\\xa9\\x81G\\xd6\\xd1\\x9fo\\xb4'],
	['e8b13d1b-665f-4f4c-aa83-76fa782b030a', ['e8b13d1b-665f-4f4c-aa83-76fa782b030a'], 'e8b13d1b-665f-4f4c-aa83-76fa782b030a', '\\x1\\xe8\\xb1=\\x1bf_OL\\xaa\\x83v\\xfax+\\x3\\n'],
	// Condensed:
	['00000000-0000-1000-8000-000000000000', ['00000000-0000-1000-8000-000000000000'], '00000000-0000-1000-8000-000000000000', '\\x1c\\x0\\x0\\x0'],
	['11111111-1111-1111-8111-111111111111', ['11111111-1111-1111-8111-111111111111'], '~GcL2nemYXfTUrDbsYYiTDNc', '\\xf\\x88\\x88\\x88\\x88\\x88\\x88\\x88\\x82"""""""'],
	// Condensed + Compacted:
	['230c0800-dc3c-11e7-b966-a3ab262e682b', ['230c0800-dc3c-11e7-b966-a3ab262e682b'], '~SsQg3rJrx3P', '\\x6,\\x2[\\x89fW'],
	['f2238800-debf-11e7-bbf7-dffcee0c03ab', ['f2238800-debf-11e7-bbf7-dffcee0c03ab'], '~SlMSibYTT8c', '\\x6.\\x86*\\x1f\\xbb\\xf7W'],
	// Condensed + Expanded:
	['60579016-dec5-11e7-b616-34363bc9ddd6', ['60579016-dec5-11e7-b616-34363bc9ddd6'], '60579016-dec5-11e7-b616-34363bc9ddd6', '\\xe1\\x17E\\xcc)\\xc4\\xbl,hlw\\x93\\xbb\\xac'],
	['4ec97478-c3a9-11e6-bbd0-a46ba9ba5662', ['4ec97478-c3a9-11e6-bbd0-a46ba9ba5662'], '4ec97478-c3a9-11e6-bbd0-a46ba9ba5662', '\\xe\\x89\\xb7\\xc3b\\xb6<w\\xa1H\\xd7St\\xac\\xc4'],
	// Other:
	['00000000-0000-0000-0000-000000000000', ['00000000-0000-0000-0000-000000000000'], '00000000-0000-0000-0000-000000000000', '\\x1\\x0\\x0\\x0\\x0\\x0\\x0\\x0\\x0\\x0\\x0\\x0\\x0\\x0\\x0\\x0\\x0'],
	['00000000-0000-1000-8000-010000000000', ['00000000-0000-1000-8000-010000000000'], '~notmet', '\\x1c\\x0\\x0\\x1'],
	['11111111-1111-1111-8111-101111111111', ['11111111-1111-1111-8111-101111111111'], '11111111-1111-1111-8111-101111111111', '\\xf7\\x95\\xb0k\\xa4\\x86\\x84\\x88\\x82" """""'],
	['00000000-0000-1000-a000-000000000000', ['00000000-0000-1000-a000-000000000000'], '00000000-0000-1000-a000-000000000000', '\\n@\\x0\\x0\\x0\\x0\\x0\\x0\\x0'],
	// Coumpound:
	['5759b016-10c0-4526-a981-47d6d19f6fb4;e8b13d1b-665f-4f4c-aa83-76fa782b030a', ['5759b016-10c0-4526-a981-47d6d19f6fb4', 'e8b13d1b-665f-4f4c-aa83-76fa782b030a'], '5759b016-10c0-4526-a981-47d6d19f6fb4;e8b13d1b-665f-4f4c-aa83-76fa782b030a', '\\x1WY\\xb0\\x16\\x10\\xc0E&\\xa9\\x81G\\xd6\\xd1\\x9fo\\xb4\\x1\\xe8\\xb1=\\x1bf_OL\\xaa\\x83v\\xfax+\\x3\\n'],
	['00000000-0000-1000-8000-000000000000;11111111-1111-1111-8111-111111111111', ['00000000-0000-1000-8000-000000000000', '11111111-1111-1111-8111-111111111111'], '~WPQl2On7vMpv7TMQBPSquHXo4WWz', '\\x1c\\x0\\x0\\x0\\xf\\x88\\x88\\x88\\x88\\x88\\x88\\x88\\x82"""""""'],
	['230c0800-dc3c-11e7-b966-a3ab262e682b;f2238800-debf-11e7-bbf7-dffcee0c03ab', ['230c0800-dc3c-11e7-b966-a3ab262e682b', 'f2238800-debf-11e7-bbf7-dffcee0c03ab'], '~KY9OflmS8UZsL8Ug64UcVQ', '\\x6,\\x2[\\x89fW\\x6.\\x86*\\x1f\\xbb\\xf7W'],
	['60579016-dec5-11e7-b616-34363bc9ddd6;4ec97478-c3a9-11e6-bbd0-a46ba9ba5662', ['60579016-dec5-11e7-b616-34363bc9ddd6', '4ec97478-c3a9-11e6-bbd0-a46ba9ba5662'], '60579016-dec5-11e7-b616-34363bc9ddd6;4ec97478-c3a9-11e6-bbd0-a46ba9ba5662', '\\xe1\\x17E\\xcc)\\xc4\\xbl,hlw\\x93\\xbb\\xac\\xe\\x89\\xb7\\xc3b\\xb6<w\\xa1H\\xd7St\\xac\\xc4'],
	['00000000-0000-1000-8000-010000000000;11111111-1111-1111-8111-101111111111', ['00000000-0000-1000-8000-010000000000', '11111111-1111-1111-8111-101111111111'], '00000000-0000-1000-8000-010000000000;11111111-1111-1111-8111-101111111111', '\\x1c\\x0\\x0\\x1\\xf7\\x95\\xb0k\\xa4\\x86\\x84\\x88\\x82" """""'],
	['~HAhyatqQHjUq9ztms7NrcPfd34a8PQ3pa3D2BxYrzN55QD', ['5759b016-10c0-4526-a981-47d6d19f6fb4', 'e8b13d1b-665f-4f4c-aa83-76fa782b030a'], '5759b016-10c0-4526-a981-47d6d19f6fb4;e8b13d1b-665f-4f4c-aa83-76fa782b030a', '\\x1WY\\xb0\\x16\\x10\\xc0E&\\xa9\\x81G\\xd6\\xd1\\x9fo\\xb4\\x1\\xe8\\xb1=\\x1bf_OL\\xaa\\x83v\\xfax+\\x3\\n'],
	['~WPQl2On7vMpv7TMQBPSquHXo4WWz', ['00000000-0000-1000-8000-000000000000', '11111111-1111-1111-8111-111111111111'], '~WPQl2On7vMpv7TMQBPSquHXo4WWz', '\\x1c\\x0\\x0\\x0\\xf\\x88\\x88\\x88\\x88\\x88\\x88\\x88\\x82"""""""'],
	['~KY9OflmS8UZsL8Ug64UcVQ', ['230c0800-dc3c-11e7-b966-a3ab262e682b', 'f2238800-debf-11e7-bbf7-dffcee0c03ab'], '~KY9OflmS8UZsL8Ug64UcVQ', '\\x6,\\x2[\\x89fW\\x6.\\x86*\\x1f\\xbb\\xf7W'],
	['~yoLaxhaywcbaBPzQQmZlCSFts6Sm9fCxD76ctNiw7q', ['60579016-dec5-11e7-b616-34363bc9ddd6', '4ec97478-c3a9-11e6-bbd0-a46ba9ba5662'], '60579016-dec5-11e7-b616-34363bc9ddd6;4ec97478-c3a9-11e6-bbd0-a46ba9ba5662', '\\xe1\\x17E\\xcc)\\xc4\\xbl,hlw\\x93\\xbb\\xac\\xe\\x89\\xb7\\xc3b\\xb6<w\\xa1H\\xd7St\\xac\\xc4'],
	['~WPQlEPuOJZ3Y9TclmoaUhJFiUicy', ['00000000-0000-1000-8000-010000000000', '11111111-1111-1111-8111-101111111111'], '00000000-0000-1000-8000-010000000000;11111111-1111-1111-8111-101111111111', '\\x1c\\x0\\x0\\x1\\xf7\\x95\\xb0k\\xa4\\x86\\x84\\x88\\x82" """""']
];

test('cuuid', function() {
	uuids.forEach(uuid => {
		const str_uuid = uuid[0];
		const expected = uuid[1];
		const expected_encoded = uuid[2];
		const expected_serialised = uuid[3];

		const serialised = UUID.decode(str_uuid);
		const result = UUID.unserialise(serialised);
		assert.deepEqual(result, expected);

		const result_encoded = UUID.encode(serialised);
		assert.equal(result_encoded, expected_encoded);
		assert.equal(repr.repr(serialised), expected_serialised);
	});
});