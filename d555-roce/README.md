# D555e over RoCEv2

Host side of D555 over RoCE. The D555e firmware writes frames as RoCEv2 RDMA WRITEs; these apps
receive them with the stock Holoscan Sensor Bridge 2.7 `RoceReceiverOp` (no HSB fork code).

Needs D555e firmware with RoCE support. The camera runs one firmware for both paths and picks
RoCE per stream when the host configures it, otherwise CoE (`d555-sipl/`). Tested on an IGX Thor
(IGX-SW 2.0) with a ConnectX-7 and stock HSB 2.7.0 — not the HSB 2.5.0 vendored in this repository.

| File | Purpose |
|---|---|
| `apps/d555_profiles.py` | Stream profile table (the firmware's indices), shared by apps and scripts |
| `apps/roce_d555_single.py` | One stream, any profile; `--gpu <uuid>` (GPUDirect), `--timing-csv`, `--stop-only` |
| `apps/roce_d555_dual.py` | Depth + RGB together |
| `tools/roce_stability.sh` | Soak, start/stop cycles, dual runs, cross-checked against the CX-7 RDMA counter |
| `tools/roce_latency.sh`, `roce_latency.py` | Latency breakdown from a CX-7 RDMA-sniffer capture |
| `tools/roce_matrix.sh` | Every profile with the latency capture |
| `tools/roce_pcap_check.py` | Offline packet validator (ICRC, PSN, VA, ImmDt, metadata) |
| `tools/roce_pcap.py`, `roce_common.sh` | Shared pcap parser / script settings |
| `tools/hkr_uart.py`, `hkr_uart_logger.py` | Camera CLI and log over the UART |
| `install.sh` | Installs apps + tools into the rig's HSB tree (`rs/`) |

## Running (IGX Thor + ConnectX-7)

Install into `rs/` under the HSB 2.7 tree (the only place the demo container mounts), then run
from the tree root over `ssh -tt` (the demo container runs with `docker -it`):

```sh
sh d555-roce/install.sh user@jetson   # from a dev box (or with no argument, on the rig)
sudo sh docker/demo.sh sh -c "python3 rs/roce_d555_single.py --stream rgb --ibv-name roceP4p3s0f1"
sh rs/roce_stability.sh soak dual 900
GPU=GPU-<dGPU uuid> EXTRA="--stats-every 0" sh rs/roce_stability.sh dualcycle 20 60
sh rs/roce_matrix.sh all 10
```

Keep the camera's UART log at `log err` while measuring (`log dbg` throttles streaming), and use
`--stats-every 0` for dGPU benchmarks (CuPy stats stall ~130 ms there).
