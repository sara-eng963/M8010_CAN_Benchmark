#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build -j2 >/dev/null
ctest --test-dir build --output-on-failure >/dev/null

mkdir -p results/bitrate_sweep
bitrates=(250000 500000 800000 1000000)

printf "\nM8010 Test 5 — CAN bitrate sensitivity at 500 Hz\n"
printf "6 nodes, 1 bus, RPDO4+SYNC, 250 us assumed motor response delay\n\n"
printf "%-12s %-20s %-10s %-14s %-16s %-10s %-10s %-10s\n" "bitrate" "profile" "load%" "maxCmd(us)" "maxFeedback" "cmdMiss" "fbMiss" "screen"
printf '%*s\n' 112 '' | tr ' ' '-'

for b in "${bitrates[@]}"; do
    csv="results/bitrate_sweep/bitrate_${b}.csv"
    ./build/m8010_can_benchmark --bitrate "$b" --response-delay-us 250 --csv "$csv" >/dev/null
    awk -F, -v bitrate="$b" '
        NR > 1 && $2 == "interp_rpdo4_sync" && $4 == 1 && $6 == 500 {
            label = sprintf("%.0f kbit/s", bitrate/1000.0)
            printf "%-12s %-20s %-10.1f %-14.1f %-16.1f %-10d %-10d %-10s\n", label, $3, $15, $18, $22, $24, $25, $29
        }
    ' "$csv"
done

printf "\nCSV files written under results/bitrate_sweep/\n"
printf "Purpose: determine whether the one-bus 500 Hz design depends on the M8010 actually supporting 1 Mbit/s CANopen.\n"
