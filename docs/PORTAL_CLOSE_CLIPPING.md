# Close portal clipping

portalblackclip places the eye 0.0802 units in front of a portal plane.
R_PreciseCullSurface clipped triangles against the side frustum with a
0.1-world-unit tolerance. Near this plane that tolerance retains off-frustum
vertices and can produce an undersized projected screen rectangle. The
destination view then leaves a black strip, changing with the viewing angle.

Direct portal bounds now use zero clipping tolerance. Mirrors and other
subviews retain their existing tolerance. Aperture geometry and depth masks
are unchanged. The temporary full-screen bounds diagnostic was removed.

Validation: validation/portal-black. Baseline reproduces the black left strip.
Disabling GPU scissor and lowering zNear did not fix it; conservative full-screen
bounds did. The precise correction removes it without expanding the aperture.
Muted tests cover the save, three nearby view angles, nested portals, energy
portal side views, and tall portal side views. Production zNear and scissor
settings remain unchanged.
