#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Copyright(c) 2026 RealSense, Inc. All Rights Reserved.
# Install the apps and tools flat into <HSB 2.7 tree>/rs/, the only place the demo container
# (docker/demo.sh) mounts, so the imports between them work as-is.
#
#   sh d555-roce/install.sh                     # on the rig
#   sh d555-roce/install.sh user@jetson       # from a dev box, over ssh/scp
#
# HSB: tree path relative to $HOME (default Projects/holoscan-sensor-bridge).
# SSH_OPTS: extra ssh/scp options, e.g. "-i ~/.ssh/id_ed25519".
set -e
HSB=${HSB:-Projects/holoscan-sensor-bridge}
src=$(dirname "$0")
files="$src/apps/*.py $src/tools/*.py $src/tools/*.sh"

if [ -n "$1" ]; then
    ssh $SSH_OPTS "$1" "test -d $HSB || { echo 'no HSB tree at ~/$HSB' >&2; exit 1; }; mkdir -p $HSB/rs"
    scp -q $SSH_OPTS $files "$1:$HSB/rs/"
    echo "installed into $1:~/$HSB/rs"
else
    test -d "$HOME/$HSB" || { echo "no HSB tree at ~/$HSB" >&2; exit 1; }
    mkdir -p "$HOME/$HSB/rs"
    cp $files "$HOME/$HSB/rs/"
    echo "installed into ~/$HSB/rs"
fi
