// Optional light-contribution prototype. Renderer copies only; no simulation edits.
#ifndef __PREY_PORTAL_LIGHTING_H__
#define __PREY_PORTAL_LIGHTING_H__
static idCVar g_portalWeaponLighting("g_portalWeaponLighting", "0", CVAR_GAME | CVAR_BOOL,
    "experimental distance-based weapon lighting across portals");
static idCVar g_portalLighter("g_portalLighter", "1", CVAR_GAME | CVAR_BOOL,
    "project the lighter through nearby portals (one hop, at most four lights)");
static idList<int> portalLighterHandles;
static idList<int> portalWeaponLightHandles;
static idFile *portalWeaponLightTrace = NULL;

static void ClearPortalWeaponLights() {
    for (int i = 0; i < portalWeaponLightHandles.Num(); ++i)
        gameRenderWorld->FreeLightDef(portalWeaponLightHandles[i]);
    portalWeaponLightHandles.Clear();
    for (int i = 0; i < portalLighterHandles.Num(); ++i)
        gameRenderWorld->FreeLightDef(portalLighterHandles[i]);
    portalLighterHandles.Clear();
    if (portalWeaponLightTrace) fileSystem->CloseFile(portalWeaponLightTrace);
    portalWeaponLightTrace = NULL;
}

static void DisablePortalWeaponLights() {
    for (int i = 0; i < portalWeaponLightHandles.Num(); ++i) {
        const renderLight_t *light = gameRenderWorld->GetRenderLight(portalWeaponLightHandles[i]);
        if (!light) continue;
        renderLight_t disabled = *light;
        disabled.allowLightInViewID = -1;
        gameRenderWorld->UpdateLightDef(portalWeaponLightHandles[i], &disabled);
    }
}

static void DisablePortalLighterLights() {
    for (int i = 0; i < portalLighterHandles.Num(); ++i) {
        const renderLight_t *source = gameRenderWorld->GetRenderLight(portalLighterHandles[i]);
        if (!source) continue;
        renderLight_t light = *source;
        light.allowLightInViewID = -1;
        gameRenderWorld->UpdateLightDef(portalLighterHandles[i], &light);
    }
}

static void ApplyPortalLighter(hhPlayer *player, const renderView_t &authoritative,
    const renderView_t &view, idList<presentationLightRestore_t> &restore) {
    if (!g_portalLighter.GetBool() || gameLocal.isMultiplayer || !player->IsLighterOn() ||
        player->IsSpiritOrDeathwalking() || player->spectating || player->InVehicle()) return;
    const renderLight_t *source = gameRenderWorld->GetRenderLight(player->lighterHandle);
    if (!source || !source->pointLight) return;
    renderLight_t presented = *source;
    idVec3 eye = player->GetEyePosition();
    if (!gameLocal.GetCamera() && !gameLocal.inCinematic && !pm_thirdPerson.GetBool() && view.viewID) {
        // The camera may still be rendered on the entrance side after physics
        // teleports. Move the real light and its copies with that same pose.
        const idMat3 rotation = authoritative.viewaxis.Transpose() * view.viewaxis;
        presented.origin = (source->origin - authoritative.vieworg) * rotation + view.vieworg;
        presented.axis = source->axis * rotation;
        presentationLightRestore_t saved;
        saved.handle = player->lighterHandle;
        saved.light = *source;
        restore.Append(saved);
        gameRenderWorld->UpdateLightDef(player->lighterHandle, &presented);
        eye = view.vieworg;
    }
    source = &presented;
    int used = 0;
    const float range = Max(source->lightRadius.x, Max(source->lightRadius.y, source->lightRadius.z));
    for (idEntity *ent = gameLocal.spawnedEntities.Next(); ent && used < 4; ent = ent->spawnNode.Next()) {
        if (!ent->IsType(hhPortal::Type)) continue;
        renderLight_t light = *source;
        idMat3 rotation;
        if (!static_cast<hhPortal *>(ent)->GetLighterTransform(source->origin, eye, range,
            light.origin, rotation, light.portalLightClipPlanes)) continue;
        light.axis = source->axis * rotation;
        light.portalLightClipCount = 5;
        light.portalLightOwnerViewID = player->entityNumber + 1;
        light.portalWeaponOnly = false;
        light.portalWeaponAttenuation = 0;
        light.allowLightInViewID = light.suppressLightInViewID = 0;
        light.prelightModel = NULL;
        // The virtual origin sits behind the destination portal wall. Its
        // untransformed shadow volume would incorrectly block the whole light.
        light.noShadows = true;
        if (used == portalLighterHandles.Num())
            portalLighterHandles.Append(gameRenderWorld->AddLightDef(&light));
        else gameRenderWorld->UpdateLightDef(portalLighterHandles[used], &light);
        ++used;
    }
    if (cvarSystem->GetCVarBool("com_fpsTrace"))
        gameLocal.Printf("PORTAL_LIGHTER %d lights %d\n", gameLocal.time, used);
}

static void ApplyPortalWeaponLighting(hhPlayer *player, const renderView_t &view,
    idList<presentationLightRestore_t> &restore) {
    if (!g_portalWeaponLighting.GetBool() || gameLocal.isMultiplayer || gameLocal.GetCamera() ||
        gameLocal.inCinematic || player->InVehicle() || player->spectating || pm_thirdPerson.GetBool() ||
        player->IsSpiritOrDeathwalking() || !view.viewID || !player->weapon.IsValid()) return;
    const float range = 24.0f;
    float nearest = range;
    idVec3 remoteEye, destination;
    idMat3 rotation;
    hhPortal *portal = NULL;
    for (idEntity *ent = gameLocal.spawnedEntities.Next(); ent; ent = ent->spawnNode.Next()) {
        if (!ent->IsType(hhPortal::Type)) continue;
        idVec3 eye, dest; idMat3 rot; float distance;
        hhPortal *candidate = static_cast<hhPortal *>(ent);
        if (candidate->GetWeaponLightingTransform(view.vieworg, range, eye, dest, rot, distance) && distance < nearest) {
            nearest = distance; remoteEye = eye; destination = dest; rotation = rot; portal = candidate;
        }
    }
    if (!portal) return;
    const float t = nearest / range;
    const float otherWeight = 0.5f * (1.0f - t*t*(3.0f-2.0f*t));
    const idMat3 inverse = rotation.Transpose();
    int used = 0, normalLights = 0;
    for (idEntity *ent = gameLocal.spawnedEntities.Next(); ent; ent = ent->spawnNode.Next()) {
        if (!ent->IsType(idLight::Type)) continue;
        const int handle = static_cast<idLight *>(ent)->GetLightDefHandle();
        const renderLight_t *original = handle >= 0 ? gameRenderWorld->GetRenderLight(handle) : NULL;
        if (!original || original->portalWeaponOnly ||
            (original->shader && (original->shader->IsFogLight() || original->shader->IsBlendLight()))) continue;
        // idLight entities are scene lights; camera-owned muzzle/FX lights are
        // deliberately left alone so shots and emissive weapon effects do not fade.
        const renderLight_t source = *original;
        const float reach = source.pointLight ? source.lightRadius.Length() + source.lightCenter.Length() :
            source.target.Length() + source.end.Length() + source.right.Length() + source.up.Length();
        if (source.parallel || (source.origin - view.vieworg).Length() <= reach + 96.0f) {
            // Keep an earlier presentation restore if this light was already moved.
            bool savedAlready = false;
            for (int i = 0; i < restore.Num(); ++i)
                if (restore[i].handle == handle) { savedAlready = true; break; }
            if (!savedAlready) {
                presentationLightRestore_t saved; saved.handle = handle; saved.light = source;
                restore.Append(saved);
            }
            renderLight_t attenuated = source;
            attenuated.portalWeaponAttenuation = otherWeight;
            gameRenderWorld->UpdateLightDef(handle, &attenuated);
            ++normalLights;
        }
        if (!source.parallel && (source.origin - remoteEye).Length() > reach + 96.0f) continue;
        renderLight_t light = source;
        light.origin = (source.origin - destination) * inverse + portal->GetOrigin();
        light.axis = source.axis * inverse;
        light.prelightModel = NULL;
        light.noShadows = true; // The remote room's occluders have not been transformed.
        light.portalWeaponOnly = true;
        // Weight the evaluated interaction, including shaders with constant RGB.
        light.portalWeaponAttenuation = 1.0f - otherWeight;
        light.allowLightInViewID = view.viewID;
        light.suppressLightInViewID = 0;
        if (used == portalWeaponLightHandles.Num())
            portalWeaponLightHandles.Append(gameRenderWorld->AddLightDef(&light));
        else gameRenderWorld->UpdateLightDef(portalWeaponLightHandles[used], &light);
        ++used;
    }
    if (cvarSystem->GetCVarBool("com_fpsTrace")) {
        if (!portalWeaponLightTrace) {
            portalWeaponLightTrace = fileSystem->OpenFileWrite("fps-portal-lighting.csv");
            if (portalWeaponLightTrace) portalWeaponLightTrace->Printf("game_ms,portal,distance,other_weight,normal_lights,remote_lights\n");
        }
        if (portalWeaponLightTrace) portalWeaponLightTrace->Printf("%d,%d,%.6f,%.6f,%d,%d\n",
            gameLocal.time, portal->entityNumber, nearest, otherWeight, normalLights, used);
    }
}
#endif
