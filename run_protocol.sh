#!/bin/bash
set -euo pipefail

RTT_DELAYS="10ms,10ms,10ms"
NS3_RUN_CMD="./ns3 run scratch/test_protocol/test_protocol_cmd.cc --"

RESULT_DIR="scratch/test_protocol/results"
mkdir -p "$RESULT_DIR"

echo "=== Running simulation with 3 flows: NewReno, Cubic, BBR ==="
${NS3_RUN_CMD} --numFlows=3 --RttDelays="${RTT_DELAYS}"

echo "=== Running cwnd analysis for all protocols ==="
if python3 "${RESULT_DIR}/analyze_cwnd.py"; then
    if [ -f "${RESULT_DIR}/cwnd.png" ]; then
        mv -f "${RESULT_DIR}/cwnd.png" "${RESULT_DIR}/cwnd_all_protocols.png"
    fi
else
    echo "Warning: analyze_cwnd.py failed"
fi

echo "=== Aggregating results by protocol ==="
python3 "${RESULT_DIR}/analyze_summary_by_protocol.py"

echo "=== Producing throughput plots from averaged results ==="
python3 "${RESULT_DIR}/analyze_results.py"

echo "All done. Check ${RESULT_DIR} for results."