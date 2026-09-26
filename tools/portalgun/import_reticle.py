"""Convert the locally installed Portal HUD atlas for Prey's portal gun.
Usage: python import_reticle.py PORTAL_INSTALL OUTPUT_BASE
Generated retail-derived images are local-only, not distributable source assets.
"""
from pathlib import Path
import sys
from source_assets import VPK
from import_viewmodel import vtf_image

def import_reticle(install, output):
    vpk = VPK(Path(install) / "portal/portal_pak_dir.vpk")
    output = Path(output)
    texture = output / "guis/assets/portalgun/reticle.tga"
    texture.parent.mkdir(parents=True, exist_ok=True)
    vtf_image(vpk.read("materials/sprites/hud/portal_crosshairs.vtf")).save(texture)
    materials = output / "materials"
    materials.mkdir(parents=True, exist_ok=True)
    (materials / "portalgun_reticle.mtr").write_text("""guis/assets/portalgun/reticle
{
    noShadows
    noSelfShadow
    {
        blend blend
        map guis/assets/portalgun/reticle.tga
        clamp
        colored
    }
}
""")

if __name__ == "__main__":
    import_reticle(sys.argv[1], sys.argv[2])
