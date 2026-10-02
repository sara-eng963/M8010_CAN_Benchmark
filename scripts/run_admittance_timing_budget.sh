#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build -j2 >/dev/null
ctest --test-dir build --output-on-failure >/dev/null

mkdir -p results/admittance_budget
CSV="results/admittance_budget/base_250us_response.csv"
./build/m8010_can_benchmark --response-delay-us 250 --csv "$CSV" >/dev/null

pre_delays=(0 100 250 500 750 1000)

printf "\nM8010 Test 4A — admittance end-to-end timing-budget sensitivity\n"
printf "Assumption: F/T sample becomes available at cycle start; pre-CAN delay aggregates sensor/filter + admittance + kinematics/IK compute.\n"
printf "CAN case: 6 nodes, 1 bus, 1 Mbit/s, RPDO4+SYNC, 500 Hz, 250 us motor TPDO response.\n"
printf "This is an additive timing-budget test; it does not yet model phase-dependent bus interactions from delayed command release.\n\n"
printf "%-12s %-20s %-12s %-16s %-18s %-14s %-14s\n" "preCAN(us)" "profile" "CANcmd(us)" "totalCmd(us)" "totalFeedback(us)" "cmdDeadline" "fbDeadline"
printf '%*s\n' 118 '' | tr ' ' '-'

for d in "${pre_delays[@]}"; do
  awk -F, -v d="$d" '
    NR > 1 && $2 == "interp_rpdo4_sync" && $4 == 1 && $6 == 500 {
      cmd=$18+d; fb=$22+d;
      cmdok=(cmd<=2000 ? "PASS" : "FAIL");
      fbok=(fb<=2000 ? "PASS" : "FAIL");
      printf "%-12d %-20s %-12.1f %-16.1f %-18.1f %-14s %-14s\n", d, $3, $18, cmd, fb, cmdok, fbok;
    }
  ' "$CSV"
done

printf "\nInterpretation: the 2 ms period is the hard scheduling budget. For admittance, feedback age matters because the next cycle should not use stale joint state.\n"
printf "The most conservative allowable pre-CAN budget is approximately 2000 us minus the worst feedback latency of the chosen profile.\n"
