"""ExportReferences.py's rows: reference_data.h is what --rows writes from references.json, and every round in
ammo-rounds.json is a projectile row with its packages.

Usage: py -m unittest discover -s scripts -p test_export_references.py
"""
import json
import re
import unittest
from pathlib import Path

import ExportReferences

REPO = Path(__file__).resolve().parent.parent
HEADER = REPO / 'native' / 'mod-loader' / 'reference_data.h'


class Rows(unittest.TestCase):
    def setUp(self):
        self.data = json.loads((REPO / 'mappings' / 'references.json').read_text(encoding='utf-8'))
        self.header = HEADER.read_text(encoding='utf-8')

    def test_header_is_the_rows(self):
        rows = self.header[self.header.index('typedef struct RefPackage'):]
        self.assertEqual(rows, '\n'.join(ExportReferences.header_rows(self.data)) + '\n')

    def test_ammo_rounds_are_rows(self):
        extra = json.loads((REPO / 'mappings' / 'ammo-rounds.json').read_text(encoding='utf-8'))
        ids = sorted(self.data['packages'])
        table = self.header.split('kRefProjectiles[] = {')[1].split('};')[0]
        rows = {int(t): (int(a), int(b)) for t, a, b in re.findall(r'\{(\d+)u, (\d+), (\d+), 0, 0\}', table)}
        for r in extra['projectiles']:
            self.assertIn(r['type'], rows)
            want = [ids.index(p) + 1 for p in r['packages']] + [0]
            self.assertEqual(rows[r['type']], (want[0], want[1]), r['label'])
        left = {r['type'] for r in extra['left_out']}
        self.assertTrue(left.isdisjoint(rows), 'a left-out round has a row')

    def test_merge_is_idempotent(self):
        before = json.dumps(self.data, sort_keys=True)
        ExportReferences.merge_ammo_rounds(self.data, REPO / 'mappings')
        self.assertEqual(json.dumps(self.data, sort_keys=True), before)


if __name__ == '__main__':
    unittest.main()
