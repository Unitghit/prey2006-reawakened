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
static bool RW_PortalFits(const hhPortal *portal, const idTraceModel *trm, const idMat3 &axis, const idVec3 &origin) {
    if (!trm) return false;
    // Upright wall portals must not turn the oval's narrowing lower edge into
    // a step under the player's square collision hull. Keep a flat foot opening
    // with one step-height of clearance, while retaining the curved sides/top.
    // Floor/ceiling portals keep the original aperture test in their own plane.
    const bool footClearance = axis[2] * portal->GetAxis()[2] > 0.95f;
    for (int i = 0; i < 8; ++i) {
        const idVec3 corner(trm->bounds[(i&1)!=0].x, trm->bounds[(i&2)!=0].y, trm->bounds[(i&4)!=0].z);
        const idVec3 local = (origin + corner * axis - portal->GetOrigin()) * portal->GetAxis().Transpose();
        float apertureZ = local.z;
        if (footClearance && apertureZ < -48.0f) {
            if (apertureZ < -87.0f) return false;
            apertureZ = -48.0f;
        }
        if (Square(local.y / 47.0f) + Square(apertureZ / 71.0f) > 1.0f) return false;
    }
    return true;
}
bool RW_PortalClipPlane(const idEntity *entity, const idTraceModel *trm, const idMat3 &axis,
    const idVec3 &start, const idVec3 &end, idPlane &plane, float &limit) {
    limit = 1.0f;
    if (!g_portalGun.GetBool() || gameLocal.isMultiplayer || *cvarSystem->GetCVarString("fs_game") || !entity || !entity->IsType(hhPlayer::Type) || !trm) return false;
    hhPortal *a = RW_GunPortal(0), *b = RW_GunPortal(1);
    if (!a || !b || !a->cameraTarget || !b->cameraTarget) return false;
    for (int i = 0; i < 2; ++i) {
        hhPortal *portal = i ? b : a;
        const idVec3 normal = portal->GetAxis()[0];
        const idVec3 actualOffset = entity->GetPhysics()->GetOrigin() - portal->GetOrigin();
        if (actualOffset.LengthSqr() < 128*128 && actualOffset * normal < -4.0f) continue;
        const float d0 = (start - portal->GetOrigin()) * normal;
        const float d1 = (end - portal->GetOrigin()) * normal;
        // Never open unrelated, remote or backside world geometry.
        if (d0 < -90 || d0 > 90 || d1 < -90 || d1 > 90) continue;
        if (!RW_PortalFits(portal, trm, axis, start)) continue;
        if (!RW_PortalFits(portal, trm, axis, end)) {
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
                if (RW_PortalFits(portal, trm, axis, start + (end-start)*mid)) low = mid; else high = mid;
            }
            limit = Max(0.0f, low - 0.001f);
        }
        plane.SetNormal(normal);
        plane.FitThroughPoint(portal->GetOrigin() - normal * portal->spawnArgs.GetFloat("rw_portal_surface_offset"));
        return true;
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
    gameLocal.clip.TracePoint(floor, probe, probe - axis[2]*100, MASK_SOLID, ignore);
    if (floor.fraction >= 1 || floor.c.entityNum != ENTITYNUM_WORLD || floor.c.normal * axis[2] < 0.99f) return false;
    const float height = (center - floor.endpos) * axis[2];
    if (height < 56 || height > 96) return false;
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
    idVec3 up = -GetPhysics()->GetGravityNormal();
    up -= normal * (up * normal);
    if (up.Normalize() < 0.1f) {
        up = firstPersonViewAxis[0] - normal * (firstPersonViewAxis[0] * normal);
        if (up.Normalize() < 0.1f) up = firstPersonViewAxis[2];
        up -= normal * (up * normal);
        if (up.Normalize() < 0.1f) { up.Set(1, 0, 0); up -= normal*(up*normal); up.Normalize(); }
    }
    idVec3 side = up.Cross(normal); side.Normalize(); up = normal.Cross(side); up.Normalize();
    const idMat3 axis(normal, side, up);
    idVec3 center = hit.endpos - normal * (hit.endpos * normal - hit.c.dist);
    bool supported = false;
    RW_PortalFloorCenter(center, axis, -GetPhysics()->GetGravityNormal(), this);
    const idVec3 aimedCenter = center;
    for (int adjustment = 0; adjustment <= 4 && !supported; ++adjustment) {
        center = aimedCenter + up * (adjustment * 8.0f);
        center += up * (floorf(center * up + 0.5f) - center * up);
        supported = RW_PortalSurfaceSupports(center, axis, this);
    }
    if (!supported) { gameLocal.Printf("PORTALGUN rejected: opening does not fit flat surface\n"); return false; }
    hhPortal *other = RW_GunPortal(1-color), *portal = RW_GunPortal(color);
    if (other && (other->GetOrigin()-center).Length() < 160) {
        gameLocal.Printf("PORTALGUN rejected: too close to other endpoint\n"); return false;
    }
    if ((portal && (GetOrigin()-portal->GetOrigin()).Length() < 120) ||
        (portal && other && (GetOrigin()-other->GetOrigin()).Length() < 120)) {
        gameLocal.Printf("PORTALGUN rejected: leave the opening before replacing it\n"); return false;
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
    portal->SetOrigin(center + normal * RW_PORTAL_SURFACE_OFFSET); portal->SetAxis(axis);
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
    const int buttons = usercmd.buttons & (BUTTON_ATTACK | BUTTON_ATTACK_ALT);
    const int previous = spawnArgs.GetInt("rw_weapon_portal_buttons");
    spawnArgs.SetInt("rw_weapon_portal_buttons", buttons);
    if (!selected) return;
    if (!ActiveGui() && !gameLocal.inCinematic && !bFrozen && health > 0) {
        if ((buttons & BUTTON_ATTACK) && !(previous & BUTTON_ATTACK)) PlaceGunPortal(0);
        if ((buttons & BUTTON_ATTACK_ALT) && !(previous & BUTTON_ATTACK_ALT)) PlaceGunPortal(1);
    }
    usercmd.buttons &= ~(BUTTON_ATTACK | BUTTON_ATTACK_ALT);
}
