# Snapshot assembly

The source baseline is the reviewed/cleaned v38 source candidate. The four v44
payload files supersede its renderer header/implementation and GL2/GL3 height
fragment shaders, incorporating the subsequent grass/rock and snow work.
The tested v45 installer inserts attribution comments, restores the original
Friendly Free Sounds WAV, and supplies updated docs. No source logic was edited
in this packaging step. No working-machine calibration or box profile is included.

The core v39 generated assets and DejaVu aliases are included, together with the
previously generated pine and pool texture from the exact runtime review ZIPs.
Both source and media appear in PACKAGE-SHA256.json. If the laptop source was
edited outside these supplied updates, compare before releasing; the package is
a reproducible assembly of supplied snapshots, not a live read of that machine.
