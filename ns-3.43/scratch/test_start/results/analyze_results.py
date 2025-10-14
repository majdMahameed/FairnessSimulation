import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

RESULT_DIR = os.path.dirname(__file__)
INPUT = os.path.join(RESULT_DIR, "results_summary_by_protocol.csv")

if not os.path.exists(INPUT):
    print("Averaged input not found:", INPUT)
    raise SystemExit(1)

df = pd.read_csv(INPUT)
if df.empty:
    print("No data in", INPUT)
    raise SystemExit(1)

# detect throughput columns (FlowX_Mbps)
flow_cols = [c for c in df.columns if c.startswith("Flow") and c.endswith("_Mbps")]
if not flow_cols:
    print("No flow throughput columns found in", INPUT)
    raise SystemExit(1)

# Configure RTT values for each flow
configured_rtts = ["10ms", "10ms"]

# For each protocol produce a separate image comparing flows and showing avg Jain index
for _, row in df.iterrows():
    proto = str(row['Protocol'])
    proto_fname = "".join(ch if ch.isalnum() or ch in "-_." else "_" for ch in proto)

    # throughput values (safe conversion)
    values = []
    for c in flow_cols:
        try:
            v = float(row[c]) if pd.notna(row[c]) else 0.0
        except Exception:
            v = 0.0
        values.append(v)

    # prepare RTT strings for each flow (look for FlowN_RTT columns)
    rtts = []
    for i in range(1, len(flow_cols) + 1):
        rtt_col = f"Flow{i}_RTT"
        if rtt_col in df.columns and pd.notna(row.get(rtt_col, None)):
            rtt_str = str(row[rtt_col])
        else:
            rtt_str = "N/A"
        rtts.append(rtt_str)

    # build x labels: FlowName \n RTT (so RTT appears under the flow name)
    base_labels = [c.replace("_Mbps", "") for c in flow_cols]
    labels = [f"{lab}\n{rtt}" for lab, rtt in zip(base_labels, configured_rtts)]

    # Jain index text
    jain = row.get('JainIndex', None)
    jain_txt = f"Jain index: {float(jain):.4f}" if pd.notna(jain) else "Jain index: N/A"

    plt.figure(figsize=(6,4))
    ax = plt.gca()
    x = np.arange(len(values))
    cmap = plt.get_cmap("tab10")
    colors = [cmap(i % 10) for i in range(len(values))]
    bars = ax.bar(x, values, color=colors)
    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=9)
    ax.set_ylabel("Throughput (Mbps)")
    ax.set_title(f"{proto} — Average per-flow throughput")

    # annotate bars with numeric throughput
    for b, v in zip(bars, values):
        ax.text(b.get_x() + b.get_width()/2, v, f"{v:.3f}", ha='center', va='bottom', fontsize=9)

    # Put Jain index in the top-right inside the axes
    ax.text(0.98, 0.98, jain_txt, transform=ax.transAxes,
            ha='right', va='top', fontsize=9,
            bbox=dict(boxstyle="round,pad=0.3", facecolor='white', alpha=0.85))

    plt.tight_layout()

    outpath = os.path.join(RESULT_DIR, f"throughput_{proto_fname}.png")
    try:
        plt.savefig(outpath)
    finally:
        plt.close()
    print("Saved", outpath)

# produce a combined figure showing all protocols side-by-side for the flows
protocols = df['Protocol'].astype(str).tolist()
data = df[flow_cols].astype(float).values
jain_indices = df['JainIndex'].tolist() if 'JainIndex' in df.columns else [None] * len(protocols)
num_protocols, num_flows = data.shape
x = np.arange(num_protocols)
width = 0.8 / max(1, num_flows)

plt.figure(figsize=(10, 6))
cmap = plt.get_cmap("tab10")
for i in range(num_flows):
    plt.bar(x + i * width - (width*(num_flows-1)/2), data[:, i], width,
            label=flow_cols[i].replace("_Mbps",""), color=cmap(i % 10))

# Annotate Jain index beside each protocol label
xtick_labels = []
for idx, proto in enumerate(protocols):
    jain = jain_indices[idx]
    if jain is not None and not pd.isna(jain):
        label = f"{proto}\n(Jain: {jain:.4f})"
    else:
        label = proto
    xtick_labels.append(label)
plt.xticks(x, xtick_labels, rotation=45, ha='right')

plt.xlabel("Protocol")
plt.ylabel("Throughput (Mbps)")
plt.title("Average per-flow throughput by protocol")
plt.legend(title="Flow")
plt.tight_layout()
combined_out = os.path.join(RESULT_DIR, "throughput_per_flow_by_protocol.png")
plt.savefig(combined_out)
plt.close()
print("Saved", combined_out)