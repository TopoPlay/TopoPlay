# TopoPlay assets

This page identifies the images, sounds, and fonts used by TopoPlay. Component
and media notices are separate from the application's source license. Preserve
the relevant credits, license texts, and permission records when distributing
assets; there is no blanket public-domain or shared-license claim for all media.

| Runtime paths relative to bin/data | Origin / notice |
| --- | --- |
| `rain/thunder.wav` | Friendly Free Sounds, “Thunder Sound Effect”; creator credit and supplied permission record. Exact WAV hash and supplied permission record in `docs/assets/THUNDER-PERMISSION.md`. |
| `volcano/warning.wav`, `volcano/eruption.wav` | Mathematical synthesis without imported samples; recipe `docs/assets/synthesize-sounds.py`. |
| `volcano/lava.jpg`, `volcano/whitesmoke.png` | Generated artwork; smoke is also used for clouds. |
| `dig/dinotrex.png`, `dig/closedchest.png` | Generated fossil and chest artwork. |
| `demo/rain-hand.png`, `demo/mounding-hands.png` | Generated demonstration artwork. |
| `textures/sandygrass.jpg`, `textures/grass.jpg`, `textures/spottygrass.jpg`, `textures/rock_diff.jpg`, `textures/mountainside.jpeg`, `textures/snow_diff.jpg` | Generated terrain images; some slots intentionally share an image. Names and hashes are listed in the asset records. |
| `vegetation/pine-v1.png` | Generated pine artwork sized for runtime.  |
| `images/rain-pool-caustics-v28.png` | Generated for pooling-water rendering; animated texture coordinates avoid rotating the pool into a whirlpool.  |
| Legacy UI font filenames | Unmodified DejaVu Sans, Sans Bold, Serif compatibility aliases. Exact mapping in `docs/assets/font-aliases.json`; license in `bin/data/licenses/DejaVu-fonts-copyright.txt`. |
## GUI artwork

The five files under `bin/data/ofxbraitsch/ofxdatgui/` match the corresponding
ofxDatGui upstream files byte for byte. Stephen Braitsch's MIT notice is included
in `licenses/ofxDatGui-MIT.txt`. Exact paths, hashes and the checked upstream
commit are recorded in `docs/assets/gui-image-provenance.json`.

## Asset records

File hashes are listed in `docs/assets/KNOWN-ASSET-HASHES.json` and the package
manifest. Generation prompts, export records, and font mappings are under
`docs/assets/`. The core artwork manifest does not include the pine/pool prompts;
those assets have separate development records. New shaders, procedural animals,
buttons, flames, and effects are rendered by source code.

Optional legacy Island/Map and competitive Sandimals media is not bundled.
See [runtime resources](bin/data/README.md) for the available feature scope.

## Fossil discoveries

- `bin/data/dig/fossils/triceratops.png`, `pteranodon.png` and
  `fossil-cluster.png`: generated with OpenAI image generation for this project.
  Subject/prompt notes and SHA-256 hashes are recorded in
  `docs/assets/fossil-variety-provenance.json`.
- `bin/data/dig/fossils/topoplay-logo.png`: transparent-background edit of the
  existing generated TopoPlay logo. Background-extraction prompt, source
  description and SHA-256 hash are in `docs/assets/topoplay-logo-fossil.json`.

These are stylized educational game illustrations, not anatomical reference
reconstructions. Pteranodon is a pterosaur. Keep these provenance records with
redistributed assets; this record does not change other media permissions.

## Launcher icon

`bin/data/icons/topoplay.png` is an exact copy of the existing generated
TopoPlay GitHub logo, used for the app menu and desktop shortcut. It is separate
from the transparent buried-logo sprite. Source description and SHA-256 hash
are in `docs/assets/topoplay-icon.json`.
