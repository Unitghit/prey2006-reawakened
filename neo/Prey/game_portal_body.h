#ifndef __PREY_PORTAL_BODY_H__
#define __PREY_PORTAL_BODY_H__
#include "../renderer/Model_eyeball.h"
// Render-only model pieces. None of these objects enter physics or save files.
static idCVar g_portalBodySplit("g_portalBodySplit", "1", CVAR_GAME | CVAR_BOOL,
    "split the local player's third-person model across an intersecting portal (0 for comparison)");
static idCVar g_portalBodyTrace("g_portalBodyTrace", "0", CVAR_GAME | CVAR_BOOL,
    "log portal body pieces for isolated rendering tests");
struct portalBodyPart_t {
    idEntityPtr<idEntity> entity;
    idRenderModel *base, *snapshot, *nearModel, *farModel;
    int ghostHandle;
    portalBodyPart_t() : base(NULL), snapshot(NULL), nearModel(NULL), farModel(NULL), ghostHandle(-1) {}
};
static idList<portalBodyPart_t *> portalBodyParts;
static void ClearPortalBodies() {
    for (int i = 0; i < portalBodyParts.Num(); ++i) {
        portalBodyPart_t &part = *portalBodyParts[i];
        if (part.ghostHandle >= 0) gameRenderWorld->FreeEntityDef(part.ghostHandle);
        if (part.snapshot) renderModelManager->FreeModel(part.snapshot);
        if (part.nearModel) renderModelManager->FreeModel(part.nearModel);
        if (part.farModel) renderModelManager->FreeModel(part.farModel);
    }
    portalBodyParts.DeleteContents(true);
}
static void HidePortalBodies() {
    for (int i = 0; i < portalBodyParts.Num(); ++i) {
        const int handle = portalBodyParts[i]->ghostHandle;
        const renderEntity_t *original = handle >= 0 ? gameRenderWorld->GetRenderEntity(handle) : NULL;
        if (!original) continue;
        renderEntity_t hidden = *original;
        hidden.allowSurfaceInViewID = -1; hidden.noShadow = true;
        gameRenderWorld->UpdateEntityDef(handle, &hidden);
    }
}
static portalBodyPart_t &PortalBodyPart(idEntity *entity, idRenderModel *model) {
    portalBodyPart_t *part = NULL;
    for (int i = 0; i < portalBodyParts.Num(); ++i)
        if (portalBodyParts[i]->entity.GetEntity() == entity) { part = portalBodyParts[i]; break; }
    if (!part) {
        part = new portalBodyPart_t; part->entity = entity; portalBodyParts.Append(part);
        part->nearModel = renderModelManager->AllocModel();
        part->farModel = renderModelManager->AllocModel();
    }
    if (part->base != model) {
        if (part->snapshot) renderModelManager->FreeModel(part->snapshot);
        part->snapshot = NULL; part->base = model;
    }
    return *part;
}
static void PortalBodyTriangles(const idList<idDrawVert> &polygon, idList<idDrawVert> &triangles) {
    for (int i = 1; i + 1 < polygon.Num(); ++i) {
        if ((polygon[i].xyz - polygon[0].xyz).Cross(polygon[i+1].xyz - polygon[0].xyz).LengthSqr() < 1e-10f) continue;
        triangles.Append(polygon[0]); triangles.Append(polygon[i]); triangles.Append(polygon[i+1]);
    }
}
static void PortalBodySplitPolygon(const idList<idDrawVert> &polygon, const idPlane &plane,
    idList<idDrawVert> &inside, idList<idDrawVert> &outside) {
    inside.Clear(); outside.Clear();
    for (int i = 0; i < polygon.Num(); ++i) {
        const idDrawVert &a = polygon[i], &b = polygon[(i+1)%polygon.Num()];
        const float da = plane.Distance(a.xyz), db = plane.Distance(b.xyz);
        if (da >= 0) inside.Append(a); else outside.Append(a);
        if ((da > 0 && db < 0) || (da < 0 && db > 0)) {
            idDrawVert edge; edge.LerpAll(a, b, da/(da-db)); edge.Normalize();
            inside.Append(edge); outside.Append(edge);
        }
    }
}
static void PortalBodySurface(idRenderModel *model, const modelSurface_t &source, const idList<idDrawVert> &vertices, bool depthBias) {
    if (!vertices.Num()) return;
    modelSurface_t surface; surface.id = source.id; surface.shader = source.shader;
    surface.geometry = model->AllocSurfaceTriangles(vertices.Num(), vertices.Num());
    srfTriangles_t *tri = surface.geometry;
    tri->numVerts = tri->numIndexes = vertices.Num(); tri->bounds.Clear();
    tri->eyeballDeformed = source.shader->Deform() == DFRM_EYEBALL;
    tri->portalBodyDepthBias = depthBias;
    for (int i = 0; i < vertices.Num(); ++i) {
        tri->verts[i] = vertices[i]; tri->indexes[i] = i; tri->bounds.AddPoint(vertices[i].xyz);
    }
    model->AddSurface(surface);
}
static int SplitPortalBodyModel(portalBodyPart_t &part, const renderEntity_t &pose, const idPlane *planes, int count, bool depthBias) {
    idRenderModel *model = pose.hModel;
    if (model->IsDynamicModel() != DM_STATIC) {
        part.snapshot = pose.hModel->InstantiateDynamicModel(&pose, NULL, part.snapshot);
        model = part.snapshot;
    }
    if (!model) return 0;
    // MD5 skinning defers these until drawing. Compute them on the intact
    // pose so both cut pieces inherit the same smooth lighting basis.
    model->EnsureSurfaceTangents();
    part.nearModel->InitEmpty("_portalBody_near"); part.farModel->InitEmpty("_portalBody_far");
    idPlane local[17];
    for (int i = 0; i < count; ++i) {
        local[i].SetNormal(planes[i].Normal() * pose.axis.Transpose());
        local[i][3] = planes[i].Distance(pose.origin);
    }
    int farTriangles = 0;
    for (int s = 0; s < model->NumBaseSurfaces(); ++s) {
        const modelSurface_t *surface = model->Surface(s);
        if (!surface || !surface->geometry || !surface->shader) continue;
        srfTriangles_t mesh = *surface->geometry;
        idList<idDrawVert> eyeVertices; idList<glIndex_t> eyeIndexes;
        if (surface->shader->Deform() == DFRM_EYEBALL) {
            if (!R_BakeEyeballSurface(&mesh, eyeVertices, eyeIndexes)) continue;
            mesh.verts = eyeVertices.Ptr(); mesh.indexes = eyeIndexes.Ptr();
            mesh.numVerts = eyeVertices.Num(); mesh.numIndexes = eyeIndexes.Num();
        }
        idList<idDrawVert> nearVerts, farVerts, polygon, inside, outside;
        for (int t = 0; t < mesh.numIndexes; t += 3) {
            polygon.Clear(); for (int k = 0; k < 3; ++k) polygon.Append(mesh.verts[mesh.indexes[t+k]]);
            for (int i = 0; i < count && polygon.Num() >= 3; ++i) {
                idPlane plane = local[i];
                if (depthBias && i == 0) plane[3] -= PORTAL_BODY_SEAM_OVERLAP;
                PortalBodySplitPolygon(polygon, plane, inside, outside);
                PortalBodyTriangles(outside, nearVerts); polygon = inside;
            }
            if (depthBias) {
                // The two rooms resolve MSAA separately. Retain a narrow common
                // band so resolving one half cannot expose background at the seam.
                polygon.Clear(); for (int k = 0; k < 3; ++k) polygon.Append(mesh.verts[mesh.indexes[t+k]]);
                for (int i = 0; i < count && polygon.Num() >= 3; ++i) {
                    idPlane plane = local[i];
                    if (i == 0) plane[3] += PORTAL_BODY_SEAM_OVERLAP;
                    PortalBodySplitPolygon(polygon, plane, inside, outside); polygon = inside;
                }
            }
            PortalBodyTriangles(polygon, farVerts);
        }
        farTriangles += farVerts.Num()/3;
        PortalBodySurface(part.nearModel, *surface, nearVerts, depthBias);
        PortalBodySurface(part.farModel, *surface, farVerts, depthBias);
    }
    part.nearModel->FinishSurfaces(); part.farModel->FinishSurfaces();
    return farTriangles;
}
struct portalBodyPose_t { idEntity *entity; renderEntity_t original, pose; };
static bool ApplyPortalBodies(hhPlayer *player, const renderView_t &authoritative, const renderView_t &view,
    bool sourceSide, idList<presentationModelRestore_t> &restore) {
    idTimer timer;
    if (g_portalBodyTrace.GetBool()) timer.Start();
    HidePortalBodies();
    if (!g_portalBodySplit.GetBool() || gameLocal.isMultiplayer || player->spectating || player->InVehicle() ||
        player->IsSpiritOrDeathwalking() || gameLocal.inCinematic || gameLocal.GetCamera() || player->IsHidden()) return false;
    const int viewID = player->entityNumber + 1;
    idList<portalBodyPose_t> poses;
    idBounds worldBounds; worldBounds.Clear();
    const bool crossing = presentationPortalTime == gameLocal.time;
    const renderEntity_t *body = gameRenderWorld->GetRenderEntity(player->GetModelDefHandle());
    if (!body) return false;
    for (idEntity *ent = gameLocal.spawnedEntities.Next(); ent; ent = ent->spawnNode.Next()) {
        if (ent->IsHidden() || ent->GetModelDefHandle() < 0) continue;
        const renderEntity_t *render = gameRenderWorld->GetRenderEntity(ent->GetModelDefHandle());
        if (!render || !render->hModel || render->hModel->IsDynamicModel() == DM_CONTINUOUS || render->weaponDepthHack || render->allowSurfaceInViewID ||
            render->suppressSurfaceInViewID != viewID || render->remoteRenderView) continue;
        portalBodyPose_t item; item.entity = ent; item.original = item.pose = *render;
        if (item.pose.callback) item.pose.callback(&item.pose, &view);
        if (crossing) {
            // The body or a bound attachment can still carry the pre-teleport
            // render transform. Physics is already authoritative in the exit room.
            const idVec3 mapped = (item.pose.origin-presentationPortalSource)*presentationPortalRotation+presentationPortalDestination;
            if ((mapped-player->GetPhysics()->GetOrigin()).LengthSqr()+1 < (item.pose.origin-player->GetPhysics()->GetOrigin()).LengthSqr()) {
                item.pose.origin = mapped; item.pose.axis *= presentationPortalRotation;
            }
            idVec3 drawEye = view.vieworg;
            if (sourceSide) drawEye = (drawEye-presentationPortalSource)*presentationPortalRotation+presentationPortalDestination;
            item.pose.origin += drawEye-authoritative.vieworg;
            if (sourceSide) {
                const idMat3 inverse = presentationPortalRotation.Transpose();
                item.pose.origin = (item.pose.origin-presentationPortalDestination)*inverse+presentationPortalSource;
                item.pose.axis *= inverse;
            }
        }
        idBounds bounds; bounds.FromTransformedBounds(item.pose.bounds, item.pose.origin, item.pose.axis);
        worldBounds.AddBounds(bounds); poses.Append(item);
    }
    if (!poses.Num()) return false;
    hhPortal *portal = NULL;
    idVec3 source, destination; idMat3 rotation; idPlane planes[17]; int count = 0;
    float nearest = idMath::INFINITY;
    for (idEntity *ent = gameLocal.spawnedEntities.Next(); ent; ent = ent->spawnNode.Next()) {
        if (!ent->IsType(hhPortal::Type)) continue;
        idVec3 a, b; idMat3 r; idPlane p[17]; int n; float distance;
        if (!static_cast<hhPortal *>(ent)->GetBodyPortalTransform(worldBounds, view.vieworg, a, b, r, p, n, distance) || distance >= nearest) continue;
        portal = static_cast<hhPortal *>(ent); nearest = distance;
        source = a; destination = b; rotation = r; count = n;
        for (int i = 0; i < n; ++i) planes[i] = p[i];
    }
    if (!portal) return false;
    int pieces = 0, triangles = 0;
    for (int i = 0; i < poses.Num(); ++i) {
        portalBodyPose_t &item = poses[i];
        portalBodyPart_t &part = PortalBodyPart(item.entity, item.pose.hModel);
        const int farTriangles = SplitPortalBodyModel(part, item.pose, planes, count, portal->spawnArgs.GetBool("rw_portalGun"));
        // Even an attachment wholly on one side can contain the virtual eye
        // for a crossing frame. Keep the own-eye guard on uncut parts as well.
        item.pose.portalBodyEye = view.vieworg; item.pose.portalBodyEyeRadius = 12.0f;
        if (!farTriangles) {
            presentationModelRestore_t saved; saved.handle = item.entity->GetModelDefHandle(); saved.model = item.original;
            restore.Append(saved);
            gameRenderWorld->UpdateEntityDef(saved.handle, &item.pose);
            continue;
        }
        presentationModelRestore_t saved; saved.handle = item.entity->GetModelDefHandle(); saved.model = item.original;
        restore.Append(saved);
        renderEntity_t sourcePiece = item.pose;
        sourcePiece.portalBodyEye = view.vieworg; sourcePiece.portalBodyEyeRadius = 12.0f;
        sourcePiece.hModel = part.nearModel; sourcePiece.bounds = part.nearModel->Bounds();
        sourcePiece.callback = NULL; sourcePiece.callbackData = NULL; sourcePiece.joints = NULL; sourcePiece.numJoints = 0; sourcePiece.forceUpdate = true;
        gameRenderWorld->UpdateEntityDef(saved.handle, &sourcePiece);
        renderEntity_t remotePiece = sourcePiece; remotePiece.hModel = part.farModel; remotePiece.bounds = part.farModel->Bounds();
        // This is the body in the other room, not the first-person body.
        // The view ID is shared across the entire main view: inheriting it hides
        // the remote half even when it is directly visible in front of the exit.
        // The transformed-eye test still hides it from its own virtual camera.
        remotePiece.suppressSurfaceInViewID = 0;
        remotePiece.suppressShadowInViewID = 0;
        remotePiece.portalBodyEye = (view.vieworg-source)*rotation+destination;
        remotePiece.origin = (sourcePiece.origin-source)*rotation+destination; remotePiece.axis = sourcePiece.axis*rotation;
        if (part.ghostHandle < 0) part.ghostHandle = gameRenderWorld->AddEntityDef(&remotePiece);
        else gameRenderWorld->UpdateEntityDef(part.ghostHandle, &remotePiece);
        ++pieces; triangles += farTriangles;
    }
    if (g_portalBodyTrace.GetBool()) {
        timer.Stop();
        gameLocal.Printf("PORTAL_BODY time %d portal %s parts %d remote_triangles %d source_side %d build_ms %u\n",
            gameLocal.time, portal->name.c_str(), pieces, triangles, sourceSide ? 1 : 0, timer.Milliseconds());
    }
    return pieces != 0;
}
static void RestorePortalBodies(const idList<presentationModelRestore_t> &restore) {
    for (int i = 0; i < restore.Num(); ++i) gameRenderWorld->UpdateEntityDef(restore[i].handle, &restore[i].model);
    HidePortalBodies();
}
#endif
