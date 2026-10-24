# Historical source snapshot assembly

This describes the earlier source-export assembly, before the TopoPlay public
release. The current repository and package checksum manifest identify the
files distributed now.

The source baseline is the reviewed and cleaned source candidate. Four later renderer
payload files supersede its renderer header/implementation and GL2/GL3 height
fragment shaders, incorporating the subsequent grass/rock and snow work.
The tested attribution-and-asset installer inserts attribution comments, restores the original
Friendly Free Sounds WAV, and supplies updated docs. No source logic was edited
in this packaging step. No working-machine calibration or box profile is included.

The core generated assets and DejaVu aliases are included, together with the
previously generated pine and pool texture from the exact runtime review ZIPs.
Both source and media appear in PACKAGE-SHA256.json. If the laptop source was
edited outside these supplied updates, compare before releasing; the package is
a reproducible assembly of supplied snapshots, not a live read of that machine.
