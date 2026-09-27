"""Source-only checks for GUI holster generation. No retail assets required."""
import re
import unittest
from import_shotgun import gui_lowering_animations, fix_gui_transitions

SOURCE = """MD5Version 10
numFrames 3
numJoints 1
frameRate 24
numAnimatedComponents 1
hierarchy { "origin" -1 1 0 }
bounds {
( 0 0 0 ) ( 1 1 1 )
( 0 0 -1 ) ( 1 1 0 )
( 0 0 -2 ) ( 1 1 -1 )
}
baseframe { ( 0 0 0 ) ( 0 0 0 ) }
frame 0 { 0 }
frame 1 { -1 }
frame 2 { -2 }
"""

class GuiLoweringTests(unittest.TestCase):
    def test_endpoints_and_reverse(self):
        lower, held, raised = gui_lowering_animations(SOURCE)
        self.assertEqual(lower, SOURCE)
        def frames(s):
            return [float(v) for v in re.findall(r'frame\s+\d+\s*\{(.*?)\}', s, re.S)]
        self.assertEqual(frames(held), [-2, -2])
        self.assertEqual(frames(raised), [-2, -1, 0])
        self.assertIn('numFrames 2', held)
        self.assertIn('hierarchy { "origin" -1 1 0 }', held)
        self.assertEqual(held.count('( 0 0 -2 ) ( 1 1 -1 )'), 2)
        self.assertLess(raised.index('( 0 0 -2 )'), raised.index('( 0 0 0 )'))

    def test_weapon_aliases(self):
        for weapon in ('shotgun', 'machinegun', 'supershotgun'):
            name = f'def/doom3_{weapon}_models.def'
            files = {name: b'model view { anim down assets/lower.md5anim\nanim put_aside old\nanim aside old\nanim upright old\n}',
                     'assets/lower.md5anim': SOURCE.encode()}
            fix_gui_transitions(files)
            for alias, suffix in [('put_aside','lower'), ('aside','held'), ('upright','raise')]:
                self.assertIn(f'anim {alias} assets/gui_{suffix}.md5anim'.encode(), files[name])
                self.assertIn(f'assets/gui_{suffix}.md5anim', files)
            self.assertIn(b'anim down assets/lower.md5anim', files[name])

if __name__ == '__main__':
    unittest.main()
