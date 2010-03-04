set -vex
not () { "$@" && exit 1 || return 0; }

divine reachability --report peterson-naive.dve > report
grep "^Finished: Yes" report
grep "^States-Visited:" report
grep "^States-Visited: 94062" report
