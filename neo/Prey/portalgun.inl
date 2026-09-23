// Experimental single-player portal tool. State uses existing entity/player save dictionaries.
static idCVar g_portalGun("g_portalGun", "0", CVAR_GAME | CVAR_BOOL | CVAR_ARCHIVE, "experimental slot-1 blue/orange portal tool");
// Keep the opening ahead of thin wall-decoration layers, without separating
// the visible aperture from the plane that actually teleports the player.
static const float RW_PORTAL_SURFACE_OFFSET = 1.0f;
static const char *RW_PortalName(int color) { return color ? "rw_gun_orange" : "rw_gun_blue"; }
static hhPortal *RW_GunPortal(int color) {
    idEntity *ent = gameLocal.FindEntity(RW_PortalName(color));
    return ent && ent->IsType(hhPortal::Type) ? static_cast<hhPortal *>(ent) : NULL;
}
// Defer replacement while an eligible hull straddles either opening.
static bool RW_PortalOccupied(const hhPortal *portal, const idPhysics *physics) {
    if (!portal) return false;
    idBounds local;
    local.Clear();
    const idBounds &bounds = physics->GetBounds();
    for (int i = 0; i < 8; ++i) {
        const idVec3 corner(bounds[(i&1)!=0].x, bounds[(i&2)!=0].y, bounds[(i&4)!=0].z);
        local.AddPoint((physics->GetOrigin() + corner * physics->GetAxis() - portal->GetOrigin()) * portal->GetAxis().Transpose());
    }
    return local.IntersectsBounds(idBounds(idVec3(-2, -49, -87), idVec3(2, 49, 73)));
}
static bool RW_GunPortalEntity(const idEntity *ent) {
    return ent && !ent->fl.noPortal && !ent->IsBound() &&
        (ent->IsType(hhPlayer::Type) || ent->IsType(hhProjectile::Type) || ent->IsType(idMoveable::Type));
}
static bool RW_PortalHasOccupants(const hhPortal *portal) {
    if (!portal) return false;
    idEntity *entities[MAX_GENTITIES];
    const int count = gameLocal.clip.EntitiesTouchingBounds(portal->GetPhysics()->GetAbsBounds().Expand(16), -1, entities, MAX_GENTITIES);
    for (int i = 0; i < count; ++i)
        if (RW_GunPortalEntity(entities[i]) && RW_PortalOccupied(portal, entities[i]->GetPhysics())) return true;
    return false;
}

// Simulation-time approach assistance, independent of render rate and camera
// interpolation. Strong player input and fast lateral travel always take priority.
void RW_AssistPortalFall(const idEntity *entity, const idVec3 &origin, const idVec3 &look,
    int forwardInput, int sideInput, float dt, idVec3 &velocity) {
    if (!g_portalGun.GetBool() || gameLocal.isMultiplayer || *cvarSystem->GetCVarString("fs_game") ||
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

static void RW_AssistFloorExit(idEntity *entity, idEntity *destination, const idVec3 &origin,
    const idMat3 &axis, idVec3 &velocity) {
    if (!entity->IsType(hhPlayer::Type)) return;
    idVec3 up = -destination->GetGravity();
    const float gravity = up.Normalize();
    const idVec3 normal = destination->GetAxis()[0];
    if (gravity < 0.01f || normal * up < 0.95f) return;
    const float acceleration = gravity * (normal * up);
    const float maxMinimum = idMath::Sqrt(2 * acceleration * 24);
    const float outgoing = velocity * normal;
    if (outgoing >= maxMinimum) return;
    trace_t clearance;
    gameLocal.clip.Translation(clearance, origin, origin + normal*26, entity->GetPhysics()->GetClipModel(),
        axis, entity->GetPhysics()->GetClipMask(), entity);
    const float space = Min(24.0f, (clearance.endpos-origin)*normal - 2);
    if (space < 4) return;
    const float minimum = idMath::Sqrt(2 * acceleration * space);
    if (outgoing >= minimum) return;
    velocity += normal * (minimum - outgoing);
    entity->GetPhysics()->SetLinearVelocity(velocity);
    if (cvarSystem->GetCVarBool("com_fpsTrace"))
        gameLocal.Printf("PORTAL_FLOOR_EXIT before=%.3f after=%.3f clearance=%.3f\n", outgoing, minimum, space);
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
            local.y -= idMath::ClampFloat(-8, 8, local.y - hullCenter.y);
            local.z -= idMath::ClampFloat(-8, 8, local.z - hullCenter.z);
        }
        float apertureZ = local.z;
        if (footClearance && apertureZ < -48.0f) {
            if (apertureZ < -105.0f) return false;
            apertureZ = -48.0f;
        }
        if (Square(local.y / 47.0f) + Square(apertureZ / 71.0f) > 1.0f) return false;
    }
    return true;
}
bool RW_PortalClipPlane(const idEntity *entity, const idTraceModel *trm, const idMat3 &axis,
    const idVec3 &start, const idVec3 &end, idPlane &plane, float &limit) {
    limit = 1.0f;
    if (!g_portalGun.GetBool() || gameLocal.isMultiplayer || *cvarSystem->GetCVarString("fs_game") || !RW_GunPortalEntity(entity) || !trm) return false;
    hhPortal *a = RW_GunPortal(0), *b = RW_GunPortal(1);
    if (!a || !b || !a->cameraTarget || !b->cameraTarget) return false;
    for (int i = 0; i < 2; ++i) {
        hhPortal *portal = i ? b : a;
        const idVec3 normal = portal->GetAxis()[0];
        const idVec3 actualOffset = entity->GetPhysics()->GetOrigin() - portal->GetOrigin();
        if (actualOffset.LengthSqr() < 128*128 && actualOffset * normal < -4.0f) continue;
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
        if (d0 < -90 || d0 > 90 || d1 < -90 || d1 > 90) continue;
        if (!RW_PortalFits(portal, trm, axis, start, entity->IsType(hhPlayer::Type))) continue;
        if (!RW_PortalFits(portal, trm, axis, end, entity->IsType(hhPlayer::Type))) {
            // Once a hull straddles the wall, the oval edge must be a real
            // collision boundary, including during the player's step-down trace.
            float minDepth = 1e9f, maxDepth = -1e9f;
            for (int k = 0; k < 8; ++k) {
                const idVec3 corner(trm->bounds[(k&1)!=0].x, trm->bounds[(k&2)!=0].y, trm->bounds[(k&4)!=0].z);
                const float depth = d0 + (corner * axis) * normal;
                minDepth = Min(minDepth, depth); maxDepth = Max(maxDepth, depth);
            }
            if (minDepth > 0.5f || maxDepth < -0.5f) continue;
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
        const idVec3 surface = portal->GetOrigin() - portal->GetAxis()[0] * RW_PORTAL_SURFACE_OFFSET;
        if (idMath::Fabs(wall.Distance(surface)) > 0.15f) continue;
        const float distance = (query - portal->GetOrigin()).LengthSqr();
        if (distance < nearestDistance) { nearest = portal; nearestDistance = distance; }
    }
    if (nearest) {
        hhPortal *portal = nearest;
        const idVec3 surface = portal->GetOrigin() - portal->GetAxis()[0] * RW_PORTAL_SURFACE_OFFSET;
        if (!portal->spawnArgs.GetBool("rw_portal_cover_checked")) {
            trace_t trace;
            gameLocal.clip.TracePoint(trace, surface + wall.Normal()*32, surface - wall.Normal(), CONTENTS_PLAYERCLIP, NULL);
            const bool valid = trace.fraction < 1 && trace.c.entityNum == ENTITYNUM_WORLD && trace.c.normal * wall.Normal() > 0.95f;
            portal->spawnArgs.SetBool("rw_portal_cover_checked", true);
            portal->spawnArgs.SetBool("rw_portal_cover_valid", valid);
            if (valid) {
                portal->spawnArgs.SetVector("rw_portal_cover_normal", trace.c.normal);
                portal->spawnArgs.SetFloat("rw_portal_cover_dist", trace.c.dist);
            }
        }
        if (portal->spawnArgs.GetBool("rw_portal_cover_valid")) {
            cover.SetNormal(portal->spawnArgs.GetVector("rw_portal_cover_normal"));
            cover.SetDist(portal->spawnArgs.GetFloat("rw_portal_cover_dist"));
            return true;
        }
    }
    return false;
}
// A wall portal close to a floor should be walk-through, not a raised hoop.
// This adjusts portal placement, never the player's origin or teleport velocity.
static bool RW_PortalSurfaceSupports(const idVec3 &center, const idMat3 &axis, const idEntity *ignore) {
    for (int y = -6; y <= 6; ++y) for (int z = -9; z <= 9; ++z) {
        const float py = y * 8.0f, pz = z * 8.0f;
        if (Square(py / 49.0f) + Square(pz / 73.0f) > 1.05f) continue;
        const idVec3 sample = center + axis[1] * py + axis[2] * pz;
        trace_t support;
        gameLocal.clip.TracePoint(support, sample + axis[0]*2, sample - axis[0]*2, MASK_SOLID, ignore);
        if (support.fraction >= 1 || support.c.entityNum != ENTITYNUM_WORLD ||
            support.c.normal * axis[0] < 0.999f || idMath::Fabs(center * support.c.normal - support.c.dist) > 0.1f) return false;
    }
    return true;
}
static bool RW_PortalFloorCenter(idVec3 &center, const idMat3 &axis, const idVec3 &gravityUp, const idEntity *ignore) {
    if (axis[2] * gravityUp < 0.95f) return false;
    trace_t floor;
    const idVec3 probe = center + axis[0] * 24;
    gameLocal.clip.TracePoint(floor, probe, probe - axis[2]*128, MASK_SOLID, ignore);
    if (floor.fraction >= 1 || floor.c.entityNum != ENTITYNUM_WORLD || floor.c.normal * axis[2] < 0.99f) return false;
    const float height = (center - floor.endpos) * axis[2];
    if (height < 56 || height > 120) return false;
    // The floor trace stops a clip epsilon above the actual plane. Use the
    // plane itself so two endpoints over the same floor align exactly.
    const float planeHeight = (center * floor.c.normal - floor.c.dist) / (axis[2] * floor.c.normal);
    const idVec3 candidate = center + axis[2] * (73.0f - planeHeight);
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
    return g_portalGun.GetBool() && !gameLocal.isMultiplayer && !*cvarSystem->GetCVarString("fs_game") && idealWeapon == 1 &&
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
bool hhPlayer::PlaceGunPortal(int color) {
    if (!g_portalGun.GetBool() || gameLocal.isMultiplayer || *cvarSystem->GetCVarString("fs_game") || color < 0 || color > 1 ||
        health <= 0 || InVehicle() || IsSpiritOrDeathwalking() || gameLocal.inCinematic) return false;
    const idVec3 eye = GetEyePosition();

    // firstPersonViewAxis includes Prey's local-gravity orientation.
    trace_t hit;
    idVec3 shotDirection = firstPersonViewAxis[0];
    if (cvarSystem->GetCVarBool("developer") && spawnArgs.GetBool("rw_portal_test_aim")) {
        shotDirection = spawnArgs.GetVector("rw_portal_test_direction");
        spawnArgs.SetBool("rw_portal_test_aim", false);
    }
    gameLocal.clip.TracePoint(hit, eye, eye + shotDirection * 8192.0f, MASK_SOLID, this);
    if (hit.fraction >= 1 || hit.c.entityNum != ENTITYNUM_WORLD) {
        gameLocal.Printf("PORTALGUN rejected: aim at a stationary world surface\n"); return false;
    }
    idVec3 normal = hit.c.normal;
    normal.Normalize();
    const idVec3 gravityUp = -GetPhysics()->GetGravityNormal();
    idVec3 up = gravityUp - normal * (gravityUp * normal);
    if (idMath::Fabs(gravityUp * normal) > 0.95f) {
        // Floor/ceiling ovals follow the shooter's approach, not world axes.
        up = shotDirection - normal * (shotDirection * normal);
        if (up.Normalize() < 0.1f) {
            // A vertical shot has no projected forward direction. View-left
            // retains yaw even at the pitch limit, so use it to recover heading.
            up = firstPersonViewAxis[1].Cross(gravityUp);
            up -= normal * (up * normal);
        }
    }
    up.Normalize();
    idVec3 side = up.Cross(normal); side.Normalize(); up = normal.Cross(side); up.Normalize();
    const idMat3 axis(normal, side, up);
    idVec3 center = hit.endpos - normal * (hit.endpos * normal - hit.c.dist);
    bool supported = false;
    RW_PortalFloorCenter(center, axis, -GetPhysics()->GetGravityNormal(), this);
    center += up * (floorf(center * up + 0.5f) - center * up);
    const idVec3 aimedCenter = center;
    hhPortal *other = RW_GunPortal(1-color), *portal = RW_GunPortal(color);
    // Search nearest first on a bounded surface-plane grid. A failed shot
    // leaves the existing endpoints and their link untouched.
    for (int radiusSquared = 0; radiusSquared <= 36 && !supported; ++radiusSquared) {
        for (int z = -6; z <= 6 && !supported; ++z) for (int y = -6; y <= 6 && !supported; ++y) {
            if (y*y + z*z != radiusSquared) continue;
            const idVec3 candidate = aimedCenter + axis[1]*(y*8.0f) + up*(z*8.0f);
            if (other && (other->GetOrigin()-candidate).LengthSqr() < Square(160.0f)) continue;
            if (RW_PortalSurfaceSupports(candidate, axis, this)) { center = candidate; supported = true; }
        }
    }
    if (!supported) { gameLocal.Printf("PORTALGUN rejected: no nearby supported opening\n"); return false; }
    if (portal && (RW_PortalOccupied(portal, GetPhysics()) || RW_PortalOccupied(other, GetPhysics()) ||
        RW_PortalHasOccupants(portal) || RW_PortalHasOccupants(other))) {
        gameLocal.Printf("PORTALGUN rejected: leave the opening clear before replacing it\n"); return false;
    }
    if (!portal) {
        idDict args;
        args.Set("classname", "object_portal"); args.Set("name", RW_PortalName(color));
        args.SetBool("rw_portalGun", true); args.SetBool("startActive", true);
        args.Set("model", "models/reawakened/portalgun/closed.ase");
        args.Set("mins", "-4 -48 -72"); args.Set("maxs", "4 48 72");
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
    portal->SetModel(color ? "models/reawakened/portalgun/orange_closed.ase" : "models/reawakened/portalgun/blue_closed.ase");
    portal->GetPhysics()->SetContents(0);
    portal->SetGravity(GetPhysics()->GetGravity());
    if (other) {
        portal->cameraTarget = other; other->cameraTarget = portal;
        portal->spawnArgs.Set("cameraTarget", other->GetName()); other->spawnArgs.Set("cameraTarget", portal->GetName());
        portal->SetModel(color ? "models/reawakened/portalgun/orange.ase" : "models/reawakened/portalgun/blue.ase");
        other->SetModel(color ? "models/reawakened/portalgun/blue.ase" : "models/reawakened/portalgun/orange.ase");
        portal->GetPhysics()->SetContents(CONTENTS_SOLID); other->GetPhysics()->SetContents(CONTENTS_SOLID);
        other->UpdateVisuals();
    }
    portal->UpdateVisuals();
    gameLocal.Printf("PORTALGUN placed %s at %s normal %s paired=%d\n", color ? "orange" : "blue", portal->GetOrigin().ToString(), normal.ToString(), other != NULL);
    return true;
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
    if (!ActiveGui() && !gameLocal.inCinematic && !bFrozen && health > 0) {
        if ((buttons & BUTTON_ATTACK) && !(previous & BUTTON_ATTACK)) {
            PlaceGunPortal(0);
            spawnArgs.SetInt("rw_portal_view_fire", 1);
        }
        if ((buttons & BUTTON_ATTACK_ALT) && !(previous & BUTTON_ATTACK_ALT)) {
            PlaceGunPortal(1);
            spawnArgs.SetInt("rw_portal_view_fire", 2);
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
        weapon->SetShaderParm(5, shot == 2 ? 1.0f : 0.0f);
        weapon->StartSound(shot == 2 ? "snd_altfire" : "snd_fire", SND_CHANNEL_WEAPON, 0, false, NULL);
        if (cvarSystem->GetCVarBool("developer")) gameLocal.Printf("PORTALGUN_VIEW fire %s\n", shot == 2 ? "orange" : "blue");
    } else if (spawnArgs.GetInt("rw_portal_view_anim_end") > 0 &&
               gameLocal.time >= spawnArgs.GetInt("rw_portal_view_anim_end") && weapon->IsReady()) {
        animator->CycleAnim(ANIMCHANNEL_ALL, animator->GetAnim("idle"), gameLocal.time, 80);
        spawnArgs.SetInt("rw_portal_view_anim_end", 0);
    }
}
