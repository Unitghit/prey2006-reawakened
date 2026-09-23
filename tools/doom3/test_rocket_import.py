"""Importer regression without retail assets: Doom declaration names ignore case."""
import struct
import unittest
from import_rocket_fx import import_rocket_fx
from import_shotgun import block


class RocketParticleImportTests(unittest.TestCase):
    def test_lowercase_retail_explosion_name_is_registered_as_particle(self):
        source = '''particle rockettrail {
 { count 1 material textures/test/smoke time 1 }
}
particle rocketexplosion {
 { count 1 material textures/test/fire time 1 }
}
'''
        files, shaders = {}, set()
        import_rocket_fx({'test.prt': None},
                        lambda _: b'FORM'+struct.pack('>I', 4)+b'LWO2',
                        lambda _: source, block, files, shaders)
        particles = files['particles/doom3_rocket.prt'].decode()
        self.assertIn('particle d3_rockettrail {', particles)
        self.assertIn('particle d3_rocketExplosion {', particles)
        self.assertNotIn('table ', particles)
        self.assertIn('material doom3/textures/test/fire', particles)
        self.assertIn('d3_rocketExplosion.prt', files['fx/doom3_rocket.fx'].decode())


if __name__ == '__main__':
    unittest.main()
