#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build -j2 >/dev/null
ctest --test-dir build --output-on-failure >/dev/null

mkdir -p results/delay_sweep

delays=(0 100 250 500 750)

printf "\nM8010 Test 2 — 500 Hz response-delay sensitivity\n"
printf "%-10s %-20s %-10s %-14s %-14s %-10s\n" "delay(us)" "profile" "load%" "maxCmd(us)" "maxFeedback(us)" "misses"
printf '%*s\n' 84 '' | tr ' ' '-'

for d in "${delays[@]}"; do
    csv="results/delay_sweep/delay_${d}us.csv"
    ./build/m8010_can_benchmark --response-delay-us "$d" --csv "$csv" >/dev/null

    awk -F, -v delay="$d" '
        NR > 1 && $2 == "interp_rpdo4_sync" && $4 == 1 && $6 == 500 {
            printf "%-10s %-20s %-10.1f %-14.1f %-14.1f %-10d\n", delay, $3, $15, $18, $22, ($24 + $25)
        }
    ' "$csv"
done

printf "\nCSV files written under results/delay_sweep/\n"
printf "Focus: one bus, six M8010 nodes, 1 Mbit/s, RPDO4 + SYNC, 500 Hz.\n"
