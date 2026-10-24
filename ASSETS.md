# TopoPlay asset provenance


This record supersedes the earlier source-only asset review for the following
replacement files. The reviewed/generated core media, restored thunder WAV and font license text
are bundled in this GitHub preparation package.
Hashes identify the prepared replacement assets, not an assertion that every file
in a locally edited installation still matches. See `docs/assets/KNOWN-ASSET-HASHES.json` and the complete package manifest.

| Runtime paths relative to bin/data | Origin / notice |
| --- | --- |
| `rain/thunder.wav` | Friendly Free Sounds; credited in README, CREDITS, rain source comment and sidecar. Operator-supplied permission record and exact restored hash in `docs/assets/THUNDER-PERMISSION.md`. |
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

## Remaining legacy resources

Unused or optional old map-game images, reference islands and splash screens
have not acquired a new redistribution license merely because replacement
artwork was created elsewhere. Their individual provenance still needs review,
replacement, or exclusion from a public export. Original font files in backups
are not licensed by the DejaVu replacement. Do not publish personal calibration,
box-startup settings, performance captures or automatic backups.

The GPL source license and media permission records serve different purposes.
Keep original author/dependency/font notices; do not label the thunder or every
asset as public domain, Creative Commons, or AI-generated. This update records
known origins and permissions and does not invent a blanket license for media.
