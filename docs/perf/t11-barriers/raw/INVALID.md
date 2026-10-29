> Status: CURRENT (2026-10-29) invalid runs in this directory.

Round 1 of `grid1` for arms B1 and B2 and of `grid2` for arm F is invalid on the profile metrics (engine CPU, critical thread):
`profile_wall_s` is 10.0 to 11.9 s against 4.5 to 5.4 s for every other strata run, so the sampler's window did not cover
the RESTORE. These are the first executions of a freshly built binary (the smoke run in the same label is normal). Every
summary quoted from these data drops round 1 for all arms (four rounds), and says so; the all-five-round figures are in
the README for completeness.
