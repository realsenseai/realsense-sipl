#!/bin/bash
set -euo pipefail

# Hololink board IP:
HL=192.168.0.2

# Registers to read
addrs=(
  0x02000304  # dp_pkt_len
  0x02000308  # dp_pkt_host_udp_port
  0x0200030C  # dp_pkt_vip_mask
  0x00001020  # dp_pkt_mac_addr_lo
  0x00001024  # dp_pkt_mac_addr_hi
  0x00001028  # dp_pkt_ip_addr
  0x0000102C  # dp_pkt_fpga_udp_port
  0x00001000  # Destination QP
  0x00001004  # Remote Key
  0x00001008  # Buffer 0 Virtual Address
  0x00001018  # Bytes per Window
  0x0000101C  # Buffer Enable
  0x02000108  # Eth pkt data plane priority
)

echo "Reading factory defaults from Hololink @ $HL"
for addr in "${addrs[@]}"; do
  # discard all INFO logs (stderr) so only the hex stays on stdout
  val=$( hololink --hololink "$HL" read_uint32 "$addr" 2>/dev/null )
  printf "%s = %s\n" "$addr" "$val"
done
