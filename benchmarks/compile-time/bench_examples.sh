#!/usr/bin/env bash
# args: CXX STDLIB REFLFLAG LABEL  (emits CSV: label,example,backend,run,seconds)
CXX="$1"; STDLIB="$2"; REFLFLAG="$3"; LABEL="$4"; RUNS=6
INC="-I /repo/include -I /repo/examples -I /me -I /repo"
for c in /w/cases/*.cpp; do
  n=$(basename "$c" .cpp)
  for cfg in "pfr:-DAVND_USE_BOOST_PFR=1" "p1061:" "refl:$REFLFLAG"; do
    bn=${cfg%%:*}; fl=${cfg#*:}
    for r in $(seq 1 $RUNS); do
      t0=$(date +%s.%N)
      $CXX -std=c++26 $fl $STDLIB -c -O0 $INC "$c" -o /tmp/o_$n.o 2>/dev/null
      rc=$?
      t1=$(date +%s.%N)
      secs=$(awk "BEGIN{printf \"%.3f\", $t1-$t0}")
      [ $rc -ne 0 ] && secs=NA
      echo "$LABEL,$n,$bn,$r,$secs"
    done
  done
done
