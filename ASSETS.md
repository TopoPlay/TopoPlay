# TopoPlay asset provenance


This record supersedes the earlier source-only asset review for the following
replacement files. The reviewed/generated core media, credited original thunder WAV and font license text
are bundled in this GitHub preparation package.
Hashes identify the prepared replacement assets, not an assertion that every file
in a locally edited installation still matches. See `docs/assets/KNOWN-ASSET-HASHES.json` and the complete package manifest.

| Runtime paths relative to bin/data | Origin / notice |
| --- | --- |
| `rain/thunder.wav` | Friendly Free Sounds, “Thunder Sound Effect”; operator-confirmed project use with channel credit. Exact WAV hash and supplied permission record in `docs/assets/THUNDER-PERMISSION.md`. |
| `volcano/warning.wav`, `volcano/eruption.wav` | Mathematical synthesis without imported samples; recipe `docs/assets/synthesize-sounds.py`. |
| `volcano/lava.jpg`, `volcano/whitesmoke.png` | Generated artwork; smoke is also used for clouds. |
| `dig/dinotrex.png`, `dig/closedchest.png` | Generated fossil and chest artwork. |
| `demo/rain-hand.png`, `demo/mounding-hands.png` | Generated demonstration artwork; replaces the earlier operator-supplied hand images. |
| `textures/sandygrass.jpg`, `textures/grass.jpg`, `textures/spottygrass.jpg`, `textures/rock_diff.jpg`, `textures/mountainside.jpeg`, `textures/snow_diff.jpg` | Generated terrain images; some slots intentionally share an image. Full names and hashes in the recorded export manifest. |
| `vegetation/pine-v1.png` | Generated during pine-tree development and sized/exported for runtime. Created separately from the core artwork batch. |
| `images/rain-pool-caustics-v28.png` | Generated for pooling-water rendering; animated texture coordinates avoid rotating the pool into a whirlpool. Created separately from the core artwork batch. |
| Legacy UI font filenames | Unmodified DejaVu Sans, Sans Bold, Serif compatibility aliases. Exact mapping in `docs/assets/font-aliases.json`; license in `bin/data/licenses/DejaVu-fonts-copyright.txt`. |

The core artwork prompt/provenance JSON records original text-to-image generation; old
assets informed functional requirements but were not provided as pixel references.
Earlier pine/pool assets have conversation creation history; no claim is made
that their original prompts are in the core artwork JSON. New shaders and procedural fish,
sharks, rays, bone/shovel controls, flames and effects are source-rendered features;
the original source notices and project license remain applicable.

## GUI artwork

The five files under `bin/data/ofxbraitsch/ofxdatgui/` match the corresponding
ofxDatGui upstream files byte for byte. Stephen Braitsch's MIT notice is included
in `licenses/ofxDatGui-MIT.txt`. Exact paths, hashes and the checked upstream
commit are recorded in `docs/assets/gui-image-provenance.json`.

## Source snapshot boundaries

Old map-game island/splash resources are not included in this source snapshot.
This does not license other old files kept in local installations or backups.
The DejaVu replacement does not license original font binaries in old backups.
Keep calibration, box-startup settings, performance captures and backups private.

The GPL source license and media permission records serve different purposes.
Keep original author/dependency/font notices; do not label all assets as public
domain or as sharing a blanket media license. This update records
known origins and permissions and does not invent a blanket license for media.

## Additional fossil discoveries — 2026-10-24

- `bin/data/dig/fossils/triceratops.png`, `pteranodon.png` and
  `fossil-cluster.png`: generated with OpenAI image generation for this project.
  Subject/prompt notes and SHA-256 hashes are recorded in
  `docs/assets/fossil-variety-provenance.json`.
- `bin/data/dig/fossils/topoplay-logo.png`: transparent-background edit of the
  existing generated TopoPlay logo. Background-extraction prompt, source
  description and SHA-256 hash are in `docs/assets/topoplay-logo-fossil.json`.

The existing `dig/dinotrex.png` and original GitHub logo are not replaced.
These are stylized educational game illustrations, not anatomical reference
reconstructions. Pteranodon is a pterosaur. Keep these provenance records with
redistributed assets; this record does not change other media permissions.

## Launcher icon — 2026-10-24

`bin/data/icons/topoplay.png` is an exact copy of the existing generated
TopoPlay GitHub logo, used for the app menu and desktop shortcut. It is separate
from the transparent buried-logo sprite. Source description and SHA-256 hash
are in `docs/assets/topoplay-icon.json`. No existing image is replaced.
