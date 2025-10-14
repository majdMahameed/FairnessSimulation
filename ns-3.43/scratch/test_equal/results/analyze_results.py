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

# For each protocol produce a separate image comparing flows and showing avg Jain index
for _, row in df.iterrows():
    proto = str(row['Protocol'])
    # sanitize filename
    proto_fname = "".join(ch if ch.isalnum() or ch in "-_." else "_" for ch in proto)

    values = [float(row[c]) if not pd.isna(row[c]) else 0.0 for c in flow_cols]
    labels = [c.replace("_Mbps", "") for c in flow_cols]

    jain = row.get('JainIndex', None)
    jain_txt = f"Jain index: {float(jain):.4f}" if pd.notna(jain) else "Jain index: N/A"

    plt.figure(figsize=(6,4))
    x = np.arange(len(values))
    bars = plt.bar(x, values, color=plt.get_cmap("tab10").colors[:len(values)])
    plt.xticks(x, labels)
    plt.ylabel("Throughput (Mbps)")
    plt.title(f"{proto} — Average per-flow throughput\n{jain_txt}")
    # annotate bars
    for b, v in zip(bars, values):
        plt.text(b.get_x() + b.get_width()/2, v, f"{v:.3f}", ha='center', va='bottom', fontsize=9)
    plt.tight_layout()

    outpath = os.path.join(RESULT_DIR, f"throughput_{proto_fname}.png")
    plt.savefig(outpath)
    plt.close()
    print("Saved", outpath)

# also produce a combined figure showing all protocols side-by-side for the flows
# rows = protocols, cols = flows
protocols = df['Protocol'].astype(str).tolist()
data = df[flow_cols].astype(float).values
jain_indices = df['JainIndex'].tolist() if 'JainIndex' in df.columns else [None] * len(protocols)
num_protocols, num_flows = data.shape
x = np.arange(num_protocols)
width = 0.8 / max(1, num_flows)

plt.figure(figsize=(10, 6))
for i in range(num_flows):
    plt.bar(x + i * width - (width*(num_flows-1)/2), data[:, i], width, label=flow_cols[i].replace("_Mbps",""))

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