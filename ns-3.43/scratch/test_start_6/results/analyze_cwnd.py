import pandas as pd
import matplotlib.pyplot as plt
import os
import re

df = pd.read_csv('scratch/test_start_6/results/cwnd_trace.csv')

# Try to extract RTT delays from the last simulation command in run_and_analyze.sh
rtt_delay_list = []
rtt_pattern = re.compile(r'--RttDelays=([^\s]+)')
run_script_path = os.path.join(os.path.dirname(__file__), '../../run_start_6.sh')
if os.path.exists(run_script_path):
    with open(run_script_path, 'r') as f:
        for line in f:
            match = rtt_pattern.search(line)
            if match:
                rtt_delay_list = match.group(1).split(',')
                break

# Fallback: try to extract from results_summary.csv if not found in shell script
if not rtt_delay_list:
    summary_path = os.path.join(os.path.dirname(__file__), 'results_summary.csv')
    if os.path.exists(summary_path):
        with open(summary_path, 'r') as f:
            header = f.readline().strip().split(',')
            rtt_cols = [col for col in header if col.endswith('_RTT')]
            if rtt_cols:
                last_row = f.readlines()[-1].strip().split(',')
                rtt_delay_list = last_row[-(len(rtt_cols)+1):-1]  # before JainIndex

for protocol in df['Protocol'].unique():
    plt.figure(figsize=(10,6))
    for flow in sorted(df['Flow'].unique()):
        sub = df[(df['Protocol'] == protocol) & (df['Flow'] == flow)]
        if not sub.empty:
            # Use assigned RTT for labeling if available
            flow_idx = int(flow) - 1
            if flow_idx < len(rtt_delay_list):
                rtt_label = rtt_delay_list[flow_idx]
            else:
                rtt_label = "unknown"
            plt.plot(sub['Time'], sub['Cwnd'], label=f'Flow {flow} (RTT={rtt_label})')
    plt.title(f'Congestion Window Evolution - Protocol {protocol}')
    plt.xlabel('Time (s)')
    plt.ylabel('Congestion Window (bytes)')
    plt.legend()
    plt.tight_layout()
    plt.savefig(f'scratch/test_start_6/results/cwnd_{protocol}.png')
    plt.close()