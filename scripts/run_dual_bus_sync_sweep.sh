#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build -j2 >/dev/null
ctest --test-dir build --output-on-failure >/dev/null

mkdir -p results/dual_bus_sync

delays=(0 100 250 500 750 1000)

printf "\nM8010 Test 5B — dual-bus SYNC skew sensitivity\n"
printf "Fallback architecture: 6 nodes split 3+3, two 500 kbit/s CAN buses, RPDO4+SYNC, 500 Hz\n\n"
printf "%-12s %-20s %-10s %-14s %-14s %-14s %-10s %-10s\n" "respDelay" "profile" "load%" "maxCmd(us)" "p99Skew(us)" "maxSkew(us)" "cmdMiss" "fbMiss"
printf '%*s\n' 116 '' | tr ' ' '-'

for d in "${delays[@]}"; do
    csv="results/dual_bus_sync/delay_${d}us.csv"
    ./build/m8010_can_benchmark --bitrate 500000 --response-delay-us "$d" --csv "$csv" >/dev/null
    awk -F, -v delay="$d" '
        NR > 1 && $2 == "interp_rpdo4_sync" && $4 == 2 && $6 == 500 {
            printf "%-12s %-20s %-10.1f %-14.1f %-14.1f %-14.1f %-10d %-10d\n", \
                   delay " us", $3, $15, $18, $21, $20, $24, $25
        }
    ' "$csv"
done

printf "\nInterpretation: execution skew is the difference between the latest and earliest joint execution timestamps in each cycle.\n"
printf "For two buses, nonzero skew can arise because each bus carries a separate SYNC frame and the buses can be occupied differently.\n"
printf "This is a simulator sensitivity result; final dual-FDCAN hardware timing must still be measured on the STM32H745.\n"
