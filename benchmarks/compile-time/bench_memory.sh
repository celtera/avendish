#!/usr/bin/env bash
CXX="$1"; STDLIB="$2"; REFLFLAG="$3"; LABEL="$4"
apt-get update -qq >/dev/null 2>&1; apt-get install -y -qq time >/dev/null 2>&1
INC="-I /repo/include -I /repo/examples -I /me -I /repo"
for n in audioeffect kabang; do
  for cfg in "pfr:-DAVND_USE_BOOST_PFR=1" "p1061:" "refl:$REFLFLAG"; do
    bn=${cfg%%:*}; fl=${cfg#*:}
    rss=$(/usr/bin/time -f "%M" $CXX -std=c++26 $fl $STDLIB -c -O0 $INC /w/cases/$n.cpp -o /tmp/o.o 2>&1 >/dev/null | tail -1)
    mb=$(awk "BEGIN{printf \"%.0f\", $rss/1024}")
    echo "$LABEL,$n,$bn,${mb}MB"
  done
done
