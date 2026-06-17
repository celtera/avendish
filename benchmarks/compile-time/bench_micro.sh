#!/usr/bin/env bash
CXX="$1"; STDLIB="$2"; REFLFLAG="$3"; LABEL="$4"; RUNS=3
for c in /w/micro/*.cpp; do
  n=$(basename "$c" .cpp)
  fl=""; case "$n" in refl_*) fl="$REFLFLAG";; esac
  best=99999
  for r in $(seq 1 $RUNS); do
    t0=$(date +%s.%N); $CXX -std=c++26 $fl $STDLIB -c -O0 "$c" -o /tmp/o.o 2>/tmp/e.log; rc=$?; t1=$(date +%s.%N)
    s=$(awk "BEGIN{printf \"%.3f\", $t1-$t0}")
    [ $rc -ne 0 ] && { best=NA; break; }
    awk "BEGIN{exit !($s<$best)}" && best=$s
  done
  echo "$LABEL,$n,$best"
done
