#!/bin/bash
B=${LAG_FLOOR:-.scratch/bin/lag_floor}
for args in "--secs 20" "--secs 20 --spin" "--secs 20 --busy" "--secs 20 --busy --spin" "--secs 20 --burn 8 --spin" "--secs 20 --burn 8 --busy --spin"; do
  echo "== $args"; $B $args | awk 'NR%4==0||NR==1'
done
