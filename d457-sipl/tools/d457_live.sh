#!/bin/sh
# Live D457 viewer over SSH. Usage:  sudo sh /home/mic-742/d457_live.sh [rgb|ir|depth] [port]
# Streams MJPEG (with FPS + resolution overlay) on :PORT. From your PC:
#   ssh -L 8080:localhost:8080 mic-742@<rig>   then open  http://localhost:8080/
# Ctrl-C here to stop (kills capture, unmounts ramdisk).
VIEW=${1:-rgb}
PORT=${2:-8080}
RUNFOR=${3:-86400}   # seconds to keep capturing (default ~24h; Ctrl-C stops sooner)
DIR=/tmp/live

cleanup() {
    pkill -9 -x nvsipl_camera 2>/dev/null
    [ -n "$SRVPID" ] && kill -9 "$SRVPID" 2>/dev/null
    fuser -k "$PORT"/tcp 2>/dev/null
    umount "$DIR" 2>/dev/null
}
trap cleanup INT TERM EXIT

# Clean slate: stop any prior capture + MJPEG server, and clear stale (possibly stacked) mounts.
pkill -9 -x nvsipl_camera 2>/dev/null
fuser -k "$PORT"/tcp 2>/dev/null      # free the port from a leftover server
j=0; while fuser "$PORT"/tcp >/dev/null 2>&1 && [ $j -lt 12 ]; do sleep 1; j=$((j+1)); done
sleep 1
i=0; while mountpoint -q "$DIR" && [ $i -lt 12 ]; do umount "$DIR" 2>/dev/null; i=$((i+1)); done
rm -rf "$DIR"; mkdir -p "$DIR"

# SAFETY: frames must land on a size-capped tmpfs so a write burst can NEVER fill the
# root disk. Mount it and VERIFY — if the mount fails, ABORT (do not start capture).
mount -t tmpfs -o size=400M tmpfs "$DIR"
if ! mountpoint -q "$DIR"; then
    echo "FATAL: could not mount tmpfs at $DIR — aborting (refusing to write frames to the root disk)." >&2
    exit 1
fi
echo ">> tmpfs mounted at $DIR (400M cap)"

echo ">> power-cycling DS5"
i2cset -y 9 0x28 0x01 0x00; sleep 3; i2cset -y 9 0x28 0x01 0x1f; sleep 2

echo ">> starting capture (stream=$VIEW, runfor=${RUNFOR}s)"
# -r <secs> disables the interactive runtime menu and runs continuously (without it, the
# backgrounded app gets stdin EOF and tears down after a few hundred frames -> frozen, 0 fps).
# -W <large> keeps WRITING every frame (default writeFrames count would stop the file dump early).
env D457_STREAM="$VIEW" \
    nvsipl_camera -H -R -0 -1 -2 -m 0x0001 -c D457_Camera -f "$DIR/f" \
    -r "$RUNFOR" -W 1000000000 >/dev/null 2>&1 &
sleep 4

echo ">> live MJPEG on http://<rig>:$PORT/   (or via:  ssh -L $PORT:localhost:$PORT mic-742@<rig>)"
echo ">> Ctrl-C to stop."
D457_VIEW="$VIEW" D457_PORT="$PORT" D457_DIR="$DIR" python3 /home/mic-742/d457_mjpeg.py &
SRVPID=$!
wait "$SRVPID"
