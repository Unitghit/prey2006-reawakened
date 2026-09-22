// Transient renderer-only history. Never serialized or used for collision.
#ifndef __PREY_GAME_PRESENTATION_H__
#define __PREY_GAME_PRESENTATION_H__

static idCVar g_interpolateEffects("g_interpolateEffects", "1", CVAR_GAME | CVAR_BOOL,
    "advance renderer material and particle time between simulation ticks");

static idCVar g_interpolateWorld( "g_interpolateWorld", "1", CVAR_GAME | CVAR_BOOL,
    "interpolate single-player world render poses without changing simulation" );

static idCVar g_interpolateWeapons( "g_interpolateWeapons", "1", CVAR_GAME | CVAR_BOOL,
    "interpolate first-person weapon-local animation without changing firing" );

struct presentationPose_t {
    idEntityPtr<idEntity> entity;
    int time, handle, viewID;
    idRenderModel *model;
    idVec3 origin;
    idMat3 axis;
    idBounds bounds;
    idList<idJointQuat> joints;
    idList<int> lightHandles;
    idList<renderLight_t> lights;
    const hhDeclBeam *beamDecl;
    idList<hhBeamNodes_t> beamNodes, drawBeamNodes;
    int worldLightHandle;
    renderLight_t worldLight;
    idJointMat *drawJoints;
    int allocatedJoints;
    presentationPose_t() : time(-1), handle(-1), model(NULL), beamDecl(NULL), worldLightHandle(-1), drawJoints(NULL), allocatedJoints(0) {}
    ~presentationPose_t() { if (drawJoints) Mem_Free16(drawJoints); }
};
static idList<presentationPose_t *> presentationPoses;
static idCVar g_presentationProbe("g_presentationProbe", "", CVAR_GAME,
    "named moving platform to inspect while com_fpsTrace is enabled");
static idFile *presentationPlatformTrace = NULL;

static void ClosePlatformTrace() {
    if (presentationPlatformTrace) fileSystem->CloseFile(presentationPlatformTrace);
    presentationPlatformTrace = NULL;
}

// Observe real pusher contacts and renderer state; never move either participant.
struct presentationPlatformSample_t {
    idEntity *platform;
    idVec3 playerOrigin, platformOrigin, simOrigin, drawOrigin, previousOrigin;
    idMat3 physicsAxis;
    float simZ, drawZ, previousZ, topZ, simEyeZ, drawEyeZ;
    bool grounded;
    presentationPlatformSample_t(hhPlayer *player, const renderView_t &sim, const renderView_t &draw) : platform(NULL) {
        if (!cvarSystem->GetCVarBool("com_fpsTrace") || !g_presentationProbe.GetString()[0]) {
            ClosePlatformTrace();
            return;
        }
        platform = gameLocal.FindEntity(g_presentationProbe.GetString());
        if (!platform || platform->GetModelDefHandle() < 0) { platform = NULL; return; }
        const renderEntity_t *render = gameRenderWorld->GetRenderEntity(platform->GetModelDefHandle());
        if (!render) { platform = NULL; return; }
        playerOrigin = player->GetPhysics()->GetOrigin();
        platformOrigin = platform->GetPhysics()->GetOrigin();
        simOrigin = platform->GetRenderEntity()->origin; drawOrigin = render->origin; previousOrigin = simOrigin;
        physicsAxis = platform->GetPhysics()->GetAxis();
        simZ = platform->GetRenderEntity()->origin.z;
        drawZ = render->origin.z;
        previousZ = simZ;
        const int slot = platform->entityNumber;
        if (slot < presentationPoses.Num() && presentationPoses[slot] &&
            presentationPoses[slot]->entity.GetEntity() == platform &&
            gameLocal.time - presentationPoses[slot]->time == USERCMD_MSEC)
            { previousZ = presentationPoses[slot]->origin.z; previousOrigin = presentationPoses[slot]->origin; }
        topZ = platform->GetPhysics()->GetAbsBounds()[1].z;
        simEyeZ = sim.vieworg.z; drawEyeZ = draw.vieworg.z;
        grounded = player->GetPhysics()->IsGroundEntity(platform->entityNumber);
    }
    void Finish(hhPlayer *player) const {
        if (!platform) return;
        if (!presentationPlatformTrace) {
            presentationPlatformTrace = fileSystem->OpenFileWrite("fps-platform.csv");
            if (presentationPlatformTrace) presentationPlatformTrace->Printf("game_ms,fraction,world,previous_z,sim_z,draw_z,physics_z,player_z,top_z,sim_eye_z,draw_eye_z,grounded,physics_unchanged,previous_x,previous_y,sim_x,sim_y,draw_x,draw_y\n");
        }
        if (presentationPlatformTrace) presentationPlatformTrace->Printf("%d,%.6f,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
            gameLocal.time, presentationFraction, g_interpolateWorld.GetBool() ? 1 : 0,
            previousZ, simZ, drawZ, platformOrigin.z, playerOrigin.z, topZ, simEyeZ, drawEyeZ, grounded ? 1 : 0,
            playerOrigin == player->GetPhysics()->GetOrigin() && platformOrigin == platform->GetPhysics()->GetOrigin() && physicsAxis == platform->GetPhysics()->GetAxis() ? 1 : 0,
            previousOrigin.x, previousOrigin.y, simOrigin.x, simOrigin.y, drawOrigin.x, drawOrigin.y);
    }
};
// Optional trace: distinguish interpolated skeletal poses between simulation ticks.
static double worldPoseChecksum = 0.0;
static double beamPoseChecksum = 0.0;
static int presentationBeamCount = 0, presentationWorldLightCount = 0;
static int presentationBeamAttachments = 0;
static float presentationBeamAttachmentError = 0.0f, presentationBeamEndError = 0.0f;

static int GetWorldPresentationLight(idEntity *ent) {
    return ent->IsType(hhWeaponSoulStripper::Type) ? static_cast<hhWeaponSoulStripper *>(ent)->beamLightHandle : -1;
}

static double SimulationBeamChecksum() {
    double sum = 0.0;
    for (idEntity *ent = gameLocal.spawnedEntities.Next(); ent; ent = ent->spawnNode.Next()) {
        const renderEntity_t *r = ent->GetRenderEntity();
        if (!r->beamNodes || !r->declBeam) continue;
        for (int b = 0; b < r->declBeam->numBeams; ++b)
            for (int n = 0; n < r->declBeam->numNodes; ++n) {
                const idVec3 &p = r->beamNodes[b].nodes[n];
                sum += (p.x + 2.0*p.y + 3.0*p.z) * (1 + n + b * MAX_BEAM_NODES);
            }
    }
    return sum;
}

static void ClearPresentationPoses() {
    presentationPoses.DeleteContents(true);
}

static bool CanPresentEntity(idEntity *ent, const renderEntity_t *render) {
    return ent->GetModelDefHandle() >= 0 && !ent->IsHidden() && render->hModel &&
        !render->allowSurfaceInViewID && !render->remoteRenderView &&
        !ent->IsType(hhPortal::Type) &&
        (!render->callback || render->callback == idEntity::ModelCallback ||
        // Pods use a timer callback to refresh their surface deformation. It
        // does not replace their transform, so their rigid pose can still blend.
        (ent->IsType(hhPod::Type) && render->callback == idEntity::SixtyHertzCallback));
}

static void CapturePresentationPoses() {
    // Do not allocate/refresh skeleton history on the legacy scheduling path.
    // One presentation frame warms this up after menus, loads, or toggles.
    if (presentationFraction < 0.0f || !cvarSystem->GetCVarBool("com_unlockedFPS") ||
        (!g_interpolateWorld.GetBool() && !g_interpolateWeapons.GetBool())) {
        ClearPresentationPoses();
        return;
    }
    for (idEntity *ent = gameLocal.spawnedEntities.Next(); ent; ent = ent->spawnNode.Next()) {
        renderEntity_t *render = ent->GetRenderEntity();
        const bool viewModel = render->allowSurfaceInViewID != 0 && render->allowSurfaceInViewID == previousPresentationView.viewID;
        if (!viewModel && !g_interpolateWorld.GetBool()) continue;
        if (!CanPresentEntity(ent, render) && !(g_interpolateWeapons.GetBool() && viewModel &&
            ent->GetModelDefHandle() >= 0 && !ent->IsHidden() && render->hModel &&
            (!render->callback || render->callback == idEntity::ModelCallback))) continue;
        const int slot = ent->entityNumber;
        if (slot < 0) continue;
        while (presentationPoses.Num() <= slot) presentationPoses.Append(NULL);
        if (!presentationPoses[slot]) presentationPoses[slot] = new presentationPose_t;
        presentationPose_t &pose = *presentationPoses[slot];
        // Only refresh the animator. Derived item callbacks need a real view
        // and may mutate view-dependent effects; never dispatch those here.
        ent->idEntity::UpdateRenderEntity(render, NULL);
        pose.entity = ent; pose.time = gameLocal.time; pose.handle = ent->GetModelDefHandle();
        pose.viewID = render->allowSurfaceInViewID;
        pose.model = render->hModel; pose.origin = render->origin; pose.axis = render->axis;
        pose.bounds = render->bounds;
        pose.beamDecl = render->declBeam;
        const int beamCount = render->beamNodes && render->declBeam ? render->declBeam->numBeams : 0;
        pose.beamNodes.SetNum(beamCount);
        for (int b = 0; b < beamCount; ++b) pose.beamNodes[b] = render->beamNodes[b];
        pose.worldLightHandle = viewModel ? GetWorldPresentationLight(ent) : -1;
        if (pose.worldLightHandle >= 0) {
            const renderLight_t *light = gameRenderWorld->GetRenderLight(pose.worldLightHandle);
            if (light) pose.worldLight = *light;
            else pose.worldLightHandle = -1;
        }
        pose.lightHandles.Clear(); pose.lights.Clear();
        if (viewModel && ent->IsType(hhWeapon::Type)) {
            idList<int> handles;
            static_cast<hhWeapon *>(ent)->GetPresentationLightHandles(handles);
            for (int i = 0; i < handles.Num(); ++i) {
                const renderLight_t *light = gameRenderWorld->GetRenderLight(handles[i]);
                if (light) { pose.lightHandles.Append(handles[i]); pose.lights.Append(*light); }
            }
        }
        const idMD5Joint *skeleton = render->hModel->GetJoints();
        const int count = skeleton && render->joints && render->numJoints == render->hModel->NumJoints() ? render->numJoints : 0;
        pose.joints.SetNum(count);
        for (int j = 0; j < count; ++j) {
            idJointMat local = render->joints[j];
            if (skeleton[j].parent) {
                const int parent = (int)(skeleton[j].parent - skeleton);
                local /= render->joints[parent];
            }
            pose.joints[j] = local.ToJointQuat();
        }
    }
}

static void BlendPresentationJoints(presentationPose_t &pose, const renderEntity_t &original, renderEntity_t &adjusted, float fraction) {
    const idMD5Joint *skeleton = original.hModel->GetJoints();
    if (skeleton && original.joints && original.numJoints == pose.joints.Num() && original.numJoints > 0) {
        if (pose.allocatedJoints != original.numJoints) {
            if (pose.drawJoints) Mem_Free16(pose.drawJoints);
            pose.drawJoints = (idJointMat *)Mem_Alloc16(sizeof(idJointMat) * original.numJoints);
            pose.allocatedJoints = original.numJoints;
        }
        for (int j = 0; j < original.numJoints; ++j) {
            const int parent = skeleton[j].parent ? (int)(skeleton[j].parent - skeleton) : -1;
            idJointMat local = original.joints[j];
            if (parent >= 0) local /= original.joints[parent];
            const idJointQuat current = local.ToJointQuat();
            idJointQuat blended;
            blended.q.Slerp(pose.joints[j].q, current.q, fraction);
            blended.t.Lerp(pose.joints[j].t, current.t, fraction);
            pose.drawJoints[j].SetRotation(blended.q.ToMat3());
            pose.drawJoints[j].SetTranslation(blended.t);
            if (parent >= 0) pose.drawJoints[j] *= pose.drawJoints[parent];
        }
        adjusted.joints = pose.drawJoints;
        adjusted.callback = NULL;

    }
}

static void BlendPresentationBeams(presentationPose_t &pose, const renderEntity_t &original,
    renderEntity_t &adjusted, float fraction, const renderView_t *currentView = NULL, const renderView_t *drawView = NULL) {
    if (!original.beamNodes || !original.declBeam || pose.beamDecl != original.declBeam ||
        pose.beamNodes.Num() != original.declBeam->numBeams) return;
    idMat3 inverseAxis = adjusted.axis;
    if (!inverseAxis.InverseSelf()) return;
    pose.drawBeamNodes.SetNum(pose.beamNodes.Num());
    for (int b = 0; b < pose.beamNodes.Num(); ++b) {
        pose.drawBeamNodes[b] = original.beamNodes[b];
        for (int n = 0; n < original.declBeam->numNodes; ++n) {
            idVec3 oldPoint = pose.origin + pose.beamNodes[b].nodes[n] * pose.axis;
            idVec3 point = original.origin + original.beamNodes[b].nodes[n] * original.axis;
            if (currentView && drawView) {
                oldPoint = (oldPoint - previousPresentationView.vieworg) * previousPresentationView.viewaxis.Transpose();
                point = (point - currentView->vieworg) * currentView->viewaxis.Transpose();
            }
            idVec3 blended; blended.Lerp(oldPoint, point, fraction);
            if (currentView && drawView) blended = drawView->vieworg + blended * drawView->viewaxis;
            pose.drawBeamNodes[b].nodes[n] = (blended - adjusted.origin) * inverseAxis;
            idBounds nodeBounds(pose.drawBeamNodes[b].nodes[n]);
            nodeBounds.ExpandSelf(original.declBeam->thickness[b] * 0.5f);
            adjusted.bounds.AddBounds(nodeBounds);
            if (cvarSystem->GetCVarBool("com_fpsTrace"))
                beamPoseChecksum += (blended.x + 2.0*blended.y + 3.0*blended.z) * (1 + n + b * MAX_BEAM_NODES);
        }
    }
    adjusted.beamNodes = pose.drawBeamNodes.Ptr();
    ++presentationBeamCount;
}

static int ApplyWorldPresentation(float fraction, idList<idEntity *> &restore) {
    worldPoseChecksum = 0.0;
    if (!g_interpolateWorld.GetBool()) return 0;
    int count = 0;
    for (int slot = 0; slot < presentationPoses.Num(); ++slot) {
        presentationPose_t *pose = presentationPoses[slot];
        if (!pose || gameLocal.time - pose->time != USERCMD_MSEC) continue;
        idEntity *ent = pose->entity.GetEntity();
        if (!ent) continue;
        renderEntity_t *original = ent->GetRenderEntity();
        if (!CanPresentEntity(ent, original) || ent->GetModelDefHandle() != pose->handle ||
            original->hModel != pose->model ||
            (original->origin - pose->origin).LengthSqr() >= 128.0f * 128.0f ||
            original->axis[0] * pose->axis[0] <= 0.0f) continue;
        if (!original->numJoints && !original->beamNodes && original->origin == pose->origin && original->axis == pose->axis) continue;
        ent->idEntity::UpdateRenderEntity(original, NULL);
        renderEntity_t adjusted = *original;
        adjusted.origin.Lerp(pose->origin, original->origin, fraction);
        idQuat rotation;
        rotation.Slerp(pose->axis.ToQuat(), original->axis.ToQuat(), fraction);
        adjusted.axis = rotation.ToMat3();
        adjusted.bounds.AddBounds(pose->bounds);
        BlendPresentationJoints(*pose, *original, adjusted, fraction);
        BlendPresentationBeams(*pose, *original, adjusted, fraction);
        if (adjusted.joints && cvarSystem->GetCVarBool("com_fpsTrace")) {
            for (int j = 0; j < adjusted.numJoints; ++j) {
                const idVec3 p = adjusted.joints[j].ToVec3();
                worldPoseChecksum += p.x + 2.0 * p.y + 3.0 * p.z;
            }
        }
        gameRenderWorld->UpdateEntityDef(pose->handle, &adjusted);
        restore.Append(ent);
        ++count;
    }
    return count;
}

struct presentationLightRestore_t { int handle; renderLight_t light; };
struct presentationModelRestore_t { int handle; renderEntity_t model; };
static double weaponPoseChecksum = 0.0;

static void SuppressPortalCrossingBody(int viewID, idList<idEntity *> &restore) {
    // The camera can reach either side before the player's bound head/world
    // weapon receives its next Think. Portal subviews clear viewID, so those
    // stale third-person meshes otherwise become visible inside our own eye.
    // Extend the normal first-person suppression for this crossing tick only.
    // The real first-person weapon and other actors retain their visibility.
    for (idEntity *ent = gameLocal.spawnedEntities.Next(); ent; ent = ent->spawnNode.Next()) {
        const int handle = ent->GetModelDefHandle();
        const renderEntity_t *model = handle >= 0 ? gameRenderWorld->GetRenderEntity(handle) : NULL;
        if (!model || !viewID || model->suppressSurfaceInViewID != viewID) continue;
        renderEntity_t hidden = *model;
        hidden.allowSurfaceInViewID = viewID;
        gameRenderWorld->UpdateEntityDef(handle, &hidden);
        restore.AddUnique(ent);
    }
}

static void BlendCameraLocalTransform(const idVec3 &oldOrigin, const idMat3 &oldAxis,
    const idVec3 &currentOrigin, const idMat3 &currentAxis, const renderView_t &currentView,
    const renderView_t &drawView, float fraction, idVec3 &origin, idMat3 &axis) {
    const idVec3 a = (oldOrigin - previousPresentationView.vieworg) * previousPresentationView.viewaxis.Transpose();
    const idVec3 b = (currentOrigin - currentView.vieworg) * currentView.viewaxis.Transpose();
    idVec3 local; local.Lerp(a, b, fraction);
    idQuat rotation;
    rotation.Slerp((oldAxis * previousPresentationView.viewaxis.Transpose()).ToQuat(),
        (currentAxis * currentView.viewaxis.Transpose()).ToQuat(), fraction);
    origin = drawView.vieworg + local * drawView.viewaxis;
    axis = rotation.ToMat3() * drawView.viewaxis;
}

static void ApplyViewPresentation(const renderView_t &currentView, const renderView_t &drawView,
    float fraction, idList<idEntity *> &restore, idList<presentationLightRestore_t> &lights) {
    weaponPoseChecksum = 0.0;
    const idMat3 cameraRotation = currentView.viewaxis.Transpose() * drawView.viewaxis;
    for (idEntity *ent = gameLocal.spawnedEntities.Next(); ent; ent = ent->spawnNode.Next()) {
        renderEntity_t *original = ent->GetRenderEntity();
        if (ent->GetModelDefHandle() < 0 || original->allowSurfaceInViewID != drawView.viewID) continue;
        if (original->callback == idEntity::ModelCallback)
            ent->idEntity::UpdateRenderEntity(original, NULL);
        renderEntity_t adjusted = *original;
        adjusted.origin = (original->origin - currentView.vieworg) * cameraRotation + drawView.vieworg;
        adjusted.axis = original->axis * cameraRotation;
        presentationPose_t *pose = ent->entityNumber >= 0 && ent->entityNumber < presentationPoses.Num() ? presentationPoses[ent->entityNumber] : NULL;
        const bool blend = g_interpolateWeapons.GetBool() && pose && pose->entity.GetEntity() == ent &&
            gameLocal.time - pose->time == USERCMD_MSEC && pose->handle == ent->GetModelDefHandle() &&
            pose->model == original->hModel && pose->viewID == original->allowSurfaceInViewID &&
            (!original->callback || original->callback == idEntity::ModelCallback) &&
            ((original->origin - currentView.vieworg) * currentView.viewaxis.Transpose() -
             (pose->origin - previousPresentationView.vieworg) * previousPresentationView.viewaxis.Transpose()).LengthSqr() < 128.0f * 128.0f;
        if (blend) {
            adjusted = *original;
            BlendCameraLocalTransform(pose->origin, pose->axis, original->origin, original->axis,
                currentView, drawView, fraction, adjusted.origin, adjusted.axis);
            adjusted.bounds.AddBounds(pose->bounds);
            BlendPresentationJoints(*pose, *original, adjusted, fraction);
            BlendPresentationBeams(*pose, *original, adjusted, fraction, &currentView, &drawView);
        }
        gameRenderWorld->UpdateEntityDef(ent->GetModelDefHandle(), &adjusted);
        restore.Append(ent);
        if (cvarSystem->GetCVarBool("com_fpsTrace") && adjusted.joints) {
            for (int j = 0; j < adjusted.numJoints; ++j) {
                const idVec3 p = adjusted.joints[j].ToVec3();
                weaponPoseChecksum += p.x + 2.0 * p.y + 3.0 * p.z;
            }
        }
        if (!ent->IsType(hhWeapon::Type)) continue;
        // Leech lighting spans the world beam. Interpolate in world space so a
        // late camera turn cannot move its far end away from the hit location.
        const int worldHandle = GetWorldPresentationLight(ent);
        const renderLight_t *worldLight = worldHandle >= 0 ? gameRenderWorld->GetRenderLight(worldHandle) : NULL;
        if (blend && worldLight && pose->worldLightHandle == worldHandle && pose->worldLight.shader == worldLight->shader) {
            presentationLightRestore_t saved; saved.handle = worldHandle; saved.light = *worldLight;
            lights.Append(saved);
            renderLight_t adjustedLight = *worldLight;
            adjustedLight.origin.Lerp(pose->worldLight.origin, worldLight->origin, fraction);
            adjustedLight.lightRadius.Lerp(pose->worldLight.lightRadius, worldLight->lightRadius, fraction);
            idQuat orientation; orientation.Slerp(pose->worldLight.axis.ToQuat(), worldLight->axis.ToQuat(), fraction);
            adjustedLight.axis = orientation.ToMat3();
            gameRenderWorld->UpdateLightDef(worldHandle, &adjustedLight);
            ++presentationWorldLightCount;
        }
        idList<int> handles;
        static_cast<hhWeapon *>(ent)->GetPresentationLightHandles(handles);
        for (int i = 0; i < handles.Num(); ++i) {
            const renderLight_t *light = gameRenderWorld->GetRenderLight(handles[i]);
            if (!light) continue;
            presentationLightRestore_t saved; saved.handle = handles[i]; saved.light = *light;
            lights.Append(saved);
            renderLight_t adjustedLight = *light;
            adjustedLight.origin = (light->origin - currentView.vieworg) * cameraRotation + drawView.vieworg;
            adjustedLight.axis = light->axis * cameraRotation;
            const int oldIndex = blend ? pose->lightHandles.FindIndex(handles[i]) : -1;
            if (oldIndex >= 0 && pose->lights[oldIndex].shader == light->shader &&
                pose->lights[oldIndex].shaderParms[SHADERPARM_TIMEOFFSET] == light->shaderParms[SHADERPARM_TIMEOFFSET]) {
                BlendCameraLocalTransform(pose->lights[oldIndex].origin, pose->lights[oldIndex].axis,
                    light->origin, light->axis, currentView, drawView, fraction, adjustedLight.origin, adjustedLight.axis);
            }
            gameRenderWorld->UpdateLightDef(handles[i], &adjustedLight);
        }
    }
}

// FX actions own additional renderer definitions, not separate game entities.
// World beams keep their far endpoint fixed while their near endpoint follows
// the displayed weapon joint, including opt-in late mouse look. Only renderer
// node copies are warped; shot traces and damage remain simulation-owned.
static void AttachWorldPresentationBeams(idList<idEntity *> &restore) {
    if (!g_interpolateWeapons.GetBool()) return;
    for (idEntity *ent = gameLocal.spawnedEntities.Next(); ent; ent = ent->spawnNode.Next()) {
        if (!ent->IsType(hhWeaponSoulStripper::Type) || ent->GetModelDefHandle() < 0) continue;
        const renderEntity_t *weapon = gameRenderWorld->GetRenderEntity(ent->GetModelDefHandle());
        if (!weapon || !weapon->allowSurfaceInViewID || !weapon->joints) continue;
        hhBeamSystem *beams[2]; jointHandle_t joints[2];
        const int count = static_cast<hhWeaponSoulStripper *>(ent)->GetPresentationBeams(beams, joints);
        for (int i = 0; i < count; ++i) {
            hhBeamSystem *beam = beams[i];
            if (!beam->IsActivated() || beam->IsHidden() || beam->GetModelDefHandle() < 0 || joints[i] < 0 || joints[i] >= weapon->numJoints) continue;
            const renderEntity_t *source = gameRenderWorld->GetRenderEntity(beam->GetModelDefHandle());
            if (!source || !source->beamNodes || !source->declBeam || source->declBeam->numNodes < 2) continue;
            idMat3 inverseAxis = source->axis;
            if (!inverseAxis.InverseSelf()) continue;
            const int slot = beam->entityNumber;
            while (presentationPoses.Num() <= slot) presentationPoses.Append(NULL);
            if (!presentationPoses[slot]) presentationPoses[slot] = new presentationPose_t;
            presentationPose_t &pose = *presentationPoses[slot];
            pose.drawBeamNodes.SetNum(source->declBeam->numBeams);
            const idVec3 muzzle = weapon->origin + weapon->joints[joints[i]].ToVec3() * weapon->axis;
            renderEntity_t adjusted = *source;
            for (int b = 0; b < source->declBeam->numBeams; ++b) {
                pose.drawBeamNodes[b] = source->beamNodes[b];
                const int last = source->declBeam->numNodes - 1;
                const idVec3 oldEnd = source->beamNodes[b].nodes[last];
                const idVec3 start = source->origin + source->beamNodes[b].nodes[0] * source->axis;
                const idVec3 shift = (muzzle - start) * inverseAxis;
                for (int n = 0; n <= last; ++n) {
                    pose.drawBeamNodes[b].nodes[n] += shift * (1.0f - (float)n / last);
                    idBounds bounds(pose.drawBeamNodes[b].nodes[n]);
                    adjusted.bounds.AddBounds(bounds.Expand(source->declBeam->thickness[b] * 0.5f));
                }
                const float error = (source->origin + pose.drawBeamNodes[b].nodes[0] * source->axis - muzzle).Length();
                presentationBeamAttachmentError = Max(presentationBeamAttachmentError, error);
                presentationBeamEndError = Max(presentationBeamEndError, (pose.drawBeamNodes[b].nodes[last] - oldEnd).Length());
            }
            adjusted.beamNodes = pose.drawBeamNodes.Ptr();
            gameRenderWorld->UpdateEntityDef(beam->GetModelDefHandle(), &adjusted);
            restore.AddUnique(beam);
            ++presentationBeamAttachments;
        }
    }
}

// Rebase attached actions with the displayed master (including a bound bone).
static void ApplyAttachedPresentation(idList<presentationModelRestore_t> &models,
    idList<presentationLightRestore_t> &lights) {
    for (idEntity *ent = gameLocal.spawnedEntities.Next(); ent; ent = ent->spawnNode.Next()) {
        if (!ent->IsType(idEntityFx::Type)) continue;
        idEntity *master = ent->GetBindMaster();
        if (!master || master->GetModelDefHandle() < 0) continue;
        const renderEntity_t *original = master->GetRenderEntity();
        const renderEntity_t *draw = gameRenderWorld->GetRenderEntity(master->GetModelDefHandle());
        if (!draw || !original->allowSurfaceInViewID) continue;
        idVec3 from = original->origin, to = draw->origin;
        idMat3 fromAxis = original->axis, toAxis = draw->axis;
        const int joint = ent->GetBindJoint();
        if (joint >= 0 && joint < original->numJoints && joint < draw->numJoints && original->joints && draw->joints) {
            from += original->joints[joint].ToVec3() * fromAxis;
            to += draw->joints[joint].ToVec3() * toAxis;
            fromAxis = original->joints[joint].ToMat3() * fromAxis;
            toAxis = draw->joints[joint].ToMat3() * toAxis;
        }
        const idMat3 rotation = fromAxis.Transpose() * toAxis;
        const idList<idFXLocalAction> &actions = static_cast<idEntityFx *>(ent)->GetPresentationActions();
        for (int i = 0; i < actions.Num(); ++i) {
            const idFXLocalAction &action = actions[i];
            if (action.modelDefHandle >= 0) {
                presentationModelRestore_t saved; saved.handle = action.modelDefHandle; saved.model = action.renderEntity;
                models.Append(saved);
                renderEntity_t adjusted = saved.model;
                adjusted.origin = (adjusted.origin - from) * rotation + to;
                adjusted.axis *= rotation;
                gameRenderWorld->UpdateEntityDef(saved.handle, &adjusted);
            }
            if (action.lightDefHandle >= 0) {
                presentationLightRestore_t saved; saved.handle = action.lightDefHandle; saved.light = action.renderLight;
                lights.Append(saved);
                renderLight_t adjusted = saved.light;
                adjusted.origin = (adjusted.origin - from) * rotation + to;
                adjusted.axis *= rotation;
                gameRenderWorld->UpdateLightDef(saved.handle, &adjusted);
            }
        }
    }
}

static idCVar g_lateMouse("g_lateMouse", "0", CVAR_GAME | CVAR_BOOL | CVAR_ARCHIVE,
    "preview unconsumed mouse look; normal first person, m_smooth 1; reticle stays on authoritative aim");

static bool ApplyLateMousePresentation(hhPlayer *player, renderView_t &view, idAngles &delta) {
    if (!g_lateMouse.GetBool() || !usercmdGen || player->health <= 0 || player->GuiActive() ||
        player->IsSpiritOrDeathwalking() || player->GetBindMaster() ||
        cvarSystem->GetCVarFloat("com_fpsTestTurn") != 0.0f ||
        !usercmdGen->GetPresentationLook(player->usercmd, delta)) return false;
    idAngles look = player->GetUntransformedViewAngles() + delta;
    look.pitch = idMath::ClampFloat(player->noclip ? -89.0f : pm_minviewpitch.GetFloat(),
        player->noclip ? 89.0f : pm_maxviewpitch.GetFloat(), look.pitch);
    const idMat3 currentAim = player->TransformToPlayerSpace(player->GetUntransformedViewAngles().ToMat3());
    const idMat3 latestAim = player->TransformToPlayerSpace(look.ToMat3());
    idQuat interpolatedAim;
    interpolatedAim.Slerp(previousPresentationAim.ToQuat(), currentAim.ToQuat(), presentationFraction);
    view.viewaxis *= interpolatedAim.ToMat3().Transpose() * latestAim;

    // Reticle marks the simulation's eye trace, including parallax from the
    // interpolated camera. Preview input never changes collision/projectiles.
    const idVec3 target = player->weapon.IsValid() ? player->weapon->GetEyeTraceInfo().endpos :
        player->GetEyePosition() + currentAim[0] * 4096.0f;
    const idVec3 direction = target - view.vieworg;
    const float depth = direction * view.viewaxis[0];
    if (depth > 0.1f) {
        const float scale = 240.0f / idMath::Tan(DEG2RAD(view.fov_y * 0.5f));
        presentationCursorOffset.Set(-(direction * view.viewaxis[1]) * scale / depth,
            -(direction * view.viewaxis[2]) * scale / depth);
    } else {
        presentationCursorOffset.Set(10000.0f, 10000.0f);
    }
    return true;
}
#endif
