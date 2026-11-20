# faultlab-disklab: disklab: dm-log-writes replay to any flush mark, dm-delay, dm-flakey, in a Linux VM (design F15)

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status faultlab-disklab`.

## Outcome

disklab, faultlab's disk tier (F15 in cpp-libraries `docs/design/faultlab.md`): in a Linux VM, dm-log-writes records every write with its flush and FUA marks and replays the device to any mark, which gives the states a real filesystem leaves at a real power cut; dm-delay slows the device and dm-flakey fails or drops writes for a window. Driven from the host over SSH, as jobs on our Linux VM; never on the host.

## Acceptance criteria

- A1 A planted unsynced write is missing when replayed to the mark before its flush and present at the mark after; the job states seconds per replay: a job
- A2 dm-delay adds the configured latency to fsync (±10%) and dm-flakey fails writes inside its window and none outside it: a job
- A3 Every fault writes its event in the history log: a test
- A4 The build-test check passes, and CI is green

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
