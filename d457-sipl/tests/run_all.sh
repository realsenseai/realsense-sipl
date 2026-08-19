#!/usr/bin/env bash
# run_all.sh — run the full D457 SIPL streaming test suite (on the rig) and report an overall result.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"

TESTS=(
    test_01_each_stream_alone.sh
    test_02_all_three.sh
    test_04_two_stream_permutations.sh
    test_05_resolution.sh
    test_03_startstop_repeat.sh
)

total_fail=0
for t in "${TESTS[@]}"; do
    echo
    echo "####################################################################"
    echo "# $t"
    echo "####################################################################"
    bash "$HERE/$t" || total_fail=$((total_fail+1))
done

echo
echo "####################################################################"
if [ "$total_fail" = "0" ]; then
    echo "#  ALL TEST FILES PASSED"
else
    echo "#  $total_fail TEST FILE(S) FAILED"
fi
echo "####################################################################"
[ "$total_fail" = "0" ]
