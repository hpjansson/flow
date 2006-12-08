#!/bin/sh

# Runs battery of tests forever, with debug output. If
# a test fails, the log is enumerated and saved, and a
# diagnostic is printed to stdout.
#
# This variant runs tests under gdb, and tries to get
# a backtrace on failure.

export MALLOC_CHECK_=2
export MALLOC_PERTURB_=a

let n=0
let m=0

echo 'run -a'     > .gdb-commands
echo 't a a bt'  >> .gdb-commands
echo 'quit'      >> .gdb-commands

# Make sure binaries are created in .libs

echo -n "Preparing... "

for i in $(cat tests.list); do
  ./$i >/dev/null 2>&1;
done

echo "done"

while true; do
  echo -n -e "\rIterations: $m"

  for i in $(cat tests.list); do
    gdb -q -batch -x .gdb-commands .libs/lt-$i >out 2>&1

    if ! grep -L ': passed' out >/dev/null 2>&1; then
      echo 'Failure: ' $i
      mv out stress-gdb-failure-$n.log
      let n=$n+1
    fi
  done

  let m=$m+1

  sleep 1
done 2>&1

rm .gdb-commands
