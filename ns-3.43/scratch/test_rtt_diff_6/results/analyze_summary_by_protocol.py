import os
import sys
import re
import numpy as np
import pandas as pd

INPUT = "scratch/test_rtt_diff_6/results/results_summary.csv"
OUTPUT = "scratch/test_rtt_diff_6/results/results_summary_by_protocol.csv"

if not os.path.exists(INPUT):
    print(f"Input file not found: {INPUT}")
    sys.exit(1)

df = pd.read_csv(INPUT)
if df.empty:
    print("Input CSV is empty.")
    sys.exit(1)

# normalize column names (trim spaces)
df.columns = [c.strip() for c in df.columns]

# detect flow throughput columns (FlowN_Mbps) and RTT columns (FlowN_RTT)
flow_throughput_cols = [c for c in df.columns if c.startswith('Flow') and c.endswith('_Mbps')]
flow_rtt_cols = [c for c in df.columns if c.startswith('Flow') and c.endswith('_RTT')]

if not flow_throughput_cols:
    print("No flow throughput columns found (expected FlowX_Mbps). Columns:", df.columns.tolist())
    sys.exit(1)

# ensure numeric throughput columns
for c in flow_throughput_cols:
    df[c] = pd.to_numeric(df[c], errors='coerce')

# parse RTT strings to seconds for averaging
def parse_time_to_seconds(s):
    if pd.isna(s):
        return np.nan
    s = str(s).strip()
    m = re.match(r'^([0-9]*\.?[0-9]+)\s*(ms|us|s)?$', s, flags=re.IGNORECASE)
    if not m:
        try:
            return float(s)
        except Exception:
            return np.nan
    val = float(m.group(1))
    unit = (m.group(2) or '').lower()
    if unit == 'ms':
        return val / 1000.0
    if unit == 'us':
        return val / 1e6
    # default or 's'
    return val

# create numeric RTT columns (seconds) for averaging
rtt_seconds_cols = []
for c in flow_rtt_cols:
    sec_col = c + "_s"
    df[sec_col] = df[c].apply(parse_time_to_seconds)
    rtt_seconds_cols.append(sec_col)

# ensure JainIndex numeric if present
if 'JainIndex' in df.columns:
    df['JainIndex'] = pd.to_numeric(df['JainIndex'], errors='coerce')

# columns to aggregate: throughputs + RTT_seconds + JainIndex (if present)
agg_cols = list(flow_throughput_cols) + list(rtt_seconds_cols)
if 'JainIndex' in df.columns:
    agg_cols.append('JainIndex')

# group by Protocol and compute mean
summary = df.groupby('Protocol')[agg_cols].mean().reset_index()

# convert averaged RTT seconds back to human-readable strings (use ms)
def format_seconds_as_ms_str(s):
    if pd.isna(s):
        return ""
    ms = float(s) * 1000.0
    # format with up to 3 decimal places, strip trailing zeros
    s_ms = f"{ms:.3f}".rstrip('0').rstrip('.')
    return f"{s_ms}ms"

# build final dataframe with desired column order:
# Protocol, Flow1_Mbps..FlowN_Mbps, Flow1_RTT..FlowN_RTT, JainIndex
out_cols = ['Protocol'] + flow_throughput_cols
# append RTT formatted columns in same flow order
for i, rtt_sec_col in enumerate(rtt_seconds_cols, start=1):
    orig_rtt_col = f"Flow{i}_RTT"
    # if there are mismatches in numbering, try to keep original name
    if orig_rtt_col in df.columns:
        out_rtt_name = orig_rtt_col
    else:
        # fallback name from the parsed column name
        out_rtt_name = rtt_sec_col.replace('_s', '')
    out_cols.append(out_rtt_name)

if 'JainIndex' in summary.columns:
    out_cols.append('JainIndex')

# prepare output rows
out_rows = []
for _, row in summary.iterrows():
    out_row = {}
    out_row['Protocol'] = row['Protocol']
    for c in flow_throughput_cols:
        out_row[c] = row[c]
    # RTT formatting
    for i, rtt_sec_col in enumerate(rtt_seconds_cols, start=1):
        sec_val = row[rtt_sec_col]
        out_rtt_name = f"Flow{i}_RTT"
        out_row[out_rtt_name] = format_seconds_as_ms_str(sec_val)
    if 'JainIndex' in summary.columns:
        out_row['JainIndex'] = row['JainIndex']
    out_rows.append(out_row)

out_df = pd.DataFrame(out_rows, columns=out_cols)

# Save and print
out_df.to_csv(OUTPUT, index=False)
print("Averaged results by protocol saved to:", OUTPUT)
print(out_df.to_string(index=False))