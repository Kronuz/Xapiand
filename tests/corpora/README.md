# Restore corpora

Each directory holds `rt.mp`, a `--dump` of the index `rt` holding `source.ndjson` (3000 documents, `harness/restore_corpora_gen.py`), and a `BUILD` line. Replay: `harness/restore_corpora.sh check BIN`.

- `alpha9/rt.mp`: written by Xapiand 1.0.0-alpha.9, the last Xapian release (git tag v1.0.0-alpha.9, 6961bb98c), RelWithDebInfo, by `restore_corpora.sh write` (job 261111-144242-730675).
- `master/rt.mp`: written by Xapiand 1.0.0-alpha.10, master at 9bfc9ca08 (the build also carries this harness, a7d165cd3), RelWithDebInfo, by `restore_corpora.sh write` (job 261111-144244-01ce7e).

The two files are byte-identical (1,409,249 bytes each).
