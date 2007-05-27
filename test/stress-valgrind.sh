#!/bin/sh

# Runs battery of tests forever, with debug output. If
# a test fails, the log is enumerated and saved, and a
# diagnostic is printed to stdout.
#
# This variant runs tests under valgrind.

export MALLOC_CHECK_=2
export MALLOC_PERTURB_=a

let n=0
let m=0

# Make sure binaries are created in .libs

echo -n "Preparing... "

for i in $(cat tests.list); do
  ./$i >/dev/null 2>&1;
done

echo "done"

while true; do
  echo -n -e "\rIterations: $m"

  for i in $(cat tests.list); do
    nice -10 valgrind -q --trace-children=yes --track-fds=yes --time-stamp=yes --num-callers=32 .libs/lt-$i -n >out 2>&1

    if grep -L 'Thread' out >/dev/null 2>&1; then
      echo -e '\nFailure: ' $i
      mv out stress-valgrind-failure-$n.log
      let n=$n+1
    fi
  done

  let m=$m+1

  sleep 1
done 2>&1
