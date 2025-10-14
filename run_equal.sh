#!/bin/bash
set -euo pipefail

PROTOCOLS=("DCTCP")
# "NewReno" "Cubic" "BBR"
# change this if your ns-3 run command differs
NS3_RUN_CMD="./ns3 run scratch/test_equal/test_everything_equal_cmd.cc -- --TcpProtocol="

RESULT_DIR="scratch/test_equal/results"
mkdir -p "$RESULT_DIR"

# Run all simulations and analyze cwnd after each protocol
for proto in "${PROTOCOLS[@]}"; do
    echo "=== Running simulation with protocol: $proto ==="
    ${NS3_RUN_CMD}"${proto}"

    echo "=== Running cwnd analysis for protocol: $proto ==="
    if python3 "${RESULT_DIR}/analyze_cwnd.py"; then
        # If a generic cwnd.png is created, rename it to include the protocol
        if [ -f "${RESULT_DIR}/cwnd.png" ] && [ ! -f "${RESULT_DIR}/cwnd_${proto}.png" ]; then
            mv -f "${RESULT_DIR}/cwnd.png" "${RESULT_DIR}/cwnd_${proto}.png"
        fi
    else
        echo "Warning: analyze_cwnd.py failed for ${proto}"
    fi
done

# After all simulations, compute per-protocol averages
echo "=== Aggregating results by protocol ==="
python3 "${RESULT_DIR}/analyze_summary_by_protocol.py"

# Now produce plots from the averaged CSV
echo "=== Producing throughput plot from averaged results ==="
python3 "${RESULT_DIR}/analyze_results.py"

echo "=== Producing cwnd plots from averaged results ==="
python3 "${RESULT_DIR}/analyze_cwnd.py"

echo "All done. Check ${RESULT_DIR} for"