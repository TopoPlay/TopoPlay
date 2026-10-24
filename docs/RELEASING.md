# Source release checklist

1. Retain the operator's working installation and private final backup.
2. Run `python3 scripts/verify-package.py` in the current source repository.
3. Confirm source notices, GPL text, dependency MIT/Boost notices, media provenance,
   DejaVu license and generated thunder recipe are included.
4. Build/test if possible on the original hardware; label untested changes honestly.
   The new thunder requires an audible hardware check. Source notices do not alter
   simulation/rendering logic or saved settings.
5. Commit, push and create a source archive from that exact commit. Example:

```bash
git archive --format=zip --prefix=TopoPlay/ \
  --output="$HOME/Desktop/TopoPlay-1.0.1-source.zip" HEAD
```

6. Attach only that current archive when publishing a source release; identify it
   as source requiring separately installed dependencies. See PUBLISHING.md.

No compiled application or shared-library bundle is cleared by this source review.
Binary distribution remains a separate license/dependency and clean-system test.
Earlier Git history still contains the previous recording; see the sound record.
