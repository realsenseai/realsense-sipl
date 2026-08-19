# RealSense fork notice

This directory is a **vendored snapshot** of NVIDIA's Holoscan Sensor Bridge,
modified by RealSense AI. It is checked in as ordinary files rather than as a git
submodule so that this repository is self-contained.

| | |
|---|---|
| Upstream project | https://github.com/nvidia-holoscan/holoscan-sensor-bridge |
| Upstream base | tag `2.5.0` (commit `903f3db376a13ff6a2d4221b388931f87f5632b5`) |
| Snapshot base | `00d3e48f60d6dc854b7af8f2922a59f72d85ed59`, reduced to shared infrastructure only |
| License | Apache-2.0 — see `LICENSE` |

Upstream files that are unmodified carry their original NVIDIA copyright headers.

## Modifications relative to upstream 2.5.0

RealSense changes are almost entirely *additions* — the D457/D555 example players,
their `sipl_config/`, and the accompanying `hololink` operators — plus small edits
to `docker/` and the `examples`/`python`/`src` CMake and `__init__` wiring that
register them.

- `ab20c4f` realsense camera
- `a302ee3` seperate ext inr logic
- `c216788` update IP to 192.168.0.2
- `0765f32` fix coe reciver
- `aa9d07e` fix coe reciver
- `f2309ae` fix coe reciver
- `817ddca` extend csi object
- `646216b` update modes
- `6899dd2` format docs
- `2ae6c17` add lfs config
- `62a86a1` dual stream coe player start
- `dc7abb3` set stream id instead of hard coded 1 to channel in a single stream coe player
- `a4b9619` fix op name in dual stream coe palyer
- `ee0c2fc` rm proxy settings and dos2unix endings
- `3225406` stack size limit for docker in dem.sh to avoid warning
- `cd17734` rm proxy settings from Docker file
- `022e8e7` add fusa coe d555 player python first sketch
- `26b2a0e` configure the fusa coe operator hard coded 640x360 format
- `a23acc5` update docker files
- `aa8c25f` change timeout of aquire buffer from 100 to 500 ms + any resolution + dos2unix over docker files
- `4385b30` add fusa coe d555 dual stream w/o changes from linux coe d555 dual stream player
- `173a649` update dual stream fusa coe d555 player. not tested yet
- `b86f42a` rename operators per stream + print name
- `138ea14` increase timeout to 3 sec, just in case
- `56ee0a5` revert docker changes after upgrading to JP7.1 from SDK manager
- `00d3e48` D457 SIPL: Holoscan capture operator + player + PeopleNet detection (R39.2 API)

The D457 and D555 capture operators, players and sensor models that these commits added are **no
longer in this tree**: they are owned by `d457-sipl/hsb/` and `d555-sipl/hsb/` and grafted on at
build time (see the repository README). What remains here beyond upstream is shared
infrastructure only — docker, core `data_channel`/`csi_formats`, the receiver operators and
`fusa_coe_capture`.

Beyond those commits, two shared operators gained **RAW_16 support**, which upstream 2.5.0 does not
implement: `image_processor` (treat all 16 bits as data) and `packed_format_converter` (a strided
copy, since 16 bpp needs no unpacking). Both are generic pixel-format additions rather than
camera-specific code, and the D555 depth/RGB CoE path does not stream without them.

## Note on binary assets

Upstream tracks `docs/` images with Git LFS. In this snapshot those files are stored
as ordinary binaries and the corresponding `filter=lfs` rules have been removed from
`.gitattributes` (and `.lfsconfig` dropped), so no LFS setup is needed to clone this
repository.
