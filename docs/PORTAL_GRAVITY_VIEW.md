# Portal camera fallback during gravity changes

The visualjolt save reproduces a continuous portal crossing where gravity
changes and the ordinary camera interpolation eligibility check fails.
PortalViewNeedsTransform correctly identifies the camera cached in entrance
coordinates, but Draw previously restored savedView when interpolation was
rejected. Two rendered frames showed backing geometry before the next tick
rebuilt the camera in destination coordinates.

Draw now retains the destination-space authoritative view independently of
interpolation eligibility. On a stale-camera crossing without interpolation,
view models and attached lights are rebased to that view, and the crossing
body is suppressed. The original cached view is restored after drawing as
before. Physics position, velocity, gravity and collision are unchanged.

Muted captures and CSV traces are under validation/visualjolt. The baseline
draw position stays at the entrance during the crossing; the fixed draw
position matches the destination authoritative view.
