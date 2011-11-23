#!/bin/bash

# Runs battery of tests forever, with debug output. If
# a test fails, the log is enumerated and saved, and a
# diagnostic is printed to stdout.

export MALLOC_CHECK_=2
export MALLOC_PERTURB_=a

let n=0
let m=0

while true; do
  echo -n -e "\rIterations: $m"

  for i in $(grep "^[ \t]*[^#]" tests.list); do
    if ! nice -10 libtool --mode=execute ./$i -v >out 2>&1; then
      echo 'Failure: ' $(head -1 out)
      mv out stress-failure-$n.log
      let n=$n+1
    fi
  done

  let m=$m+1

  sleep 1
done 2>&1

