#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build one self-contained Extended OBD from supplied private runtime assets.

The canonical master world must first pass the C++ World codec/resource checks.
Publication is explicit; development candidates remain playable:false by default.
No proprietary data is included with this tool.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import zipfile


def digest(value):
    return hashlib.sha256(value).hexdigest()


def finalize(source, world_path, destination, playable=False, expected_source=None):
    source = Path(source)
    if source.is_dir():
        source /= 'COMMUNITY.OBD'
    destination = Path(destination)
    if destination.exists():
        raise ValueError('Output already exists')
    source_bytes = source.read_bytes()
    source_hash = digest(source_bytes)
    if expected_source and source_hash != expected_source.lower():
        raise ValueError('Input bundle checksum changed')
    world_bytes = Path(world_path).read_bytes()
    world = json.loads(world_bytes)
    if (world.get('format') != 'openbmh-world-12' or world.get('profile') != 'dual-classic-2'
            or type(world.get('version')) is not int or world['version'] < 42):
        raise ValueError('Canonical Extended World schema is required')
    if world.get('managers') or world.get('loans') or world.get('players_initialized'):
        raise ValueError('Use a master dataset, not an initialized career')
    entries = {}
    with zipfile.ZipFile(source) as archive:
        for info in archive.infolist():
            if info.is_dir():
                continue
            name = info.filename.replace('\\', '/')
            path = PurePosixPath(name)
            if path.is_absolute() or '..' in path.parts or ':' in name or name in entries:
                raise ValueError('Unsafe or duplicate input entry')
            # Runtime data uses indexed Original formats. Reference programs,
            # saved careers and development raster studies stay outside it.
            if path.suffix.lower() in {'.exe', '.dll', '.fbm', '.png', '.jpg', '.jpeg'}:
                continue
            entries[name] = archive.read(info)
    metadata = json.loads(entries.pop('PACKAGE.json'))
    if (metadata.get('format') != 'openbmh-community-12'
            or metadata.get('profile') != 'dual-classic-2'
            or metadata.get('requires') != 'runtime:dual-classic-2'):
        raise ValueError('Input runtime/profile metadata differs')
    entries['WORLD12.JSON'] = world_bytes
    for required in ('RULES-EN.DAT', 'KB/SETUP12.VGA', 'REFEREES.JSON'):
        if required not in entries:
            raise ValueError('Required runtime resource is missing: ' + required)
    metadata.update(playable=bool(playable), world_schema=world['version'],
                    source_bundle_sha256=source_hash,
                    files={name: digest(data) for name, data in sorted(entries.items())})
    entries['PACKAGE.json'] = (json.dumps(metadata, indent=2, sort_keys=True) + '\n').encode()
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_name(destination.name + '.partial')
    try:
        with temporary.open('xb') as output:
            with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED,
                                 compresslevel=9) as archive:
                for name, data in sorted(entries.items()):
                    info = zipfile.ZipInfo(name, (2026, 1, 1, 0, 0, 0))
                    info.compress_type = zipfile.ZIP_DEFLATED
                    archive.writestr(info, data, compress_type=zipfile.ZIP_DEFLATED,
                                     compresslevel=9)
        with zipfile.ZipFile(temporary) as archive:
            if set(archive.namelist()) != set(entries):
                raise ValueError('Output entry set differs')
            for name, data in entries.items():
                if archive.read(name) != data:
                    raise ValueError('Output verification differs: ' + name)
        os.link(temporary, destination)
    finally:
        temporary.unlink(missing_ok=True)
    return {'file': str(destination), 'playable': bool(playable),
            'world_schema': world['version'], 'entries': len(entries),
            'bytes': destination.stat().st_size,
            'sha256': digest(destination.read_bytes()), 'source_sha256': source_hash}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--world', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--source-sha256')
    parser.add_argument('--playable', action='store_true')
    args = parser.parse_args()
    print(json.dumps(finalize(args.source, args.world, args.out, args.playable,
                              args.source_sha256), indent=2))


if __name__ == '__main__':
    main()
