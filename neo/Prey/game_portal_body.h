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
struct npcPortalPose_t {
    idEntityPtr<idEntity> entity, destination;
    idMat3 correction;
    int turnStart, crossingTime;
    idVec3 oldCenter, newCenter;
    idMat3 staleRotation;
};
static idList<npcPortalPose_t> npcPortalPoses;
void hhGameLocal::BeginNPCPortalPresentation(idEntity *entity, idEntity *destination,
    const idMat3 &mappedAxis, const idMat3 &physicalAxis, const idMat3 &rotation, const idVec3 &oldOrigin) {
    int index = -1;
    for (int i = 0; i < npcPortalPoses.Num(); ++i)
        if (npcPortalPoses[i].entity.GetEntity() == entity) { index = i; break; }
    idMat3 previous = mat3_identity;
    if (index >= 0) {
        const npcPortalPose_t &old = npcPortalPoses[index];
        float t = old.turnStart < 0 ? 0 : idMath::ClampFloat(0,1,(time-old.turnStart)/250.0f);
        t=t*t*(3-2*t); idQuat q; q.Slerp(old.correction.ToQuat(),mat3_identity.ToQuat(),t); previous=q.ToMat3();
    }
    if (index < 0) { npcPortalPose_t entry; index = npcPortalPoses.Append(entry); }
    npcPortalPose_t &entry = npcPortalPoses[index];
    entry.entity = entity; entry.destination = destination;
    entry.correction = physicalAxis.Transpose() * mappedAxis * rotation.Transpose() * previous * rotation;
    entry.turnStart = -1; entry.crossingTime = time;
    const idMat3 oldAxis = mappedAxis * rotation.Transpose();
    const idVec3 localCenter=entity->GetPhysics()->GetBounds().GetCenter();
    entry.oldCenter=oldOrigin+localCenter*oldAxis;
    entry.newCenter=entity->GetPhysics()->GetOrigin()+localCenter*physicalAxis;
    entry.staleRotation=oldAxis.Transpose()*physicalAxis;
}
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
// Camera-facing effects require four vertices and six indexes per quad.
// Transfer complete quads by their center; polygon clipping destroys the
// topology consumed by the renderer's sprite/tube deformation routines.
static int PortalBodyEffectQuads(portalBodyPart_t &part, const modelSurface_t &source,
    const idPlane *planes, int count) {
    const srfTriangles_t &mesh = *source.geometry;
    if ((mesh.numVerts & 3) || mesh.numIndexes != mesh.numVerts / 4 * 6) return 0;
    idList<int> nearQuads, farQuads;
    for (int q = 0; q < mesh.numVerts / 4; ++q) {
        idVec3 center = vec3_origin;
        for (int k = 0; k < 4; ++k) center += mesh.verts[q*4+k].xyz;
        center *= 0.25f;
        bool through = true;
        for (int i = 0; i < count; ++i) if (planes[i].Distance(center) < 0) { through = false; break; }
        (through ? farQuads : nearQuads).Append(q);
    }
    for (int side = 0; side < 2; ++side) {
        const idList<int> &quads = side ? farQuads : nearQuads;
        if (!quads.Num()) continue;
        idRenderModel *model = side ? part.farModel : part.nearModel;
        modelSurface_t out; out.id = source.id; out.shader = source.shader;
        out.geometry = model->AllocSurfaceTriangles(quads.Num()*4, quads.Num()*6);
        srfTriangles_t &tri = *out.geometry;
        tri.numVerts = quads.Num()*4; tri.numIndexes = quads.Num()*6; tri.bounds.Clear();
        for (int q = 0; q < quads.Num(); ++q) {
            for (int k = 0; k < 4; ++k) {
                tri.verts[q*4+k] = mesh.verts[quads[q]*4+k];
                tri.bounds.AddPoint(tri.verts[q*4+k].xyz);
            }
            for (int k = 0; k < 6; ++k) tri.indexes[q*6+k] = mesh.indexes[quads[q]*6+k]-quads[q]*4+q*4;
        }
        model->AddSurface(out);
    }
    return farQuads.Num()*2;
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
        if (surface->shader->Deform() == DFRM_SPRITE || surface->shader->Deform() == DFRM_TUBE) {
            farTriangles += PortalBodyEffectQuads(part, *surface, local, count);
            continue;
        }
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
// NPCs use the same clipped mesh builder, without first-person eye exclusions.
// Ghosts are renderer definitions only: no additional AI, health or collision.
static void ApplyNPCPortalBodies(const renderView_t &view, idList<presentationModelRestore_t> &restore) {
    if (!g_portalBodySplit.GetBool() || gameLocal.isMultiplayer) return;
    for (int i = npcPortalPoses.Num()-1; i >= 0; --i)
        if (!npcPortalPoses[i].entity.IsValid() || !npcPortalPoses[i].destination.IsValid() ||
            npcPortalPoses[i].entity->health <= 0) npcPortalPoses.RemoveIndex(i);
    idList<hhPortal *> portals;
    for (idEntity *ent=gameLocal.spawnedEntities.Next();ent;ent=ent->spawnNode.Next())
        if(ent->IsType(hhPortal::Type) && ent->spawnArgs.GetBool("rw_portalGun")) portals.Append(static_cast<hhPortal *>(ent));
    if(!portals.Num() && !npcPortalPoses.Num()) return;
    for (idEntity *actor = gameLocal.spawnedEntities.Next(); actor; actor = actor->spawnNode.Next()) {
        if (!actor->IsType(idAI::Type) || actor->health <= 0 || actor->IsHidden() || actor->IsBound() ||
            actor->fl.noPortal || !actor->GetPhysics()->IsType(idPhysics_Monster::Type)) continue;
        const renderEntity_t *body = gameRenderWorld->GetRenderEntity(actor->GetModelDefHandle());
        if (!body || !body->hModel) continue;
        const idVec3 center = actor->GetPhysics()->GetOrigin() +
            actor->GetPhysics()->GetBounds().GetCenter()*actor->GetPhysics()->GetAxis();
        idMat3 correction = mat3_identity;
        bool correcting = false;
        float turnBlend = 1;
        int npcState = -1;
        for (int i = 0; i < npcPortalPoses.Num(); ++i) {
            npcPortalPose_t &entry = npcPortalPoses[i];
            if (entry.entity.GetEntity() != actor) continue;
            npcState = i;
            idEntity *exit = entry.destination.GetEntity();
            const float radius = (actor->GetPhysics()->GetBounds()[1]-actor->GetPhysics()->GetBounds()[0]).Length()*0.5f;
            // Wait until the entire turning body clears the aperture. Then
            // ease to upright around its center, rather than pivoting at feet.
            if (entry.turnStart < 0 && (center-exit->GetOrigin())*exit->GetAxis()[0] > radius+2)
                entry.turnStart = gameLocal.time;
            float t = entry.turnStart < 0 ? 0 : idMath::ClampFloat(0, 1, (gameLocal.time + (presentationFraction >= 0 ? (presentationFraction-1)*USERCMD_MSEC : 0) - entry.turnStart)/250.0f);
            t = t*t*(3-2*t); turnBlend=t;
            idQuat q; q.Slerp(entry.correction.ToQuat(), mat3_identity.ToQuat(), t);
            correction = q.ToMat3(); correcting = t < 1;
            if (t >= 1) { npcPortalPoses.RemoveIndex(i); npcState=-1; }
            break;
        }
        bool nearby = correcting;
        for(int p=0;p<portals.Num() && !nearby;++p)
            nearby=(center-portals[p]->GetOrigin()).LengthSqr()<256*256;
        if(!nearby) continue;
        idList<portalBodyPose_t> poses;
        idBounds worldBounds; worldBounds.Clear();
        for (idEntity *ent = gameLocal.spawnedEntities.Next(); ent; ent = ent->spawnNode.Next()) {
            idEntity *owner = ent;
            while (owner->GetBindMaster()) owner = owner->GetBindMaster();
            if (owner != actor || ent->IsHidden() || ent->GetModelDefHandle() < 0) continue;
            const renderEntity_t *render = gameRenderWorld->GetRenderEntity(ent->GetModelDefHandle());
            if (!render || !render->hModel || render->hModel->IsDynamicModel() == DM_CONTINUOUS ||
                render->remoteRenderView || render->weaponDepthHack) continue;
            portalBodyPose_t item; item.entity = ent; item.original = item.pose = *render;
            if (item.pose.callback) item.pose.callback(&item.pose, &view);
            if (npcState >= 0 && npcPortalPoses[npcState].crossingTime == gameLocal.time) {
                const npcPortalPose_t &entry=npcPortalPoses[npcState];
                if((item.pose.origin-entry.oldCenter).LengthSqr()+1 < (item.pose.origin-entry.newCenter).LengthSqr()) {
                    item.pose.origin=(item.pose.origin-entry.oldCenter)*entry.staleRotation+entry.newCenter;
                    item.pose.axis*=entry.staleRotation;
                }
            }
            if (correcting) {
                item.pose.origin = center + (item.pose.origin-center)*correction;
                item.pose.axis *= correction;
            }
            idBounds bounds; bounds.FromTransformedBounds(item.pose.bounds, item.pose.origin, item.pose.axis);
            worldBounds.AddBounds(bounds); poses.Append(item);
        }
        hhPortal *portal = NULL;
        idVec3 source, destination; idMat3 rotation; idPlane planes[17]; int count = 0;
        float nearest = idMath::INFINITY;
        for (int portalIndex=0;portalIndex<portals.Num();++portalIndex) {
            hhPortal *ent=portals[portalIndex];
            idVec3 a,b; idMat3 r; idPlane p[17]; int n; float distance;
            // Portal selection follows the actor, independent of the viewer.
            const idVec3 front = ent->GetOrigin()+ent->GetAxis()[0];
            if (!static_cast<hhPortal *>(ent)->GetBodyPortalTransform(worldBounds, front, a,b,r,p,n,distance)) continue;
            distance = (center-ent->GetOrigin()).LengthSqr();
            if (distance >= nearest) continue;
            portal = static_cast<hhPortal *>(ent); nearest=distance;
            source=a; destination=b; rotation=r; count=n;
            for (int k=0;k<n;++k) planes[k]=p[k];
        }
        if (!portal && !correcting) continue;
        int triangles = 0;
        for (int i=0;i<poses.Num();++i) {
            portalBodyPose_t &item=poses[i];
            presentationModelRestore_t saved; saved.handle=item.entity->GetModelDefHandle(); saved.model=item.original;
            restore.Append(saved);
            if (!portal) { gameRenderWorld->UpdateEntityDef(saved.handle,&item.pose); continue; }
            portalBodyPart_t &part=PortalBodyPart(item.entity,item.pose.hModel);
            int remoteTriangles=SplitPortalBodyModel(part,item.pose,planes,count,true);
            if (!remoteTriangles) { gameRenderWorld->UpdateEntityDef(saved.handle,&item.pose); continue; }
            renderEntity_t nearPiece=item.pose;
            nearPiece.hModel=part.nearModel; nearPiece.bounds=part.nearModel->Bounds();
            nearPiece.callback=NULL; nearPiece.callbackData=NULL; nearPiece.joints=NULL; nearPiece.numJoints=0;
            nearPiece.forceUpdate=true; nearPiece.portalBodyEyeRadius=0;
            gameRenderWorld->UpdateEntityDef(saved.handle,&nearPiece);
            renderEntity_t farPiece=nearPiece;
            farPiece.hModel=part.farModel; farPiece.bounds=part.farModel->Bounds();
            farPiece.origin=(nearPiece.origin-source)*rotation+destination; farPiece.axis=nearPiece.axis*rotation;
            if(part.ghostHandle<0) part.ghostHandle=gameRenderWorld->AddEntityDef(&farPiece);
            else gameRenderWorld->UpdateEntityDef(part.ghostHandle,&farPiece);
            triangles+=remoteTriangles;
        }
        if(g_portalBodyTrace.GetBool()) gameLocal.Printf("PORTAL_NPC_BODY name=%s triangles=%d turning=%d blend=%.3f\n",actor->GetName(),triangles,correcting,turnBlend);
    }
}
static void RestorePortalBodies(const idList<presentationModelRestore_t> &restore) {
    for (int i = 0; i < restore.Num(); ++i) gameRenderWorld->UpdateEntityDef(restore[i].handle, &restore[i].model);
    HidePortalBodies();
}
#endif
