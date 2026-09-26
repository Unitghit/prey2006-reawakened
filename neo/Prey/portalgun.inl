// Experimental single-player portal tool. State uses existing entity/player save dictionaries.
static idCVar g_portalGun("g_portalGun", "0", CVAR_GAME | CVAR_BOOL | CVAR_ARCHIVE, "experimental slot-1 blue/orange portal tool");
static idCVar g_portalGunReticle("g_portalGunReticle", "1", CVAR_GAME | CVAR_BOOL | CVAR_ARCHIVE, "portal-gun placement indicators");
static idCVar g_portalReticleTrace("g_portalReticleTrace", "0", CVAR_GAME | CVAR_BOOL, "log portal aim preview results");
// Keep the visual aperture just ahead of thin wall decorations.
static const float RW_PORTAL_SURFACE_OFFSET = 1.0f;
// Artwork backing, hull clearance and broad-phase occupancy are deliberately
// different. Shrinking traversal to the artwork restores shoulder/foot snags.
static const float RW_PORTAL_BACKING_HALF_WIDTH = 39.0f;
static const float RW_PORTAL_BACKING_HALF_HEIGHT = 49.0f;
static const float RW_PORTAL_TRAVERSAL_HALF_WIDTH = 47.0f;
static const float RW_PORTAL_TRAVERSAL_HALF_HEIGHT = 71.0f;
static const float RW_PORTAL_PLAYER_CORNER_ALLOWANCE = 8.0f;
static const float RW_PORTAL_FOOT_FLAT_START = -48.0f;
static const float RW_PORTAL_FOOT_MIN = -105.0f;
// Candidate filter only; closing-clearance traces determine actual occupancy.
static const idBounds RW_PORTAL_OCCUPANCY_BOUNDS(idVec3(-2, -49, -87), idVec3(2, 49, 73));
static const idBounds RW_PORTAL_ENTITY_BOUNDS(idVec3(-4, -48, -72), idVec3(4, 48, 72));
static bool RW_PortalGunEnabled() {
    return g_portalGun.GetBool() && !gameLocal.isMultiplayer && !*cvarSystem->GetCVarString("fs_game");
}
static const char *RW_PortalName(int color) { return color ? "rw_gun_orange" : "rw_gun_blue"; }
static hhPortal *RW_GunPortal(int color) {
    idEntity *ent = gameLocal.FindEntity(RW_PortalName(color));
    return ent && ent->IsType(hhPortal::Type) ? static_cast<hhPortal *>(ent) : NULL;
}
// Compare the visible openings, not their deliberately oversized traversal
// bounds. A separating projection proves the two thin oval volumes are clear.
// Exact ellipse support keeps the sampled-axis test conservative: it can never
// approve intersecting ovals, including differently oriented floor portals.
static bool RW_PortalOpeningsOverlap(const idVec3 &surfaceCenter, const idMat3 &axis, const hhPortal *other) {
    if (!other) return false;
    const idVec3 delta = surfaceCenter + axis[0]*RW_PORTAL_SURFACE_OFFSET - other->GetOrigin();
    const float width = RW_PORTAL_BACKING_HALF_WIDTH + 1.0f;
    const float height = RW_PORTAL_BACKING_HALF_HEIGHT + 1.0f;
    const float thickness = 1.0f;
    if (delta.LengthSqr() > 4*(height*height+thickness*thickness)) return false;
    const idMat3 &otherAxis = other->GetAxis();
    for (int sample = 0; sample < 67; ++sample) {
        idVec3 direction;
        if (sample < 2) direction = sample ? otherAxis[0] : axis[0];
        else if (sample == 66) direction = axis[0].Cross(otherAxis[0]);
        else {
            const idMat3 &basis = sample < 34 ? axis : otherAxis;
            const float angle = (sample-2)%32 * (idMath::PI/32);
            direction = basis[1]*idMath::Cos(angle) + basis[2]*idMath::Sin(angle);
        }
        if (direction.LengthSqr() < 1e-8f) continue;
        const float a = idMath::Sqrt(Square(width*(direction*axis[1])) + Square(height*(direction*axis[2]))) +
            thickness*idMath::Fabs(direction*axis[0]);
        const float b = idMath::Sqrt(Square(width*(direction*otherAxis[1])) + Square(height*(direction*otherAxis[2]))) +
            thickness*idMath::Fabs(direction*otherAxis[0]);
        if (idMath::Fabs(delta*direction) >= a+b) return false;
    }
    return true;
}
// Broad-phase overlap only: touching the rim does not imply being in the wall.
static bool RW_PortalOccupied(const hhPortal *portal, const idPhysics *physics) {
    if (!portal) return false;
    idBounds local;
    local.Clear();
    const idBounds &bounds = physics->GetBounds();
    for (int i = 0; i < 8; ++i) {
        const idVec3 corner(bounds[(i&1)!=0].x, bounds[(i&2)!=0].y, bounds[(i&4)!=0].z);
        local.AddPoint((physics->GetOrigin() + corner * physics->GetAxis() - portal->GetOrigin()) * portal->GetAxis().Transpose());
    }
    return local.IntersectsBounds(RW_PORTAL_OCCUPANCY_BOUNDS);
}
static bool RW_GunPortalEntity(const idEntity *ent) {
    return ent && !ent->fl.noPortal && !ent->IsBound() &&
        (ent->IsType(hhPlayer::Type) || ent->IsType(hhProjectile::Type) || ent->IsType(idMoveable::Type) ||
         (ent->IsType(idAI::Type) && ent->health > 0 && ent->GetPhysics()->IsType(idPhysics_Monster::Type)));
}
// Disable only the wall cutout for final closing-clearance queries. Keeping
// portal trigger handling active avoids treating their sensor volumes as walls.
static bool rw_portalClosingQuery = false;
// Resolve movable occupants before moving either endpoint. Validate every move
// first, so a blocked object cannot leave a half-updated pair or moved neighbors.
static bool RW_ClearPortalOccupants(hhPortal *first, hhPortal *second) {
    idList<idEntity *> occupants;
    idList<idVec3> positions, normals;
    hhPortal *portals[2] = { first, second };
    for (int side = 0; side < 2; ++side) {
        hhPortal *portal = portals[side];
        if (!portal) continue;
        idEntity *entities[MAX_GENTITIES];
        const int count = gameLocal.clip.EntitiesTouchingBounds(portal->GetPhysics()->GetAbsBounds().Expand(16), -1, entities, MAX_GENTITIES);
        for (int i = 0; i < count; ++i) {
            idEntity *entity = entities[i];
            idPhysics *physics = entity->GetPhysics();
            if (!RW_GunPortalEntity(entity) || !RW_PortalOccupied(portal, physics)) continue;
            if (occupants.FindIndex(entity) >= 0) continue;
            const idVec3 normal = portal->GetAxis()[0];
            const idBounds &bounds = physics->GetBounds();
            float back = idMath::INFINITY;
            for (int k = 0; k < 8; ++k) {
                const idVec3 corner(bounds[(k&1)!=0].x, bounds[(k&2)!=0].y, bounds[(k&4)!=0].z);
                back = Min(back, (physics->GetOrigin()+corner*physics->GetAxis()-portal->GetOrigin())*normal);
            }
            // The visual plane is elevated. A stool/actor resting on the
            // original floor can overlap it without using the wall cutout.
            // Exact-contact traces may report solid for that existing contact.
            if (back >= -portal->spawnArgs.GetFloat("rw_portal_surface_offset") - 0.1f) continue;
            // The rectangular aperture bounds include harmless rim contact,
            // and a floor portal sits above the actual supporting floor. Test
            // the hull against the restored wall before deciding to move it.
            trace_t closingTrace;
            const bool previousClosingQuery = rw_portalClosingQuery;
            rw_portalClosingQuery = true;
            const bool needsClearance = gameLocal.clip.Translation(closingTrace,
                physics->GetOrigin(), physics->GetOrigin(), physics->GetClipModel(),
                physics->GetAxis(), physics->GetClipMask(), entity);
            rw_portalClosingQuery = previousClosingQuery;
            if (!needsClearance) continue;
            const idVec3 outward = physics->GetOrigin()+normal*Max(0.0f, 3.0f-back);
            idVec3 end;
            bool clear = false;
            // A ball can rest against the lip of an uneven opening. Prefer
            // straight out, then small diagonal sweeps into nearby free space.
            for (int ring = 0; ring <= 12 && !clear; ++ring) {
                const int samples = (ring ? 32 : 1)*5;
                for (int sample = 0; sample < samples && !clear; ++sample) {
                    const float angle = (sample/5)*(idMath::TWO_PI/32.0f);
                    end = outward + normal*((sample%5)*8.0f) + (portal->GetAxis()[1]*idMath::Cos(angle) +
                        portal->GetAxis()[2]*idMath::Sin(angle))*(ring*4.0f);
                    trace_t trace;
                    if (gameLocal.clip.Translation(trace, physics->GetOrigin(), end, physics->GetClipModel(),
                            physics->GetAxis(), physics->GetClipMask(), entity)) {
                        // A swept move may finish against a nearby surface
                        // after the entire hull has already cleared the opening.
                        if (trace.fraction <= 0 || back+(trace.endpos-physics->GetOrigin())*normal <= 2.25f) continue;
                        end = trace.endpos;
                    }
                    // The final position must be clear even after closure.
                    const bool oldClosingQuery = rw_portalClosingQuery;
                    rw_portalClosingQuery = true;
                    bool blocked = gameLocal.clip.Translation(trace, end, end, physics->GetClipModel(),
                        physics->GetAxis(), physics->GetClipMask(), entity);
                    rw_portalClosingQuery = oldClosingQuery;
                    // Independently clear destinations must not overlap one another.
                    const idBounds destination = physics->GetAbsBounds().Translate(end-physics->GetOrigin());
                    for (int k = 0; !blocked && k < occupants.Num(); ++k) {
                        const idPhysics *other = occupants[k]->GetPhysics();
                        blocked = destination.IntersectsBounds(other->GetAbsBounds().Translate(positions[k]-other->GetOrigin()));
                    }
                    clear = !blocked;
                }
            }
            if (!clear) {
                if (cvarSystem->GetCVarBool("developer")) gameLocal.Printf("PORTAL_REPLACEMENT_BLOCK clearance name=%s class=%s back=%f origin=%s\n", entity->GetName(), entity->spawnArgs.GetString("classname"), back, physics->GetOrigin().ToString());
                return false;
            }
            occupants.Append(entity); positions.Append(end); normals.Append(normal);
        }
    }
    for (int i = 0; i < occupants.Num(); ++i) {
        idEntity *entity = occupants[i];
        idPhysics *physics = entity->GetPhysics();
        idVec3 velocity = physics->GetLinearVelocity();
        velocity -= normals[i]*Min(0.0f, velocity*normals[i]);
        entity->SetOrigin(positions[i]);
        physics->SetLinearVelocity(velocity);
        entity->BecomeActive(TH_PHYSICS);
        entity->UpdateVisuals();
        if (cvarSystem->GetCVarBool("developer"))
            gameLocal.Printf("PORTAL_REPLACEMENT_CLEAR name=%s origin=%s\n", entity->GetName(), positions[i].ToString());
    }
    return true;
}

// Simulation-time approach assistance, independent of render rate and camera
// interpolation. Strong player input and fast lateral travel always take priority.
void RW_AssistPortalFall(const idEntity *entity, const idVec3 &origin, const idVec3 &look,
    int forwardInput, int sideInput, float dt, idVec3 &velocity) {
    if (!RW_PortalGunEnabled() ||
        !entity || !entity->IsType(hhPlayer::Type) || abs(forwardInput) > 32 || abs(sideInput) > 32 || dt <= 0) return;
    const hhPlayer *player = static_cast<const hhPlayer *>(entity);
    if (player->health <= 0 || player->IsSpiritOrDeathwalking() || player->InVehicle()) return;
    const idVec3 down = entity->GetPhysics()->GetGravityNormal();
    const float falling = velocity * down;
    idVec3 lateral = velocity - down * falling;
    if (falling < 80 || look * down < 0.15f || lateral.LengthSqr() > Square(240.0f)) return;
    hhPortal *target = NULL;
    float nearest = 1e30f;
    idVec3 offset;
    float height = 0;
    for (int color = 0; color < 2; ++color) {
        hhPortal *portal = RW_GunPortal(color);
        if (!portal || !portal->cameraTarget || portal->GetAxis()[0] * -down < 0.95f) continue;
        const idVec3 relative = origin - portal->GetOrigin();
        const float h = relative * -down;
        if (h < 8 || h > 384 || idMath::Fabs(relative * portal->GetAxis()[1]) > 67 ||
            idMath::Fabs(relative * portal->GetAxis()[2]) > 96) continue;
        const idVec3 sideways = relative + down * h;
        if (sideways.LengthSqr() < nearest) { target = portal; nearest = sideways.LengthSqr(); offset = -sideways; height = h; }
    }
    if (!target) return;
    const float gravity = entity->GetPhysics()->GetGravity().Length();
    const float arrival = gravity > 0.01f ? (idMath::Sqrt(falling*falling + 2*gravity*height)-falling)/gravity : height/falling;
    idVec3 desired = offset / Max(0.12f, arrival);
    desired.Truncate(96);
    idVec3 correction = desired - lateral;
    correction.Truncate(400 * dt);
    velocity += correction;
    if (cvarSystem->GetCVarBool("com_fpsTrace") && correction.LengthSqr() > 0.001f)
        gameLocal.Printf("PORTAL_GUIDANCE time=%d distance=%.3f correction=%.3f\n", gameLocal.time, idMath::Sqrt(nearest), correction.Length());
}

// Guide a walking hull past the narrow shoulder of an upright opening. This
// changes lateral velocity only; all movement still uses ordinary swept collision.
void RW_AssistPortalApproach(const idEntity *entity, const idVec3 &origin,
    const idVec3 &wish, float dt, idVec3 &velocity) {
    if (!RW_PortalGunEnabled() ||
        !entity || !entity->IsType(hhPlayer::Type) || dt <= 0) return;
    const hhPlayer *player = static_cast<const hhPlayer *>(entity);
    if (player->health <= 0 || player->IsSpiritOrDeathwalking() || player->InVehicle()) return;
    const idPhysics *physics = entity->GetPhysics();
    const idClipModel *clip = physics->GetClipModel();
    if (!clip || !clip->IsTraceModel()) return;
    for (int color = 0; color < 2; ++color) {
        hhPortal *portal = RW_GunPortal(color);
        if (!portal || !portal->cameraTarget || portal->GetAxis()[2]*-physics->GetGravityNormal() < 0.99f) continue;
        const idVec3 normal = portal->GetAxis()[0];
        const idVec3 local = (origin-portal->GetOrigin())*portal->GetAxis().Transpose();
        if (local.x < 0 || local.x > 64 || idMath::Fabs(local.y) > 47 || wish*normal > -0.5f ||
            RW_PortalFits(portal, clip->GetTraceModel(), physics->GetAxis(), origin, true)) continue;
        const idVec3 toward = portal->GetAxis()[1]*(local.y > 0 ? -1.0f : 1.0f);
        // Explicit steering away from the center overrides assistance.
        if (wish*toward < -0.25f) continue;
        for (float distance = 2; distance <= 24; distance += 2) {
            const idVec3 candidate = origin+toward*distance;
            if (!RW_PortalFits(portal, clip->GetTraceModel(), physics->GetAxis(), candidate, true)) continue;
            trace_t trace;
            if (gameLocal.clip.Translation(trace, origin, candidate, clip, physics->GetAxis(), physics->GetClipMask(), entity)) {
                // A sloped approach may require the same up/across/down sweep
                // used by normal walking. This validates guidance, not a warp.
                const idVec3 up = -physics->GetGravityNormal();
                const float step = Min(pm_stepsize.GetFloat(), pm_bboxwidth.GetFloat()*0.5f);
                const idVec3 raised = origin+up*step;
                const idVec3 across = candidate+up*step;
                if (gameLocal.clip.Translation(trace, origin, raised, clip, physics->GetAxis(), physics->GetClipMask(), entity) ||
                    gameLocal.clip.Translation(trace, raised, across, clip, physics->GetAxis(), physics->GetClipMask(), entity)) break;
                gameLocal.clip.Translation(trace, across, candidate, clip, physics->GetAxis(), physics->GetClipMask(), entity);
                if (trace.fraction <= 0 || (trace.fraction < 1 && trace.c.normal*up < 0.7f) ||
                    !RW_PortalFits(portal, clip->GetTraceModel(), physics->GetAxis(), trace.endpos, true)) break;
            }
            const float target = Min(48.0f, distance/dt);
            velocity += toward*Max(0.0f, target-velocity*toward);
            if (cvarSystem->GetCVarBool("com_fpsTrace"))
                gameLocal.Printf("PORTAL_APPROACH_GUIDANCE distance=%.3f\n", distance);
            return;
        }
    }
}
static void RW_AssistFloorExit(idEntity *entity, idEntity *destination, const idVec3 &origin,
    const idMat3 &axis, idVec3 &velocity) {
    const bool npc = entity->IsType(idAI::Type) && entity->GetPhysics()->IsType(idPhysics_Monster::Type);
    if (!entity->IsType(hhPlayer::Type) && !npc) return;
    idVec3 up = -destination->GetGravity();
    const float gravity = up.Normalize();
    const idVec3 normal = destination->GetAxis()[0];
    if (gravity < 0.01f || normal * up < 0.95f) return;
    const float acceleration = gravity * (normal * up);
    // Players need only enough lift to clear their remaining hull. NPCs cross
    // at their feet and need a small rise to emerge before gravity pulls back.
    float back = idMath::INFINITY;
    const idBounds &bounds = entity->GetPhysics()->GetBounds();
    for (int k = 0; k < 8; ++k) {
        const idVec3 corner(bounds[(k&1)!=0].x, bounds[(k&2)!=0].y, bounds[(k&4)!=0].z);
        back = Min(back, (origin + corner*axis - destination->GetOrigin())*normal);
    }
    const float needed = npc ? idMath::ClampFloat(0, 128, 24.0f-back) : idMath::ClampFloat(0.0f, 24.0f, 1.0f-back);
    if (needed <= 0) return;
    const float maxMinimum = idMath::Sqrt(2 * acceleration * needed);
    const float outgoing = velocity * normal;
    // Preserve sufficient outgoing momentum. Players retain their existing
    // near-stall threshold; NPCs get the bounded, clearance-tested rise.
    if ((!npc && outgoing >= 48.0f) || outgoing >= maxMinimum) return;
    trace_t clearance;
    gameLocal.clip.Translation(clearance, origin, origin + normal*(npc ? needed+2 : 26), entity->GetPhysics()->GetClipModel(),
        axis, entity->GetPhysics()->GetClipMask(), entity);
    const float space = Min(needed, (clearance.endpos-origin)*normal - 2);
    if (space <= 0) return;
    const float minimum = idMath::Sqrt(2 * acceleration * space);
    if (outgoing >= minimum) return;
    velocity += normal * (minimum - outgoing);
    entity->GetPhysics()->SetLinearVelocity(velocity);
    if (cvarSystem->GetCVarBool("com_fpsTrace"))
        gameLocal.Printf("PORTAL_FLOOR_EXIT before=%.3f after=%.3f clearance=%.3f\n", outgoing, minimum, space);
}

// During camera-delayed ground entry, only the emerged slice belongs in the
// destination world. Clip the query hull at the source plane before transforming
// it; testing the full body would collide with objects still behind the exit.
static bool RW_GroundPortalPartialBlocked(hhPortal *portal, idEntity *entity, const idVec3 &origin, idVec3 *clearOrigin) {
    if (!portal->cameraTarget) return false;
    const float eyeOffset = RW_GroundPortalEyeOffset(portal, entity);
    const idVec3 normal = portal->GetAxis()[0];
    const float depth = (origin-portal->GetOrigin())*normal;
    // The visual opening is raised above the support surface. Standing on
    // that surface has not entered the hole and must not consult the exit.
    const float surfaceOffset = portal->spawnArgs.GetFloat("rw_portal_surface_offset", "1");
    if (eyeOffset <= 0 || depth >= -surfaceOffset - 0.25f) return false;
    const idMat3 sourceHullAxis = entity->GetPhysics()->GetAxis();
    // Proximity includes the surrounding floor. Only query the remote body
    // once the same aperture test used by the collision cutout accepts it.
    // Otherwise an obstacle at the mapped position creates an invisible fence
    // outside the opening, where the player is still entirely in this world.
    const idClipModel *sourceClip = entity->GetPhysics()->GetClipModel();
    if (!sourceClip || !sourceClip->IsTraceModel() ||
        !RW_PortalFits(portal, sourceClip->GetTraceModel(), sourceHullAxis, origin, true)) return false;
    const float upDot = sourceHullAxis[2]*normal;
    if (upDot < 0.95f) return false;
    idBounds emerged = entity->GetPhysics()->GetBounds();
    const float sideExtent = Max(idMath::Fabs(emerged[0].x), idMath::Fabs(emerged[1].x))*idMath::Fabs(sourceHullAxis[0]*normal) +
        Max(idMath::Fabs(emerged[0].y), idMath::Fabs(emerged[1].y))*idMath::Fabs(sourceHullAxis[1]*normal);
    emerged[1].z = Min(emerged[1].z, (-depth-sideExtent)/upDot);
    if (emerged[1].z <= emerged[0].z + 0.25f) return false;
    const idMat3 inverse = portal->GetAxis().Transpose();
    const idMat3 destination = portal->cameraTarget->GetAxis();
    idMat3 remoteAxis = sourceHullAxis;
    for (int k = 0; k < 3; ++k) PortalRotate(remoteAxis[k], inverse, destination, true);
    idVec3 remote = origin-portal->GetOrigin();
    PortalRotate(remote, inverse, destination, true);
    remote += portal->cameraTarget->GetOrigin();
    idTraceModel shape(emerged);
    idClipModel clip(shape);
    trace_t trace;
    const bool blocked = gameLocal.clip.Translation(trace, remote, remote, &clip, remoteAxis, entity->GetPhysics()->GetClipMask(), entity);
    // A rotated square hull can overlap the exit floor at the oval's edge.
    // Use ordinary step clearance for floor contacts, including fast approaches
    // that overshoot the skin margin. Resolve world overlaps toward the opening
    // center, with a swept source check and a fresh destination occupancy test.
    // Other contacts retain the small skin allowance; movable objects block.
    if (blocked && clearOrigin && trace.c.entityNum == ENTITYNUM_WORLD) {
        idVec3 destinationUp = -portal->cameraTarget->GetGravity();
        destinationUp.Normalize();
        // Contents tests report the closest brush face. Inside a thin floor
        // slab that may be its underside, even though entry came from above.
        // Find the supporting top face rather than pushing toward the void.
        if (trace.c.normal*destinationUp < -0.99f) {
            trace_t floor;
            gameLocal.clip.TracePoint(floor, remote, remote-destinationUp*(pm_bboxwidth.GetFloat()*2),
                entity->GetPhysics()->GetClipMask(), entity);
            if (floor.fraction > 0 && floor.fraction < 1 && floor.c.entityNum == ENTITYNUM_WORLD &&
                floor.c.normal*destinationUp > 0.99f) trace.c = floor.c;
        }
        float minimum = idMath::INFINITY;
        for (int k = 0; k < 8; ++k) {
            const idVec3 corner(emerged[(k&1)!=0].x, emerged[(k&2)!=0].y, emerged[(k&4)!=0].z);
            minimum = Min(minimum, (remote + corner*remoteAxis)*trace.c.normal);
        }
        const float penetration = trace.c.dist - minimum;
        const bool floorContact = trace.c.normal*destinationUp > 0.99f;
        const float stepClearance = Min(pm_stepsize.GetFloat(), pm_bboxwidth.GetFloat()*0.5f);
        const float maxClearance = floorContact ? Max(2.0f, stepClearance) : 2.0f;
        idVec3 direction = trace.c.normal;
        PortalRotate(direction, destination.Transpose(), portal->GetAxis(), true);
        if (penetration >= 0 && penetration <= maxClearance &&
            idMath::Fabs(direction*normal) < 0.01f && direction*(origin-portal->GetOrigin()) < 0) {
            const idVec3 candidate = origin + direction*(penetration+0.25f);
            trace_t source;
            if (!gameLocal.clip.Translation(source, origin, candidate, sourceClip, sourceHullAxis,
                    entity->GetPhysics()->GetClipMask(), entity) &&
                !RW_GroundPortalPartialBlocked(portal, entity, candidate)) {
                *clearOrigin = candidate;
                if (cvarSystem->GetCVarBool("com_fpsTrace"))
                    gameLocal.Printf("PORTAL_ENTRY_CLEARANCE distance=%.3f\n", penetration+0.25f);
                return false;
            }
        }
    }
    // A moving exit obstruction can catch a corner of the emerged hull.
    // Find a small, fully collision-checked lateral clearance rather than
    // freezing the entire player at that overlap. Never bypass the obstruction:
    // the final emerged hull must be clear and the source sweep must succeed.
    if (blocked && clearOrigin) {
        const float limit = Min(pm_stepsize.GetFloat(), pm_bboxwidth.GetFloat()*0.5f);
        const idVec3 relative = origin-portal->GetOrigin();
        for (float distance = 2; distance <= limit; distance += 2) {
            for (int sample = 0; sample < 16; ++sample) {
                const float angle = sample * (idMath::TWO_PI/16.0f);
                const idVec3 direction = portal->GetAxis()[1]*idMath::Cos(angle) + portal->GetAxis()[2]*idMath::Sin(angle);
                if (direction*relative >= -0.01f) continue;
                const idVec3 candidate = origin + direction*distance;
                if (!RW_PortalFits(portal, sourceClip->GetTraceModel(), sourceHullAxis, candidate, true) ||
                    RW_GroundPortalPartialBlocked(portal, entity, candidate)) continue;
                trace_t source;
                if (gameLocal.clip.Translation(source, origin, candidate, sourceClip, sourceHullAxis,
                        entity->GetPhysics()->GetClipMask(), entity)) continue;
                *clearOrigin = candidate;
                if (cvarSystem->GetCVarBool("com_fpsTrace"))
                    gameLocal.Printf("PORTAL_ENTRY_LATERAL_CLEARANCE distance=%.3f\n", distance);
                return false;
            }
        }
    }
    if (blocked && cvarSystem->GetCVarBool("com_fpsTrace"))
        gameLocal.Printf("PORTAL_PARTIAL_BLOCK depth=%.3f entity=%d material=%s normal=%s remote=%s dist=%.3f contents=%d\n", depth, trace.c.entityNum, trace.c.material ? trace.c.material->GetName() : "none", trace.c.normal.ToString(), remote.ToString(), trace.c.dist, trace.c.contents);
    return blocked;
}

static bool RW_PortalFits(const hhPortal *portal, const idTraceModel *trm, const idMat3 &axis, const idVec3 &origin, bool playerHull) {
    if (!trm) return false;
    // Upright wall portals must not turn the oval's narrowing lower edge into
    // a step under the player's square collision hull. Keep a flat foot opening
    // with one step-height of clearance, while retaining the curved sides/top.
    // Floor/ceiling portals keep the original aperture test in their own plane.
    const bool footClearance = playerHull && axis[2] * portal->GetAxis()[2] > 0.95f;
    const idVec3 hullCenter = (origin + trm->bounds.GetCenter() * axis - portal->GetOrigin()) * portal->GetAxis().Transpose();
    for (int i = 0; i < 8; ++i) {
        const idVec3 corner(trm->bounds[(i&1)!=0].x, trm->bounds[(i&2)!=0].y, trm->bounds[(i&4)!=0].z);
        idVec3 local = (origin + corner * axis - portal->GetOrigin()) * portal->GetAxis().Transpose();
        // Give the player's square collision corners a small shoulder/foot
        // allowance. The center must still fit; no camera nudging or suction.
        if (playerHull) {
            local.y -= idMath::ClampFloat(-RW_PORTAL_PLAYER_CORNER_ALLOWANCE, RW_PORTAL_PLAYER_CORNER_ALLOWANCE, local.y - hullCenter.y);
            local.z -= idMath::ClampFloat(-RW_PORTAL_PLAYER_CORNER_ALLOWANCE, RW_PORTAL_PLAYER_CORNER_ALLOWANCE, local.z - hullCenter.z);
        }
        float apertureZ = local.z;
        if (footClearance && apertureZ < RW_PORTAL_FOOT_FLAT_START) {
            if (apertureZ < RW_PORTAL_FOOT_MIN) return false;
            apertureZ = RW_PORTAL_FOOT_FLAT_START;
        }
        if (Square(local.y / RW_PORTAL_TRAVERSAL_HALF_WIDTH) + Square(apertureZ / RW_PORTAL_TRAVERSAL_HALF_HEIGHT) > 1.0f) return false;
    }
    return true;
}
bool RW_PortalHoldPlayerAxis(const idEntity *entity) {
    if (!RW_PortalGunEnabled() ||
        !entity || !entity->IsType(hhPlayer::Type)) return false;
    const idPhysics *physics = entity->GetPhysics();
    const idClipModel *clip = physics->GetClipModel();
    if (!clip || !clip->IsTraceModel()) return false;
    for (int color = 0; color < 2; ++color) {
        const hhPortal *portal = RW_GunPortal(color);
        if (!portal || !portal->cameraTarget) continue;
        const idVec3 normal = portal->GetAxis()[0];
        const float depth = (physics->GetOrigin()-portal->GetOrigin())*normal;
        // Do not rotate a head-first emerging hull upright inside the wall.
        // Resume normal gravity alignment after its feet have cleared enough
        // for the upright hull's half-width.
        if (physics->GetAxis()[2]*normal > 0.95f && depth > -physics->GetBounds()[1].z &&
            depth < 18.0f && RW_PortalFits(portal, clip->GetTraceModel(), physics->GetAxis(), physics->GetOrigin(), true)) return true;
        // Wall-to-sloped-ceiling exits emerge sideways. Gravity alignment must
        // not swing the tall hull back through the supporting ceiling while its
        // current orientation is still passing through the opening.
        const idVec3 gravityUp = -physics->GetGravityNormal();
        if (physics->GetAxis()[2]*gravityUp < 0.999f) {
            const idBounds &bounds = physics->GetBounds();
            float front = depth;
            for (int k = 0; k < 8; ++k) {
                const idVec3 corner(bounds[(k&1)!=0].x, bounds[(k&2)!=0].y, bounds[(k&4)!=0].z);
                front = Max(front, depth + (corner*physics->GetAxis())*normal);
            }
            const float vertical = normal*gravityUp;
            const float radius = idMath::Sqrt(Square(Max(idMath::Fabs(bounds[0].x), idMath::Fabs(bounds[1].x))) +
                Square(Max(idMath::Fabs(bounds[0].y), idMath::Fabs(bounds[1].y))));
            const float uprightBack = Min(bounds[0].z*vertical, bounds[1].z*vertical) -
                radius*idMath::Sqrt(Max(0.0f, 1.0f-vertical*vertical));
            if (front > 0 && depth + uprightBack < 1.0f && depth > -bounds[1].z &&
                RW_PortalFits(portal, clip->GetTraceModel(), physics->GetAxis(), physics->GetOrigin(), true)) return true;
        }
    }
    return false;
}
bool RW_PortalClipPlane(const idEntity *entity, const idTraceModel *trm, const idMat3 &axis,
    const idVec3 &start, const idVec3 &end, idPlane &plane, float &limit) {
    limit = 1.0f;
    if (rw_portalClosingQuery) return false;
    if (!RW_PortalGunEnabled() || !RW_GunPortalEntity(entity) || !trm) return false;
    hhPortal *a = RW_GunPortal(0), *b = RW_GunPortal(1);
    if (!a || !b || !a->cameraTarget || !b->cameraTarget) return false;
    for (int i = 0; i < 2; ++i) {
        hhPortal *portal = i ? b : a;
        const idVec3 normal = portal->GetAxis()[0];
        const idVec3 actualOffset = entity->GetPhysics()->GetOrigin() - portal->GetOrigin();
        // After head-first ceiling entry, the feet remain behind the exit
        // while the rotated hull emerges. Keep the cutout for that overlap,
        // but still reject players whose whole hull is behind the surface.
        float bodyFront = 0;
        if (entity->IsType(hhPlayer::Type) || entity->IsType(idAI::Type)) {
            const idBounds &body = entity->GetPhysics()->GetBounds();
            for (int k = 0; k < 8; ++k) {
                const idVec3 corner(body[(k&1)!=0].x, body[(k&2)!=0].y, body[(k&4)!=0].z);
                bodyFront = Max(bodyFront, (corner*entity->GetPhysics()->GetAxis())*normal);
            }
        }
        if (actualOffset.LengthSqr() < 128*128 && actualOffset * normal < -Max(4.0f, bodyFront + 8.0f)) continue;
        const float d0 = (start - portal->GetOrigin()) * normal;
        const float d1 = (end - portal->GetOrigin()) * normal;
        // Fast projectiles can cross the complete opening in one physics step.
        // Validate their hull at the plane instead of requiring both endpoints
        // to remain in the player's short approach band.
        if (!entity->IsType(hhPlayer::Type) && d0 >= 0 && d1 <= 0 && d0 > d1) {
            const idVec3 crossing = start + (end-start) * (d0 / (d0-d1));
            if (RW_PortalFits(portal, trm, axis, crossing, false)) {
                plane.SetNormal(normal);
                plane.FitThroughPoint(portal->GetOrigin() - normal * portal->spawnArgs.GetFloat("rw_portal_surface_offset"));
                return true;
            }
        }
        // Never open unrelated, remote or backside world geometry.
        const float eyeOffset = RW_GroundPortalEyeOffset(portal, entity);
        const bool groundCrossing = eyeOffset > 0 && d0 + eyeOffset >= -8 && d1 + eyeOffset <= 0 && d1 < d0;
        if (d0 < -90 || d0 > 90 || (d1 < -90 && !groundCrossing) || d1 > 90) continue;
        if (!RW_PortalFits(portal, trm, axis, start, entity->IsType(hhPlayer::Type))) continue;
        if (!RW_PortalFits(portal, trm, axis, end, entity->IsType(hhPlayer::Type))) {
            // Once a hull straddles the wall, the oval edge must be a real
            // collision boundary, including during the player's step-down trace.
            float minDepth = 1e9f, maxDepth = -1e9f;
            for (int k = 0; k < 8; ++k) {
                const idVec3 corner(trm->bounds[(k&1)!=0].x, trm->bounds[(k&2)!=0].y, trm->bounds[(k&4)!=0].z);
                // Test against the supporting wall, not the offset visual plane.
                // Otherwise feet already above a floor are treated as embedded
                // and the synthetic oval edge becomes an invisible fence.
                const float depth = d0 + portal->spawnArgs.GetFloat("rw_portal_surface_offset") + (corner * axis) * normal;
                minDepth = Min(minDepth, depth); maxDepth = Max(maxDepth, depth);
            }
            if (minDepth >= 0.25f || maxDepth < -0.25f) continue;
            float low = 0, high = 1;
            for (int k = 0; k < 20; ++k) {
                const float mid = (low + high) * 0.5f;
                if (RW_PortalFits(portal, trm, axis, start + (end-start)*mid, entity->IsType(hhPlayer::Type))) low = mid; else high = mid;
            }
            limit = Max(0.0f, low - 0.001f);
        }
        plane.SetNormal(normal);
        plane.FitThroughPoint(portal->GetOrigin() - normal * portal->spawnArgs.GetFloat("rw_portal_surface_offset"));
        return true;
    }
    return false;
}
// Some maps place an invisible actor-clip shell ahead of the visible wall.
// Match that exact nearby plane, only for invisible player-clip contents.
bool RW_PortalCoverPlane(const idPlane &wall, const idVec3 &query, idPlane &cover) {
    hhPortal *nearest = NULL;
    float nearestDistance = 1e30f;
    for (int color = 0; color < 2; ++color) {
        hhPortal *portal = RW_GunPortal(color);
        if (!portal || portal->GetAxis()[0] * wall.Normal() < 0.9999f) continue;
        const idVec3 surface = portal->GetOrigin() - portal->GetAxis()[0] * portal->spawnArgs.GetFloat("rw_portal_surface_offset");
        if (idMath::Fabs(wall.Distance(surface)) > 0.15f) continue;
        const float distance = (query - portal->GetOrigin()).LengthSqr();
        if (distance < nearestDistance) { nearest = portal; nearestDistance = distance; }
    }
    if (nearest) {
        // Uneven terrain can have several actor-clip facets under one portal.
        // Query the facet beneath this hull, rather than caching the center's
        // plane for every future entry, exit and restored save.
        const idVec3 sample = query - wall.Normal()*wall.Distance(query);
        trace_t trace;
        // Leave a start margin around a cover at the maximum allowed depth.
        gameLocal.clip.TracePoint(trace, sample + wall.Normal()*33, sample - wall.Normal()*8,
            CONTENTS_PLAYERCLIP, NULL);
        const float facing = trace.c.normal*wall.Normal();
        const float coverDepth = facing > 0 ? (trace.c.dist-trace.c.normal*sample)/facing : 0;
        if (trace.fraction > 0 && trace.fraction < 1 && coverDepth <= 32.15f && trace.c.entityNum == ENTITYNUM_WORLD &&
            // Actor-clip covers may bevel away from the visible support wall.
            // Accept up to 30 degrees while retaining the bounded probe and
            // exact detected player-clip plane; real solid obstacles stay solid.
            trace.c.normal * wall.Normal() >= 0.8660254f) {
            cover.SetNormal(trace.c.normal);
            cover.SetDist(trace.c.dist);
            return true;
        }
    }
    return false;
}
// Fixed scenery meshes use the same half-space collision cutout as the world.
// Do not anchor portals to props, animated objects, or bound/moving geometry.
static bool RW_PortalPlacementSurface(const trace_t &hit) {
    if (hit.fraction >= 1) return false;
    if (hit.c.entityNum == ENTITYNUM_WORLD) return true;
    if (hit.c.entityNum < 0 || hit.c.entityNum >= MAX_GENTITIES) return false;
    const idEntity *surface = gameLocal.entities[hit.c.entityNum];
    if (!surface || !surface->IsType(idStaticEntity::Type) || surface->GetBindMaster() || surface->fl.takedamage) return false;
    const idPhysics *physics = surface->GetPhysics();
    const idClipModel *clip = physics->GetClipModel();
    return physics->IsType(idPhysics_Static::Type) && physics->GetNumClipModels() == 1 &&
        clip && !clip->IsTraceModel() && physics->GetLinearVelocity().LengthSqr() < 0.0001f &&
        physics->GetAngularVelocity().LengthSqr() < 0.0001f;
}
// Check the full aperture volume, including solid obstacles between sample rays.
static bool RW_PortalWindowClear(const idVec3 &center, const idMat3 &axis, const idEntity *ignore, float skin = 0.25f) {
    const float rimScale = 1.0f / idMath::Cos(idMath::PI / 8.0f);
    idTraceModel window;
    window.SetupCylinder(idBounds(idVec3(-RW_PORTAL_BACKING_HALF_WIDTH*rimScale, -RW_PORTAL_BACKING_HALF_HEIGHT*rimScale, skin),
        idVec3(RW_PORTAL_BACKING_HALF_WIDTH*rimScale, RW_PORTAL_BACKING_HALF_HEIGHT*rimScale, 4.0f)), 8);
    idClipModel clearance(window);
    return !gameLocal.clip.Contents(center, &clearance, idMat3(axis[1], axis[2], axis[0]), MASK_SOLID, ignore);
}
static bool RW_PortalSurfaceSupports(const idVec3 &center, const idMat3 &axis, const idEntity *ignore) {
    // The central ellipse has half the original radii (one quarter the area).
    // It anchors the flat cutout. The full artwork still needs backing, but
    // shallow recesses outside that core are safely behind the cutout plane.
    for (int sampleIndex = 0; sampleIndex < 32 + 11*15; ++sampleIndex) {
        float py, pz;
        if (sampleIndex < 32) {
            const float angle = (sampleIndex % 16) * idMath::TWO_PI / 16.0f;
            const float scale = sampleIndex < 16 ? 1.0f : 0.5f;
            py = scale * RW_PORTAL_BACKING_HALF_WIDTH * idMath::Cos(angle); pz = scale * RW_PORTAL_BACKING_HALF_HEIGHT * idMath::Sin(angle);
        } else {
            const int grid = sampleIndex - 32;
            py = (grid % 11 - 5) * 8.0f; pz = (grid / 11 - 7) * 8.0f;
            if (Square(py / RW_PORTAL_BACKING_HALF_WIDTH) + Square(pz / RW_PORTAL_BACKING_HALF_HEIGHT) > 1.0f) continue;
        }
        const idVec3 sample = center + axis[1] * py + axis[2] * pz;
        trace_t support;
        gameLocal.clip.TracePoint(support, sample + axis[0]*2, sample - axis[0]*8.25f, MASK_SOLID, ignore);
        const float facing = support.c.normal * axis[0];
        if (!RW_PortalPlacementSurface(support) || facing < 0.5f) return false;
        const float depth = (sample * support.c.normal - support.c.dist) / facing;
        const bool flatCore = (sampleIndex >= 16 && sampleIndex < 32) || Square(py / (RW_PORTAL_BACKING_HALF_WIDTH * 0.5f)) + Square(pz / (RW_PORTAL_BACKING_HALF_HEIGHT * 0.5f)) <= 1.0f;
        if (depth < -0.14f || depth > 8.0f ||
            (flatCore && (facing < 0.9999f || idMath::Fabs(depth) > 0.14f))) return false;
    }
    // A conservative oval prism checks the whole window against protruding
    // corners and solid entities, including objects between the sample rays.
    return RW_PortalWindowClear(center, axis, ignore);
}

// Keep the aperture rigid, but fit it to gently uneven surfaces. The final plane
// lies above every sampled bump, so rendering and the existing collision cutout
// agree about which side contains the terrain. Never carve a separate deep hole.
static bool RW_FitPortalSurface(idVec3 &center, idMat3 &axis, const hhPlayer *player) {
    const idVec3 originalCenter = center;
    const idMat3 originalAxis = axis;
    for (int pass = 0; pass < 2; ++pass) {
        float xx = 0, xy = 0, yy = 0, xh = 0, yh = 0, sumH = 0;
        float lowest = idMath::INFINITY, highest = -idMath::INFINITY;
        int count = 0;
        for (int index = 0; index < 32 + 11*15; ++index) {
            float x, y;
            if (index < 32) {
                const float angle = index*idMath::TWO_PI/32.0f;
                x = 39.0f*idMath::Cos(angle); y = 49.0f*idMath::Sin(angle);
            } else {
                const int grid = index-32;
                x = (grid%11-5)*8.0f; y = (grid/11-7)*8.0f;
                if (Square(x/39.0f)+Square(y/49.0f) > 1) continue;
            }
            const idVec3 sample = center+axis[1]*x+axis[2]*y;
            trace_t hit;
            gameLocal.clip.TracePoint(hit, sample+axis[0]*16, sample-axis[0]*16, MASK_SOLID, player);
            if (!RW_PortalPlacementSurface(hit) ||
                hit.c.normal*axis[0] < (pass ? 0.9f : 0.7f)) return false;
            // Remove the collision epsilon before fitting the physical surface.
            const idVec3 point = hit.endpos-hit.c.normal*(hit.endpos*hit.c.normal-hit.c.dist);
            const float height = (point-center)*axis[0];
            xx += x*x; xy += x*y; yy += y*y; xh += x*height; yh += y*height; sumH += height;
            lowest = Min(lowest, height); highest = Max(highest, height); ++count;
        }
        if (!pass) {
            // Sampling is symmetric about the center, so x/y means are zero.
            const float determinant = xx*yy-xy*xy;
            if (determinant <= 0.001f || !count) return false;
            idVec3 normal = axis[0]-axis[1]*((xh*yy-yh*xy)/determinant)-axis[2]*((yh*xx-xh*xy)/determinant);
            normal.Normalize();
            if (normal*originalAxis[0] < 0.9f) return false;
            const idVec3 mean = center+axis[0]*(sumH/count);
            center += normal*((mean-center)*normal);
            idVec3 up = axis[2]-normal*(axis[2]*normal);
            up.Normalize();
            axis = idMat3(normal, up.Cross(normal), up);
        } else {
            // Eight units of relief across the whole opening, including its rim.
            if (highest-lowest > 8.0f || idMath::Fabs(highest) > 8.0f) return false;
            // Leave a small numerical margin above the sampled high point.
            // The volume test below includes the plane itself, so an unsampled
            // peak cannot remain in front of the collision cutout.
            center += axis[0]*(highest+0.25f);
        }
    }
    if ((center-originalCenter).LengthSqr() > Square(8.0f) || !RW_PortalWindowClear(center, axis, player, 0.0f)) return false;
    // The entrance must also accommodate the standing player in front of it.
    const idPhysics *physics = player->GetPhysics();
    idBounds bounds = physics->GetBounds();
    bounds[1].z = Max(bounds[1].z, pm_normalheight.GetFloat());
    float back = 0;
    for (int k = 0; k < 8; ++k) {
        const idVec3 corner(bounds[(k&1)!=0].x, bounds[(k&2)!=0].y, bounds[(k&4)!=0].z);
        back = Min(back, (corner*physics->GetAxis())*axis[0]);
    }
    // Center the standing hull across the opening on walls as well as floors.
    // Keep its rear extent just outside the fitted surface in every orientation.
    const idVec3 hullCenter = bounds.GetCenter()*physics->GetAxis();
    const idVec3 tangentCenter = hullCenter-axis[0]*(hullCenter*axis[0]);
    const idVec3 entry = center-tangentCenter+axis[0]*(1.0f-back);
    idTraceModel standingShape(bounds);
    idClipModel standingClip(standingShape);
    trace_t clearance;
    return !gameLocal.clip.Translation(clearance, entry, entry, &standingClip, physics->GetAxis(),
        physics->GetClipMask(), player);
}
// A wall portal close to a floor should be walk-through, not a raised hoop.
// This adjusts portal placement, never the player's origin or teleport velocity.
static bool RW_PortalFloorCenter(idVec3 &center, const idMat3 &axis, const idVec3 &gravityUp, const idEntity *ignore) {
    if (axis[2] * gravityUp < 0.95f) return false;
    trace_t floor;
    const idVec3 probe = center + axis[0] * 24;
    gameLocal.clip.TracePoint(floor, probe, probe - axis[2]*128, MASK_SOLID, ignore);
    if (!RW_PortalPlacementSurface(floor) || floor.c.normal * axis[2] < 0.99f) return false;
    const float height = (center - floor.endpos) * axis[2];
    if (height < 8 || height > 120) return false;
    // The floor trace stops a clip epsilon above the actual plane. Use the
    // plane itself so two endpoints over the same floor align exactly.
    const float planeHeight = (center * floor.c.normal - floor.c.dist) / (axis[2] * floor.c.normal);
    const idVec3 candidate = center + axis[2] * (71.0f - planeHeight);
    if (!RW_PortalSurfaceSupports(candidate, axis, ignore)) return false;
    center = candidate;
    return true;
}
static void RW_UpdateGunPortalFloor(hhPortal *portal) {
    const float previousOffset = portal->spawnArgs.GetFloat("rw_portal_surface_offset");
    const bool floorAligned = portal->spawnArgs.GetBool("rw_portal_floor_aligned");
    if (floorAligned && previousOffset == RW_PORTAL_SURFACE_OFFSET) return;
    // Save migration: work in the real wall plane, then apply the attachment
    // offset once. Never accumulate offsets across reloads or repeated Thinks.
    idVec3 center = portal->GetOrigin() - portal->GetAxis()[0] * previousOffset;
    if (!floorAligned) RW_PortalFloorCenter(center, portal->GetAxis(), -portal->GetPhysics()->GetGravityNormal(), gameLocal.GetLocalPlayer());
    portal->spawnArgs.SetBool("rw_portal_floor_aligned", true);
    portal->spawnArgs.SetFloat("rw_portal_surface_offset", RW_PORTAL_SURFACE_OFFSET);
    portal->SetOrigin(center + portal->GetAxis()[0] * RW_PORTAL_SURFACE_OFFSET);
    portal->spawnArgs.SetVector("origin", portal->GetOrigin());
    portal->UpdateVisuals();
}
bool hhPlayer::PortalGunSelected() const {
    return RW_PortalGunEnabled() && idealWeapon == 1 &&
        spawnArgs.GetBool("rw_weapon_portal_selected") && !IsSpiritOrDeathwalking();
}
bool hhPlayer::PortalGunViewAvailable() const {
    return gameLocal.FindEntityDef("weaponobj_portalgun", false) != NULL;
}
void hhPlayer::SelectPortalGun(bool selected) {
    SelectWeapon(1, false);
    spawnArgs.SetBool("rw_weapon_portal_selected", selected);
    spawnArgs.SetInt("rw_weapon_portal_buttons", BUTTON_ATTACK | BUTTON_ATTACK_ALT);
    if (weapon.IsValid() && !selected) { weapon->Show(); weapon->ShowWeapon(); }
    UpdateHudWeapon();
}
static void RW_GunPortalVisual(hhPortal *portal, bool restart) {
    const bool orange = !idStr::Icmp(portal->GetName(), "rw_gun_orange");
    const char *color = orange ? "orange" : "blue";
    const bool linked = portal->cameraTarget != NULL;
    portal->spawnArgs.SetBool("rw_energy_linked", linked);
    const char *closed = linked ? "" : "_closed";
    const idStr model = va("rw_portal_%s%s_opening", color, closed);
    if ((restart || portal->spawnArgs.GetInt("rw_open_end") > gameLocal.time) &&
        declManager->FindType(DECL_MODELDEF, model, false)) {
        if (restart) portal->spawnArgs.SetInt("rw_open_start", gameLocal.time);
        portal->SetModel(model);
        const int anim = portal->GetAnimator()->GetAnim("open");
        const int start = portal->spawnArgs.GetInt("rw_open_start");
        portal->GetAnimator()->PlayAnim(ANIMCHANNEL_ALL, anim, start, 0);
        portal->spawnArgs.SetInt("rw_open_end", start + portal->GetAnimator()->AnimLength(anim));
    } else {
        portal->SetModel(va("models/reawakened/portalgun/%s%s.ase", color, closed));
    }
    if (!linked) portal->GetRenderEntity()->remoteRenderView = NULL;
    if (cvarSystem->GetCVarBool("com_fpsTrace"))
        gameLocal.Printf("PORTAL_ENERGY %s linked=%d model=%s\n", portal->GetName(), linked,
            portal->GetRenderEntity()->hModel ? portal->GetRenderEntity()->hModel->Name() : "none");
    portal->UpdateVisuals();

}

bool hhPlayer::PlaceGunPortal(int color, const idDict *shot, int *previewCandidate) {
    const bool preview = previewCandidate != NULL;
    if (preview && *previewCandidate < 0) return *previewCandidate == -2;
    if (!RW_PortalGunEnabled() || color < 0 || color > 1 ||
        health <= 0 || InVehicle() || IsSpiritOrDeathwalking() || gameLocal.inCinematic) { if (preview) *previewCandidate = -1; return false; }
    const idVec3 eye = shot ? shot->GetVector("shot_eye") : GetEyePosition();

    // firstPersonViewAxis includes Prey's local-gravity orientation.
    trace_t hit;
    idVec3 shotDirection = shot ? shot->GetVector("shot_direction") : firstPersonViewAxis[0];
    if (!preview && !shot && cvarSystem->GetCVarBool("developer") && spawnArgs.GetBool("rw_portal_test_aim")) {
        shotDirection = spawnArgs.GetVector("rw_portal_test_direction");
        spawnArgs.SetBool("rw_portal_test_aim", false);
    }
    gameLocal.clip.TracePoint(hit, eye, eye + shotDirection * 8192.0f, MASK_SOLID, this);
    if (shot && (hit.endpos - shot->GetVector("shot_target")).LengthSqr() > Square(2.0f)) {
        if (!preview) gameLocal.Printf("PORTALGUN rejected: target obstructed during flight\n"); if (preview) *previewCandidate = -1; return false;
    }
    if (!RW_PortalPlacementSurface(hit)) {
        if (!preview) gameLocal.Printf("PORTALGUN rejected: aim at a stationary world surface\n"); if (preview) *previewCandidate = -1; return false;
    }
    idVec3 normal = hit.c.normal;
    normal.Normalize();
    const idVec3 gravityUp = shot ? shot->GetVector("shot_up") : -GetPhysics()->GetGravityNormal();
    idVec3 up = gravityUp - normal * (gravityUp * normal);
    if (idMath::Fabs(gravityUp * normal) > 0.95f) {
        // Floor/ceiling ovals follow the shooter's approach, not world axes.
        up = shotDirection - normal * (shotDirection * normal);
        if (up.Normalize() < 0.1f) {
            // A vertical shot has no projected forward direction. View-left
            // retains yaw even at the pitch limit, so use it to recover heading.
            up = (shot ? shot->GetVector("shot_left") : firstPersonViewAxis[1]).Cross(gravityUp);
            up -= normal * (up * normal);
        }
    }
    up.Normalize();
    idVec3 side = up.Cross(normal); side.Normalize(); up = normal.Cross(side); up.Normalize();
    idMat3 axis(normal, side, up);
    idVec3 center = hit.endpos - normal * (hit.endpos * normal - hit.c.dist);
    bool supported = false;
    RW_PortalFloorCenter(center, axis, gravityUp, this);
    center += up * (floorf(center * up + 0.5f) - center * up);
    const idVec3 aimedCenter = center;
    hhPortal *other = RW_GunPortal(1-color), *portal = RW_GunPortal(color);
    // Search nearest first on a bounded surface-plane grid. A failed shot
    // leaves the existing endpoints and their link untouched.
    int candidateIndex = 0, tested = 0;
    for (int radiusSquared = 0; radiusSquared <= 64 && !supported; ++radiusSquared) {
        for (int z = -8; z <= 8 && !supported; ++z) for (int y = -8; y <= 8 && !supported; ++y) {
            if (y*y + z*z != radiusSquared) continue;
            if (preview && candidateIndex++ < *previewCandidate) continue;
            // Spread difficult searches across ticks instead of adding a large
            // aim-dependent hitch. Live shots keep the original full search.
            if (preview && tested++ == 8) { *previewCandidate = candidateIndex-1; return false; }
            const idVec3 candidate = aimedCenter + axis[1]*(y*8.0f) + up*(z*8.0f);
            if (RW_PortalOpeningsOverlap(candidate, axis, other)) continue;
            if (RW_PortalSurfaceSupports(candidate, axis, this)) { center = candidate; supported = true; }
            else {
                idVec3 fittedCenter = candidate;
                idMat3 fittedAxis = axis;
                if (RW_FitPortalSurface(fittedCenter, fittedAxis, this) &&
                    !RW_PortalOpeningsOverlap(fittedCenter, fittedAxis, other)) {
                    center = fittedCenter; axis = fittedAxis; normal = axis[0]; supported = true;
                    if (!preview && cvarSystem->GetCVarBool("com_fpsTrace"))
                        gameLocal.Printf("PORTAL_TERRAIN_FIT center=%s normal=%s\n", center.ToString(), normal.ToString());
                }
            }
        }
    }
    if (!supported) { if (!preview) gameLocal.Printf("PORTALGUN rejected: no nearby supported opening\n"); if (preview) *previewCandidate = -1; return false; }
    // Surface preview never clears occupants or modifies endpoints. Objects
    // and changing geometry can still invalidate a shot before its impact.
    if (preview) { *previewCandidate = -2; return true; }
    if (portal && !RW_ClearPortalOccupants(portal, other)) {
        gameLocal.Printf("PORTALGUN rejected: leave the opening clear before replacing it\n"); return false;
    }
    if (!portal) {
        idDict args;
        args.Set("classname", "object_portal"); args.Set("name", RW_PortalName(color));
        args.SetBool("rw_portalGun", true); args.SetBool("startActive", true);
        args.Set("model", "models/reawakened/portalgun/closed.ase");
        args.SetVector("mins", RW_PORTAL_ENTITY_BOUNDS[0]); args.SetVector("maxs", RW_PORTAL_ENTITY_BOUNDS[1]);
        args.Set("deformType", "0"); args.Set("shaderParm5", "100000"); args.Set("shaderParm6", "99999");
        args.SetVector("origin", center + normal * RW_PORTAL_SURFACE_OFFSET); args.SetMatrix("rotation", axis);
        idEntity *created = NULL;
        if (!gameLocal.SpawnEntityDef(args, &created) || !created || !created->IsType(hhPortal::Type)) return false;
        portal = static_cast<hhPortal *>(created);
    }
    portal->spawnArgs.SetBool("rw_portal_cover_checked", false);
    portal->ResetGunPortalCrossings();
    if (other) other->ResetGunPortalCrossings();
    portal->SetOrigin(center + normal * RW_PORTAL_SURFACE_OFFSET); portal->SetAxis(axis);
    portal->spawnArgs.SetVector("origin", portal->GetOrigin());
    portal->spawnArgs.SetMatrix("rotation", axis);
    portal->spawnArgs.SetFloat("rw_portal_surface_offset", RW_PORTAL_SURFACE_OFFSET);
    portal->spawnArgs.SetBool("rw_portal_floor_aligned", true);
    portal->GetPhysics()->SetContents(0);
    portal->SetGravity(shot && shot->GetInt("shot_hops") > 0 ?
        hhUtils::GetLocalGravity(portal->GetOrigin(), portal->GetPhysics()->GetBounds(), gameLocal.GetGravity()) : GetPhysics()->GetGravity());
    if (other) {
        portal->cameraTarget = other; other->cameraTarget = portal;
        portal->spawnArgs.Set("cameraTarget", other->GetName()); other->spawnArgs.Set("cameraTarget", portal->GetName());
        portal->GetPhysics()->SetContents(CONTENTS_SOLID); other->GetPhysics()->SetContents(CONTENTS_SOLID);
        RW_GunPortalVisual(other, false);
    }
    RW_GunPortalVisual(portal, true);
    gameLocal.Printf("PORTALGUN placed %s at %s normal %s paired=%d\n", color ? "orange" : "blue", portal->GetOrigin().ToString(), normal.ToString(), other != NULL);
    return true;
}
CLASS_DECLARATION(idEntity, hhPortalShot)
END_CLASS

void hhPortalShot::Spawn() {
    GetPhysics()->SetContents(0);
    fl.neverDormant = true;
    SetShaderParm(SHADERPARM_TIMEOFFSET, -MS2SEC(gameLocal.time));
    BecomeActive(TH_THINK | TH_UPDATEVISUALS);
}

static void RW_PortalShotImpact(const idVec3 &point, const idVec3 &normal, int color, bool success) {
    idDict args;
    args.Set("classname", "rw_portal_shot_impact");
    args.Set("model", va("rw_portal_%s_%s.prt", color ? "orange" : "blue", success ? "impact" : "reject"));
    args.SetVector("origin", point + normal * 1.5f);
    const idMat3 surface = normal.ToMat3();
    // Prey's cone particles emit along local Z.
    args.SetMatrix("rotation", idMat3(surface[1], surface[2], surface[0]));
    args.SetFloat("shaderParm4", -MS2SEC(gameLocal.time));
    idEntity *effect = NULL;
    if (gameLocal.SpawnEntityDef(args, &effect) && effect) {
        effect->GetPhysics()->SetContents(0);
        effect->PostEventMS(&EV_Remove, 1200);
    }
}

static void RW_SetPortalShotVector(idDict &args, const char *key, const idVec3 &value);
static hhPortal *RW_FindShotPortal(const idVec3 &start, const idVec3 &end, float &fraction,
    idVec3 &remote, idMat3 &rotation) {
    hhPortal *nearest = NULL;
    fraction = 1.0f;
    for (idEntity *entity = gameLocal.spawnedEntities.Next(); entity; entity = entity->spawnNode.Next()) {
        if (!entity->IsType(hhPortal::Type)) continue;
        hhPortal *portal = static_cast<hhPortal *>(entity);
        float f; idVec3 position; idMat3 transform;
        if (portal->TracePortalShot(start, end, f, position, transform) && (!nearest || f < fraction)) {
            nearest = portal; fraction = f; remote = position; rotation = transform;
        }
    }
    return nearest;
}
// Aim at the first opening, preserving eye/crosshair accuracy despite the muzzle
// offset. Further segments are acquired only when the flying shot crosses it.
static void RW_TraceShotSegment(trace_t &hit, const idVec3 &start, const idVec3 &direction,
    float range, const idEntity *owner) {
    const idVec3 end = start+direction*range;
    gameLocal.clip.TracePoint(hit, start, end, MASK_SOLID, owner);
    float fraction; idVec3 remote; idMat3 rotation;
    if (RW_FindShotPortal(start, end, fraction, remote, rotation) && fraction <= hit.fraction) {
        hit.endpos = start+(end-start)*fraction;
        hit.fraction = fraction;
    }
}

void hhPortalShot::Think() {
    idEntity *entity = gameLocal.FindEntity(spawnArgs.GetString("shot_owner"));
    hhPlayer *owner = entity && entity->IsType(hhPlayer::Type) ? static_cast<hhPlayer *>(entity) : NULL;
    const int color = spawnArgs.GetInt("shot_color");
    if (!owner || owner->health <= 0 || !g_portalGun.GetBool() ||
        owner->spawnArgs.GetInt(va("rw_shot_serial_%d", color)) != spawnArgs.GetInt("shot_serial")) {
        Hide(); BecomeInactive(TH_THINK); PostEventMS(&EV_Remove, 0); return;
    }
    int start = spawnArgs.GetInt("shot_start");
    idVec3 target;
    float fraction = 0;
    trace_t obstacle;
    bool blocked = false;
    for (int step = 0; step <= 8; ++step) {
        const int end = spawnArgs.GetInt("shot_end");
        target = spawnArgs.GetVector("shot_target");
        fraction = idMath::ClampFloat(0, 1, float(gameLocal.time-start)/Max(1,end-start));
        const idVec3 next = spawnArgs.GetVector("shot_muzzle")*(1-fraction)+target*fraction;
        gameLocal.clip.TracePoint(obstacle, GetOrigin(), next, MASK_SOLID, owner);
        float crossing; idVec3 remote; idMat3 rotation;
        hhPortal *portal = RW_FindShotPortal(GetOrigin(), next, crossing, remote, rotation);
        if (portal && crossing <= obstacle.fraction) {
            const idVec3 point = GetOrigin()+(next-GetOrigin())*crossing;
            const idVec3 segmentStart = spawnArgs.GetVector("shot_muzzle");
            const float segmentLength = (target-segmentStart).Length();
            const int crossingTime = start + int((end-start)*(point-segmentStart).Length()/Max(0.001f, segmentLength));
            const float remaining = spawnArgs.GetFloat("shot_remaining", "8192")-
                (point-spawnArgs.GetVector("shot_eye")).Length();
            const int hops = spawnArgs.GetInt("shot_hops")+1;
            if (hops > 8 || remaining <= 1) { blocked = true; obstacle.endpos = point; obstacle.c.normal = portal->GetAxis()[0]; break; }
            idVec3 direction = spawnArgs.GetVector("shot_direction")*rotation;
            direction.Normalize();
            remote += direction*0.05f;
            trace_t hit;
            RW_TraceShotSegment(hit, remote, direction, remaining, owner);
            RW_SetPortalShotVector(spawnArgs, "shot_eye", remote);
            RW_SetPortalShotVector(spawnArgs, "shot_muzzle", remote);
            RW_SetPortalShotVector(spawnArgs, "shot_direction", direction);
            RW_SetPortalShotVector(spawnArgs, "shot_target", hit.endpos);
            RW_SetPortalShotVector(spawnArgs, "shot_up", spawnArgs.GetVector("shot_up")*rotation);
            RW_SetPortalShotVector(spawnArgs, "shot_left", spawnArgs.GetVector("shot_left")*rotation);
            RW_SetPortalShotVector(spawnArgs, "shot_normal", hit.fraction < 1 ? hit.c.normal : -direction);
            spawnArgs.SetBool("shot_hit", hit.fraction < 1);
            spawnArgs.SetFloat("shot_remaining", remaining);
            spawnArgs.SetInt("shot_hops", hops);
            // Carry unused tick time across the discontinuity rather than
            // pausing for a whole simulation frame at each opening.
            start = Min(gameLocal.time, crossingTime);
            spawnArgs.SetInt("shot_start", start);
            spawnArgs.SetInt("shot_end", start+Max(16, int((hit.endpos-remote).Length()*1000.0f/4000.0f)));
            spawnArgs.SetInt("shot_trail_start", start);
            SetOrigin(remote); SetAxis(direction.ToMat3());
            gameLocal.Printf("PORTALGUN_SHOT portal=%s hops=%d\n", portal->GetName(), hops);
            continue;
        }
        blocked = obstacle.fraction < 1 && (obstacle.endpos-target).LengthSqr() > Square(2.0f);
        SetOrigin(blocked ? obstacle.endpos : next);
        break;
    }
    if (blocked) SetOrigin(obstacle.endpos);
    const char *trail = color ? "rw_portal_orange_trail" : "rw_portal_blue_trail";
    const idDeclParticle *particle = static_cast<const idDeclParticle *>(declManager->FindType(DECL_PARTICLE, trail, false));
    const int smokeStart = spawnArgs.GetInt("shot_trail_start", va("%d", start));
    if (particle && !gameLocal.smokeParticles->EmitSmoke(particle, smokeStart,
        gameLocal.random.RandomFloat(), GetOrigin(), GetAxis())) {
        spawnArgs.SetInt("shot_trail_start", gameLocal.time);
    }
    if (fraction >= 1 || blocked) {
        const bool success = !blocked && owner->PlaceGunPortal(color, &spawnArgs);
        if (blocked || spawnArgs.GetBool("shot_hit")) RW_PortalShotImpact(GetOrigin(),
            blocked ? obstacle.c.normal : spawnArgs.GetVector("shot_normal"), color, success);
        gameLocal.Printf("PORTALGUN_SHOT impact %s success=%d blocked=%d\n", color ? "orange" : "blue", success, blocked);
        Hide(); BecomeInactive(TH_THINK); PostEventMS(&EV_Remove, 0); return;
    }
    Present();
}

// idDict::SetVector rounds to two decimal places. Flight validation re-traces
// the original aim, so grazing directions must survive a float round trip.
static void RW_SetPortalShotVector(idDict &args, const char *key, const idVec3 &value) {
    args.Set(key, va("%.9g %.9g %.9g", value.x, value.y, value.z));
}
void hhPlayer::FireGunPortal(int color) {
    if (color < 0 || color > 1 || !RW_PortalGunEnabled() || health <= 0 || InVehicle() || IsSpiritOrDeathwalking() || gameLocal.inCinematic) return;
    const idVec3 eye = GetEyePosition();
    idVec3 direction = firstPersonViewAxis[0];
    if (cvarSystem->GetCVarBool("developer") && spawnArgs.GetBool("rw_portal_test_aim")) {
        direction = spawnArgs.GetVector("rw_portal_test_direction");
        spawnArgs.SetBool("rw_portal_test_aim", false);
    }
    trace_t hit;
    RW_TraceShotSegment(hit, eye, direction, 8192.0f, this);
    idVec3 muzzle = eye + firstPersonViewAxis[0]*14 - firstPersonViewAxis[1]*6 - firstPersonViewAxis[2]*5;
    if (weapon.IsValid() && PortalGunViewAvailable()) {
        const jointHandle_t joint = weapon->GetAnimator()->GetJointHandle("ValveBiped.Front_Cover");
        idMat3 axis;
        if (joint != INVALID_JOINT && weapon->idAnimatedEntity::GetJointWorldTransform(joint, gameLocal.time, muzzle, axis))
            muzzle += idVec3(0,2.2f,2.8f)*axis;
    }
    trace_t clearance;
    gameLocal.clip.TracePoint(clearance, eye, muzzle, MASK_SOLID, this);
    // The weapon's presentation can still be on the previous side on the
    // teleport tick. Never start a flight at that stale, distant transform.
    if (clearance.fraction < 1 || (muzzle-eye).LengthSqr() > Square(96.0f)) muzzle = eye;
    const int serial = spawnArgs.GetInt(va("rw_shot_serial_%d", color)) + 1;
    idDict args;
    args.Set("classname", "rw_portal_shot");
    args.Set("model", color ? "rw_portal_orange_flight.prt" : "rw_portal_blue_flight.prt");
    args.SetVector("origin", muzzle);
    args.SetMatrix("rotation", direction.ToMat3());
    args.Set("shot_owner", GetName());
    args.SetInt("shot_color", color); args.SetInt("shot_serial", serial);
    args.SetInt("shot_start", gameLocal.time);
    args.SetInt("shot_end", gameLocal.time + Max(16, int((hit.endpos-muzzle).Length()*1000.0f/4000.0f)));
    RW_SetPortalShotVector(args, "shot_muzzle", muzzle); RW_SetPortalShotVector(args, "shot_eye", eye);
    RW_SetPortalShotVector(args, "shot_direction", direction); RW_SetPortalShotVector(args, "shot_target", hit.endpos);
    RW_SetPortalShotVector(args, "shot_up", -GetPhysics()->GetGravityNormal());
    RW_SetPortalShotVector(args, "shot_left", firstPersonViewAxis[1]);
    RW_SetPortalShotVector(args, "shot_normal", hit.fraction < 1 ? hit.c.normal : -direction);
    args.SetBool("shot_hit", hit.fraction < 1);
    idEntity *shot = NULL;
    if (gameLocal.SpawnEntityDef(args, &shot) && shot) {
        spawnArgs.SetInt(va("rw_shot_serial_%d", color), serial);
        spawnArgs.SetInt("rw_portal_view_fire", color+1);
        gameLocal.Printf("PORTALGUN_SHOT launch %s travel=%d\n", color ? "orange" : "blue", args.GetInt("shot_end")-gameLocal.time);
    }
}

void hhPlayer::UpdatePortalGun() {
    if (!g_portalGun.GetBool() && spawnArgs.GetBool("rw_weapon_portal_selected")) {
        spawnArgs.SetBool("rw_weapon_portal_selected", false);
        if (weapon.IsValid()) weapon->ShowWeapon();
    }
    const bool selected = PortalGunSelected() && currentWeapon == 1;
    // The tool remains a variant of slot 1; no weapon-array/save-layout change.
    // Replace its presentation entity when toggling between wrench and tool.
    if (idealWeapon == 1 && currentWeapon == 1 && weapon.IsValid()) {
        const char *desired = selected && PortalGunViewAvailable() ? "weaponobj_portalgun" : spawnArgs.GetString("def_weapon1");
        if (idStr::Icmp(weapon->spawnArgs.GetString("classname"), desired)) {
            SAFE_REMOVE(weapon);
            weapon = SpawnWeapon(desired);
            animPrefix = "wrench";
            weapon->Raise();
            spawnArgs.SetInt("rw_portal_view_anim_end", 0);
            if (cvarSystem->GetCVarBool("developer")) gameLocal.Printf("PORTALGUN_VIEW selected %s\n", desired);
        }
    }
    const int buttons = usercmd.buttons & (BUTTON_ATTACK | BUTTON_ATTACK_ALT);
    const int previous = spawnArgs.GetInt("rw_weapon_portal_buttons");
    spawnArgs.SetInt("rw_weapon_portal_buttons", buttons);
    if (!selected) return;
    // UpdateWeapon calls Weapon_GUI after this function. Preserve the real
    // press/release edges for the screen and Tommy's separate GUI hand.
    // Still track raw buttons above so leaving a screen while holding fire
    // cannot turn that same click into an unintended portal shot.
    if (ActiveGui()) return;
    if (!gameLocal.inCinematic && !bFrozen && health > 0) {
        if ((buttons & BUTTON_ATTACK) && !(previous & BUTTON_ATTACK)) {
            FireGunPortal(0);
        }
        if ((buttons & BUTTON_ATTACK_ALT) && !(previous & BUTTON_ATTACK_ALT)) {
            FireGunPortal(1);
        }
    }
    usercmd.buttons &= ~(BUTTON_ATTACK | BUTTON_ATTACK_ALT);
}
void hhPlayer::UpdatePortalGunView() {
    const int shot = spawnArgs.GetInt("rw_portal_view_fire");
    spawnArgs.SetInt("rw_portal_view_fire", 0);
    if (!weapon.IsValid() || !PortalGunSelected() || currentWeapon != 1) return;
    if (!PortalGunViewAvailable()) { weapon->Hide(); return; }
    idAnimator *animator = weapon->GetAnimator();
    if (!animator || idStr::Icmp(weapon->spawnArgs.GetString("classname"), "weaponobj_portalgun")) { weapon->Hide(); return; }
    if (shot) {
        const int anim = animator->GetAnim("fire");
        animator->PlayAnim(ANIMCHANNEL_ALL, anim, gameLocal.time, 0);
        spawnArgs.SetInt("rw_portal_view_anim_end", gameLocal.time + animator->AnimLength(anim));
        spawnArgs.SetInt("rw_portal_view_last_color", shot == 2 ? 1 : 0);
        spawnArgs.SetInt("rw_portal_view_flash_end", gameLocal.time + 200);
        weapon->StartSound(shot == 2 ? "snd_altfire" : "snd_fire", SND_CHANNEL_WEAPON, 0, false, NULL);
        if (cvarSystem->GetCVarBool("developer")) gameLocal.Printf("PORTALGUN_VIEW fire %s\n", shot == 2 ? "orange" : "blue");
    } else if (spawnArgs.GetInt("rw_portal_view_anim_end") > 0 &&
               gameLocal.time >= spawnArgs.GetInt("rw_portal_view_anim_end") && weapon->IsReady()) {
        animator->CycleAnim(ANIMCHANNEL_ALL, animator->GetAnim("idle"), gameLocal.time, 80);
        spawnArgs.SetInt("rw_portal_view_anim_end", 0);
    }
    // Keep the color through weapon switching and old/new saves, without
    // changing the save layout. Effects use elapsed time, not rendered frames.
    if (!spawnArgs.FindKey("rw_portal_view_last_color")) {
        spawnArgs.SetInt("rw_portal_view_last_color", weapon->GetRenderEntity()->shaderParms[5] >= 0.5f ? 1 : 0);
    }
    weapon->SetShaderParm(5, spawnArgs.GetInt("rw_portal_view_last_color") ? 1.0f : 0.0f);
    weapon->SetShaderParm(6, idMath::ClampFloat(0, 1,
        (spawnArgs.GetInt("rw_portal_view_flash_end") - gameLocal.time) / 200.0f));
}


void hhPlayer::UpdatePortalGunReticle() {
    if (!g_portalGunReticle.GetBool() || !g_showHud.GetBool() || !g_crosshair.GetInteger() ||
        pm_thirdPerson.GetBool() || privateCameraView || !PortalGunSelected() || currentWeapon != 1 ||
        health <= 0 || InVehicle() || InCinematic() || ActiveGui() || GuiActive()) {
        portalReticleRefresh = -1;
        portalReticleValid[0] = portalReticleValid[1] = false;
        return;
    }
    const idVec3 eye = GetEyePosition();
    const bool moved = portalReticleRefresh < 0 || !eye.Compare(portalReticleEye, 0.05f) ||
        !firstPersonViewAxis.Compare(portalReticleAxis, 0.001f);
    const bool finished = portalReticleCandidate[0] < 0 && portalReticleCandidate[1] < 0;
    if (moved || gameLocal.time < portalReticleRefresh || (finished && gameLocal.time >= portalReticleRefresh + 100)) {
        portalReticleEye = eye; portalReticleAxis = firstPersonViewAxis;
        portalReticleRefresh = gameLocal.time;
        portalReticleCandidate[0] = portalReticleCandidate[1] = 0;
        if (moved) portalReticleValid[0] = portalReticleValid[1] = false;
    } else if (finished) return;

    // Follow the same surfaces and linked openings as the projectile, carrying
    // its remaining range and gravity-relative orientation through each link.
    idVec3 start = eye, direction = firstPersonViewAxis[0];
    idVec3 up = -GetPhysics()->GetGravityNormal(), left = firstPersonViewAxis[1];
    float remaining = 8192.0f;
    trace_t hit;
    bool targetFound = false;
    for (int hop = 0; hop <= 8; ++hop) {
        const idVec3 end = start + direction*remaining;
        gameLocal.clip.TracePoint(hit, start, end, MASK_SOLID, this);
        float fraction; idVec3 remote; idMat3 rotation;
        hhPortal *portal = RW_FindShotPortal(start, end, fraction, remote, rotation);
        if (!portal || fraction > hit.fraction) { targetFound = true; break; }
        remaining *= 1.0f-fraction;
        if (hop == 8 || remaining <= 1) break;
        direction *= rotation; direction.Normalize();
        up *= rotation; left *= rotation;
        start = remote + direction*0.05f;
    }
    if (!targetFound || hit.fraction >= 1) {
        portalReticleValid[0] = portalReticleValid[1] = false;
        portalReticleCandidate[0] = portalReticleCandidate[1] = -1;
    } else {
        idDict aim;
        RW_SetPortalShotVector(aim, "shot_eye", start);
        RW_SetPortalShotVector(aim, "shot_direction", direction);
        RW_SetPortalShotVector(aim, "shot_up", up);
        RW_SetPortalShotVector(aim, "shot_left", left);
        RW_SetPortalShotVector(aim, "shot_target", hit.endpos);
        for (int color = 0; color < 2; ++color) {
            if (portalReticleCandidate[color] < 0) continue;
            const bool valid = PlaceGunPortal(color, &aim, &portalReticleCandidate[color]);
            if (portalReticleCandidate[color] < 0) portalReticleValid[color] = valid;
        }
    }
    if (g_portalReticleTrace.GetBool() && portalReticleCandidate[0] < 0 && portalReticleCandidate[1] < 0)
        gameLocal.Printf("PORTAL_RETICLE blue=%d orange=%d\n", portalReticleValid[0], portalReticleValid[1]);
}

bool hhPlayer::DrawPortalGunReticle() {
    if (!g_portalGunReticle.GetBool() || !PortalGunSelected() || currentWeapon != 1 ||
        !g_crosshair.GetInteger() || privateCameraView || IsLocked(idealWeapon) ||
        (hand.IsValid() && !hand->IsLowered()) || InCinematic() || InVehicle() || health <= 0 || !weapon.IsValid()) return false;
    const idMaterial *atlas = declManager->FindMaterial("guis/assets/portalgun/reticle", false);
    if (!atlas || atlas->GetState() == DS_DEFAULTED) return false;
    const idVec2 offset = gameLocal.GetPresentationCursorOffset();
    const float aspect = (4.0f/3.0f) * renderSystem->GetScreenHeight()/Max(1,renderSystem->GetScreenWidth());
    const float cx = 320 + offset.x, cy = 240 + offset.y;
    const float h = 24, w = h*44.0f/64.0f;
    for (int color = 0; color < 2; ++color) {
        const idVec3 rgb = color ? idVec3(1.0f,0.55f,0.12f) : idVec3(0.15f,0.65f,1.0f);
        renderSystem->SetColor4(rgb.x,rgb.y,rgb.z,0.9f);
        const float u = (portalReticleValid[color] ? 98.0f : 2.0f) + color*48.0f;
        renderSystem->DrawStretchPic(cx + (color ? -0.35f : -0.64f)*w*aspect,
            cy-h*0.5f + (color ? 0.17f : -0.17f)*h, w*aspect,h, u/256,0,(u+44)/256,1,atlas);
        // The small outer oval identifies the last fired color separately from
        // surface validity. Use the existing saved gun-color state.
        const bool fired = spawnArgs.GetInt("rw_shot_serial_0") > 0 || spawnArgs.GetInt("rw_shot_serial_1") > 0;
        const bool last = fired && spawnArgs.GetInt("rw_portal_view_last_color") == color;
        if (!last) continue;
        const float flash = idMath::ClampFloat(0,1,(spawnArgs.GetInt("rw_portal_view_flash_end")-gameLocal.time)/200.0f);
        renderSystem->SetColor4(rgb.x,rgb.y,rgb.z,0.75f+0.25f*flash);
        const float markerW = h*28.0f/64.0f;
        renderSystem->DrawStretchPic(cx+(color ? 0.75f : -1.85f)*markerW*aspect,
            cy-h*0.5f,markerW*aspect,h,194.0f/256,0,222.0f/256,1,atlas);
    }
    renderSystem->SetColor4(1,1,1,0.95f);
    renderSystem->DrawStretchPic(cx-0.65f*aspect,cy-0.65f,1.3f*aspect,1.3f,0,0,1,1,declManager->FindMaterial("_white"));
    renderSystem->SetColor4(1,1,1,1);
    return true;
}
