# keymaker-keys: multivalue: sort keys keep their minimum, slot, direction and phonetic encoder (M1, M4, M5)

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status keymaker-keys`.

## Outcome

Sort keys keep what they encode: MIN_STR_CMPVALUE is a one-byte NUL (M1, keymaker.cc; std::string("\x00") is empty), StringKey::serialize writes its slot and direction (M5, keymaker.cc:466), and SerializablePhoneticMetric::unserialize keeps the encoder code so a rebuilt soundex or dmetaphone key round-trips at its original length (M4: 13 -> 8 bytes for soundex english). Pins from coverage-multivalue (jobs 261119-035511-e1d625, 261119-041546-ce5...) flip to the right values.

## Acceptance criteria

- A1 A descending string sort with a missing slot orders the missing value as the minimum; a test checks the order and fails without the fix
- A2 Every sort key kind serializes and unserializes to an identical key (bytes equal) and sorts identically; tests for string (slot, direction) and phonetic (soundex, dmetaphone) fail without the fix
- A3 The coverage-multivalue pins for M1, M4, M5 assert the fixed values
- A4 The suite passes and CI is green

## Contracts

None beyond the fixed behavior.

## Decisions

- From hunt.md rows; filed by the lead 2026-11-19.

## May land early

None.
