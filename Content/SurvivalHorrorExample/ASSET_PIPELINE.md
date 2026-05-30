# Asset Import Pipeline

The asset pipeline is intentionally simple and replaceable:

1. Put reusable source assets in `ExternalAssets`.
2. Add sidecar license or credit notes near the assets when possible.
3. Run `python Tools\asset_import_pipeline.py`.
4. Review `Assets/import_report.md`.
5. Load `Assets/import_manifest.tsv` from your game layer or mirror the sample in
   `ImportedAssetSceneSample.cpp`.

Supported groups:

- Textures: common image formats plus `.tim`
- Models: Wicked scenes, OBJ, FBX, glTF/GLB, DAE, and common mesh formats
- Audio: WAV, OGG, MP3, FLAC, tracker modules, and common retro audio extensions
- Animations: `.anim`, `.bvh`, `.md5anim`, and FBX/glTF files with animation-like paths

The importer copies by default. Pass `--move` if you want to clear staging files
after import, or `--dry-run` to preview without writing.
