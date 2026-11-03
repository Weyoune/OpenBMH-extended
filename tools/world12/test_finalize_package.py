# SPDX-License-Identifier: GPL-3.0-only
import json
from pathlib import Path
import tempfile
import unittest
import zipfile
from finalize_package import finalize, digest


class FinalizePackageTests(unittest.TestCase):
    def test_preserves_runtime_bytes_updates_world_and_hashes_and_omits_references(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            source, world, output = root/'input.obd', root/'world.json', root/'output.obd'
            with zipfile.ZipFile(source, 'w') as archive:
                for name, data in {'PACKAGE.json': json.dumps({'format': 'openbmh-community-12', 'profile': 'dual-classic-2', 'requires': 'runtime:dual-classic-2', 'playable': False}).encode(), 'WORLD12.JSON': b'old', 'RULES-EN.DAT': b'authored-rules', 'KB/SETUP12.VGA': b'authored-asset', 'REFEREES.JSON': b'{}', 'REFERENCE.EXE': b'omit', 'USER.FBM': b'omit'}.items():
                    archive.writestr(name, data)
            world.write_text(json.dumps({'format': 'openbmh-world-12', 'profile': 'dual-classic-2', 'version': 42, 'managers': [], 'loans': [], 'players_initialized': False}))
            provenance = json.dumps({'new_players': [{'id': 5000, 'fictional_fields': ['name']}]}).encode()
            (root/'IMPORT-REPORT.json').write_bytes(provenance)
            result = finalize(source, world, output)
            self.assertFalse(result['playable'])
            with zipfile.ZipFile(output) as archive:
                self.assertEqual(archive.read('WORLD12.JSON'), world.read_bytes())
                self.assertEqual(archive.read('KB/SETUP12.VGA'), b'authored-asset')
                self.assertEqual(archive.read('IMPORT-REPORT.json'), provenance)
                self.assertNotIn('REFERENCE.EXE', archive.namelist())
                self.assertNotIn('USER.FBM', archive.namelist())
                metadata = json.loads(archive.read('PACKAGE.json'))
                self.assertEqual(metadata['files']['WORLD12.JSON'], digest(world.read_bytes()))
                self.assertEqual(metadata['files']['IMPORT-REPORT.json'], digest(provenance))
            with self.assertRaisesRegex(ValueError, 'already exists'):
                finalize(source, world, output)
            with self.assertRaisesRegex(ValueError, 'checksum'):
                finalize(source, world, root/'wrong.obd', expected_source='0'*64)
            data = json.loads(world.read_text());data['managers'] = [{}];world.write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, 'master dataset'):
                finalize(source, world, root/'career.obd')


if __name__ == '__main__':
    unittest.main()
