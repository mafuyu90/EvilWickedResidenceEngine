# External Assets Staging

Place reusable third-party or personally licensed source assets in this folder,
then run:

```powershell
python Tools\asset_import_pipeline.py
```

The importer copies supported files into `Assets/Textures`, `Assets/Models`,
`Assets/Audio`, and `Assets/Animations`, then writes:

- `Assets/import_manifest.tsv`
- `Assets/import_report.md`

License and credit notes are read from nearby sidecar files when present:

- `asset.ext.license.txt`
- `asset.license.txt`
- `LICENSE.txt` / `LICENSE.md`
- `CREDITS.txt` / `CREDITS.md`

Do not commit commercial archives or raw dumps here unless your project license
allows redistribution.
