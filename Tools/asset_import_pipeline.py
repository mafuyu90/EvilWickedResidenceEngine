#!/usr/bin/env python3
"""
Survival horror asset import pipeline for Wicked Engine projects.

The importer scans ExternalAssets, copies recognized files into Assets by type,
and writes both a machine-readable manifest and a human-readable credit report.
It intentionally does not extract commercial archives by default; put only assets
you are allowed to reuse into ExternalAssets before running it.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import os
import re
import shutil
from dataclasses import dataclass
from pathlib import Path


TEXTURE_EXTENSIONS = {
    ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds", ".hdr", ".ktx", ".ktx2",
    ".webp", ".gif", ".tif", ".tiff", ".tim",
}
MODEL_EXTENSIONS = {
    ".wiscene", ".obj", ".fbx", ".glb", ".gltf", ".dae", ".3ds", ".blend",
    ".ply", ".stl", ".pmx", ".pmd", ".md5mesh",
}
AUDIO_EXTENSIONS = {
    ".wav", ".ogg", ".mp3", ".flac", ".aiff", ".aif", ".opus", ".m4a",
    ".mid", ".midi", ".xm", ".mod", ".s3m", ".it", ".vag", ".xa",
}
ANIMATION_EXTENSIONS = {
    ".anim", ".bvh", ".md5anim",
}

MANIFEST_NAME = "import_manifest.tsv"
REPORT_NAME = "import_report.md"


@dataclass
class ImportedAsset:
    kind: str
    original: Path
    destination: Path
    license_notes: str
    sha256: str
    size_bytes: int


def project_root_from_script() -> Path:
    return Path(__file__).resolve().parents[1]


def safe_part(value: str) -> str:
    value = re.sub(r"[<>:\"/\\|?*\x00-\x1f]", "_", value)
    value = value.strip().strip(".")
    return value or "_"


def safe_relative_path(path: Path, root: Path) -> Path:
    relative = path.relative_to(root)
    return Path(*[safe_part(part) for part in relative.parts])


def classify(path: Path) -> str | None:
    suffix = path.suffix.lower()
    lowered = path.as_posix().lower()
    if suffix in ANIMATION_EXTENSIONS:
        return "Animations"
    if suffix in MODEL_EXTENSIONS:
        if suffix in {".fbx", ".glb", ".gltf"} and any(token in lowered for token in ("anim", "motion", "pose")):
            return "Animations"
        return "Models"
    if suffix in TEXTURE_EXTENSIONS:
        return "Textures"
    if suffix in AUDIO_EXTENSIONS:
        return "Audio"
    return None


def read_license_notes(path: Path, source_root: Path) -> str:
    candidates = [
        path.with_suffix(path.suffix + ".license.txt"),
        path.with_suffix(".license.txt"),
        path.parent / "LICENSE.txt",
        path.parent / "LICENSE.md",
        path.parent / "CREDITS.txt",
        path.parent / "CREDITS.md",
        source_root / "LICENSE.txt",
        source_root / "LICENSE.md",
        source_root / "CREDITS.txt",
        source_root / "CREDITS.md",
    ]
    for candidate in candidates:
        if candidate.exists() and candidate.is_file():
            text = candidate.read_text(encoding="utf-8", errors="replace").strip()
            if text:
                return " ".join(text.split())
    return "TODO: add license/credit note before shipping."


def hash_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def unique_destination(path: Path) -> Path:
    if not path.exists():
        return path
    stem = path.stem
    suffix = path.suffix
    parent = path.parent
    index = 2
    while True:
        candidate = parent / f"{stem}_{index}{suffix}"
        if not candidate.exists():
            return candidate
        index += 1


def iter_source_files(source_root: Path):
    for path in sorted(source_root.rglob("*")):
        if not path.is_file():
            continue
        if any(part.startswith(".") for part in path.relative_to(source_root).parts):
            continue
        yield path


def import_assets(source_root: Path, asset_root: Path, move: bool, dry_run: bool) -> list[ImportedAsset]:
    imported: list[ImportedAsset] = []
    for source in iter_source_files(source_root):
        kind = classify(source)
        if kind is None:
            continue

        relative = safe_relative_path(source, source_root)
        destination = unique_destination(asset_root / kind / relative)
        notes = read_license_notes(source, source_root)

        if not dry_run:
            destination.parent.mkdir(parents=True, exist_ok=True)
            if move:
                shutil.move(str(source), str(destination))
            else:
                shutil.copy2(source, destination)

        final_path = destination if not dry_run else source
        imported.append(
            ImportedAsset(
                kind=kind,
                original=source,
                destination=destination,
                license_notes=notes,
                sha256=hash_file(final_path),
                size_bytes=final_path.stat().st_size,
            )
        )
    return imported


def write_manifest(asset_root: Path, imported: list[ImportedAsset], project_root: Path) -> None:
    manifest_path = asset_root / MANIFEST_NAME
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    with manifest_path.open("w", encoding="utf-8", newline="") as file:
        writer = csv.writer(file, delimiter="\t", lineterminator="\n")
        writer.writerow(["kind", "original", "destination", "license_notes", "sha256", "size_bytes"])
        for asset in imported:
            writer.writerow([
                asset.kind,
                asset.original.resolve(),
                asset.destination.relative_to(project_root).as_posix(),
                asset.license_notes,
                asset.sha256,
                asset.size_bytes,
            ])


def write_report(asset_root: Path, imported: list[ImportedAsset], project_root: Path) -> None:
    report_path = asset_root / REPORT_NAME
    report_path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "# Imported Asset Report",
        "",
        "Generated by `Tools/asset_import_pipeline.py`.",
        "",
        "| Type | Original filename | Destination path | License / credit notes |",
        "| --- | --- | --- | --- |",
    ]
    for asset in imported:
        destination = asset.destination.relative_to(project_root).as_posix()
        original_name = asset.original.name.replace("|", "\\|")
        notes = asset.license_notes.replace("|", "\\|")
        lines.append(f"| {asset.kind} | {original_name} | `{destination}` | {notes} |")
    if not imported:
        lines.append("| None | No supported files were found. | | |")
    lines.append("")
    report_path.write_text("\n".join(lines), encoding="utf-8")


def ensure_roots(source_root: Path, asset_root: Path) -> None:
    source_root.mkdir(parents=True, exist_ok=True)
    for folder in ("Textures", "Models", "Audio", "Animations"):
        (asset_root / folder).mkdir(parents=True, exist_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="Import reusable retro assets into Wicked Engine asset folders.")
    parser.add_argument("--root", type=Path, default=project_root_from_script(), help="Project root. Defaults to this repository.")
    parser.add_argument("--source", type=Path, default=None, help="Source folder. Defaults to <root>/ExternalAssets.")
    parser.add_argument("--dest", type=Path, default=None, help="Destination asset folder. Defaults to <root>/Assets.")
    parser.add_argument("--move", action="store_true", help="Move assets instead of copying them.")
    parser.add_argument("--dry-run", action="store_true", help="Scan and report without copying or moving files.")
    args = parser.parse_args()

    project_root = args.root.resolve()
    source_root = (args.source or project_root / "ExternalAssets").resolve()
    asset_root = (args.dest or project_root / "Assets").resolve()

    ensure_roots(source_root, asset_root)
    imported = import_assets(source_root, asset_root, move=args.move, dry_run=args.dry_run)

    if not args.dry_run:
        write_manifest(asset_root, imported, project_root)
        write_report(asset_root, imported, project_root)

    print(f"Scanned: {source_root}")
    print(f"Imported: {len(imported)} asset(s)")
    print(f"Manifest: {asset_root / MANIFEST_NAME}")
    print(f"Report: {asset_root / REPORT_NAME}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
