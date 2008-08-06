#!/bin/bash

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

# Check prerequisites

for util in gdb expect dos2unix; do
  if ! which $util >/dev/null 2>&1; then
    echo "The '$util' utility was not found in your path. Aborting."
    exit 1
  fi
done

# Make sure binaries are created in .libs

echo -n "Preparing... "

for i in $(grep "^[ \t]*[^#]" tests.list); do
  ./$i >/dev/null 2>&1;
done

echo "done"

while true; do
  echo -n -e "\rIterations: $m"

  for i in $(grep "^[ \t]*[^#]" tests.list); do
    cat <<EOF >.expect-commands
proc send {ignore arg} {
  sleep .1
  exp_send -- \$arg
}

proc sendctrlc {} {
  send -- ""
}

set timeout 120
spawn gdb -n -q .libs/lt-$i
match_max 100000
expect -exact "(gdb) "
send -- "set pagination off\r"
expect -exact "(gdb) "
send -- "run -n -v\r"
expect {
  timeout {
    sendctrlc
    exp_continue
  }
  "(gdb) "
}
send -- "t a a bt\r"
expect -exact "(gdb) "
send -- "quit\r"
expect {
  "(y or n) " {
    send -- "y\r"
    exp_continue
  }
  eof
}
EOF

    # We need the dos2unix stage for grep's sake.
    expect -f .expect-commands 2>&1 | dos2unix > out

    if ! grep -L 'passed' out >/dev/null 2>&1; then
      echo -e '\nFailure: ' $i
      mv out stress-gdb-failure-$n.log
      let n=$n+1
    fi
  done

  let m=$m+1

  sleep 1
done 2>&1

rm .expect-commands
