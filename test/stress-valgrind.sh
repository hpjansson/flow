#!/bin/bash

# Runs battery of tests forever, with debug output. If
# a test fails, the log is enumerated and saved, and a
# diagnostic is printed to stdout.
#
# This variant runs tests under valgrind.

export MALLOC_CHECK_=2
export MALLOC_PERTURB_=a

let n=0
let m=0

while true; do
  echo -n -e "\rIterations: $m"

  for i in $(grep "^[ \t]*[^#]" tests.list); do
    nice -10 libtool --mode=execute valgrind -q --trace-children=yes --track-fds=yes --time-stamp=yes --num-callers=32 $i -n >out 2>&1

    if grep -L 'Thread' out >/dev/null 2>&1; then
      echo -e '\nFailure: ' $i
      mv out stress-valgrind-failure-$n.log
      let n=$n+1
    fi
  done

  let m=$m+1

  sleep 1
done 2>&1
