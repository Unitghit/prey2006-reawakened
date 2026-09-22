# Overlapping direct portal views

portalgeo shows a brown strip around the crosshair inside the nearby portal.
The original command order renders source-room portal entity 1945 first,
including its legitimate nested portal 151, then distant source-room portal
1940. The distant sibling overwrites the near portal's color buffer. The
parent aperture depth pass preserves that incorrect color.

R_GenerateSubViews now collects direct subviews into a separate stable list,
ordered from farthest to nearest by view-space surface center depth. Material
surface ordering is unchanged. Texture subviews still render first, followed
by skybox backgrounds, then the ordered direct views. Each recursive parent
orders its own children independently.

Muted captures under validation/portalgeo show the brown strip removed with
1940 drawn before 1945, while nested portal 151 remains enabled. The
threeportals save is used to check multiple visible nested portals.

This is painter ordering for distinct portal surfaces, not a per-pixel
solution for physically intersecting portal apertures.
