#!/bin/bash
set -euo pipefail

PROTOCOLS=("NewReno" "Cubic" "DCTCP" "BBR")
RTT_DELAYS="10ms,10ms,10ms,10ms,10ms"
# change this if your ns-3 run command differs
NS3_RUN_CMD="./ns3 run scratch/test_start_6/test_start_cmd.cc -- --TcpProtocol="

RESULT_DIR="scratch/test_start_6/results"
mkdir -p "$RESULT_DIR"

# Run all simulations; run cwnd analysis after each protocol run
for proto in "${PROTOCOLS[@]}"; do
    echo "=== Running simulation with protocol: $proto ==="
    ${NS3_RUN_CMD}${proto} --RttDelays="${RTT_DELAYS}"

    echo "=== Running cwnd analysis for protocol: $proto ==="
    # run per-run cwnd analysis (script should read cwnd_trace.csv)
    if python3 "${RESULT_DIR}/analyze_cwnd.py"; then
        # prefer protocol-labelled output if script created a generic cwnd.png
        if [ -f "${RESULT_DIR}/cwnd.png" ] && [ ! -f "${RESULT_DIR}/cwnd_${proto}.png" ]; then
            mv -f "${RESULT_DIR}/cwnd.png" "${RESULT_DIR}/cwnd_${proto}.png"
        fi
    else
        echo "Warning: analyze_cwnd.py failed for ${proto}"
    fi
done

# After all simulations, compute per-protocol averages and produce aggregated plots
echo "=== Aggregating results by protocol ==="
python3 "${RESULT_DIR}/analyze_summary_by_protocol.py"

echo "=== Producing throughput plots from averaged results ==="
python3 "${RESULT_DIR}/analyze_results.py"

echo "All done. Check"