#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
OUTPUT_DIR="${1:-results/matrix}"

"${BUILD_DIR}/can_benchmark" \
  --bitrate 1000000 \
  --duration 10 \
  --nodes 6 \
  --node-response-us 0 \
  --stuffing exact \
  --output-dir "${OUTPUT_DIR}" \
  --matrix
