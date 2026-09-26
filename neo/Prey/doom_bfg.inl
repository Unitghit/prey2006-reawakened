// BFG behavior adapted from id Software DOOM-3 neo/game/Projectile.cpp.
// Copyright (C) 1999-2011 id Software LLC. GPL-3.0-or-later.
// Uses hhProjectile so collision, gravity and portal traversal remain native.
CLASS_DECLARATION(hhProjectile, hhDoomBFGProjectile)
    EVENT(EV_Collision_Flesh, hhDoomBFGProjectile::Event_Collision_Explode)
    EVENT(EV_Collision_Metal, hhDoomBFGProjectile::Event_Collision_Explode)
    EVENT(EV_Collision_AltMetal, hhDoomBFGProjectile::Event_Collision_Explode)
    EVENT(EV_Collision_Wood, hhDoomBFGProjectile::Event_Collision_Explode)
    EVENT(EV_Collision_Stone, hhDoomBFGProjectile::Event_Collision_Explode)
    EVENT(EV_Collision_Glass, hhDoomBFGProjectile::Event_Collision_Explode)
    EVENT(EV_Collision_Liquid, hhDoomBFGProjectile::Event_Collision_Explode)
    EVENT(EV_Collision_CardBoard, hhDoomBFGProjectile::Event_Collision_Explode)
    EVENT(EV_Collision_Tile, hhDoomBFGProjectile::Event_Collision_Explode)
    EVENT(EV_Collision_Forcefield, hhDoomBFGProjectile::Event_Collision_Explode)
    EVENT(EV_Collision_Pipe, hhDoomBFGProjectile::Event_Collision_Explode)
    EVENT(EV_Collision_Wallwalk, hhDoomBFGProjectile::Event_Collision_Explode)
    EVENT(EV_Collision_Chaff, hhDoomBFGProjectile::Event_Collision_Explode)
END_CLASS

hhDoomBFGProjectile::hhDoomBFGProjectile() : shellHandle(-1), nextDamage(0), beamEnd(0) {
    memset(&shell, 0, sizeof(shell));
}
void hhDoomBFGProjectile::Spawn() {}
hhDoomBFGProjectile::~hhDoomBFGProjectile() { ClearBeams(); }
void hhDoomBFGProjectile::ClearBeams() {
    for (int i=0; i<beams.Num(); ++i) if (beams[i].handle >= 0) gameRenderWorld->FreeEntityDef(beams[i].handle);
    beams.Clear();
    if (shellHandle >= 0) gameRenderWorld->FreeEntityDef(shellHandle);
    shellHandle = -1;
}
void hhDoomBFGProjectile::Save(idSaveGame *f) const {
    f->WriteInt(nextDamage); f->WriteInt(beamEnd);
    f->WriteRenderEntity(shell); f->WriteInt(shellHandle);
    f->WriteInt(beams.Num());
    for (int i=0;i<beams.Num();++i) {
        beams[i].target.Save(f); f->WriteRenderEntity(beams[i].render); f->WriteInt(beams[i].handle);
    }
}
void hhDoomBFGProjectile::Restore(idRestoreGame *f) {
    f->ReadInt(nextDamage); f->ReadInt(beamEnd);
    f->ReadRenderEntity(shell); f->ReadInt(shellHandle);
    if (shellHandle>=0) shellHandle=gameRenderWorld->AddEntityDef(&shell);
    int count; f->ReadInt(count);
    if(count<0 || count>MAX_GENTITIES) gameLocal.Error("Invalid BFG beam count");
    beams.SetNum(count);
    for(int i=0;i<count;++i) {
        beams[i].target.Restore(f); f->ReadRenderEntity(beams[i].render); f->ReadInt(beams[i].handle);
        if(beams[i].handle>=0) beams[i].handle=gameRenderWorld->AddEntityDef(&beams[i].render);
    }
}
void hhDoomBFGProjectile::Launch(const idVec3 &start,const idMat3 &axis,const idVec3 &push,float time,float power,float damage) {
    hhProjectile::Launch(start,axis,push,0,power,damage);
    damagePower=idMath::ClampFloat(1,4,damage);
    AcquireTargets(start,axis);
}
void hhDoomBFGProjectile::AcquireTargets(const idVec3 &start,const idMat3 &axis) {
    ClearBeams();
    nextDamage=gameLocal.time+333; beamEnd=0;
    shell.hModel=renderModelManager->FindModel(spawnArgs.GetString("model_two"));
    shell.axis=axis; shell.origin=start; shell.bounds=shell.hModel->Bounds(&shell);
    for(int c=0;c<4;++c) shell.shaderParms[c]=1;
    shell.noShadow=true; shell.noSelfShadow=true;
    shellHandle=gameRenderWorld->AddEntityDef(&shell);
    idEntity *list[MAX_GENTITIES];
    int count=gameLocal.clip.EntitiesTouchingBounds(idBounds(start).Expand(spawnArgs.GetFloat("damageRadius","2048")),CONTENTS_BODY,list,MAX_GENTITIES);
    for(int i=0;i<count;++i) {
        idEntity *ent=list[i]; idVec3 point;
        if(ent==owner.GetEntity() || !ent->IsType(idActor::Type) || ent->IsHidden() || !ent->IsActive() || !ent->fl.takedamage || ent->health<=0 || !ent->CanDamage(start,point)) continue;
        beam_t b; memset(&b.render,0,sizeof(b.render)); b.target=ent;
        b.render.hModel=renderModelManager->FindModel("_beam");
        b.render.axis=mat3_identity; b.render.origin=start;
        b.render.customSkin=declManager->FindSkin("skins/d3_bfg");
        b.render.noShadow=true; b.render.noSelfShadow=true;
        b.render.bounds.Clear();
        b.render.shaderParms[SHADERPARM_BEAM_WIDTH]=24;
        b.render.shaderParms[SHADERPARM_DIVERSITY]=gameLocal.random.CRandomFloat()*0.75f;
        for(int c=0;c<4;++c) b.render.shaderParms[c]=1;
        for(int c=0;c<3;++c) b.render.shaderParms[SHADERPARM_BEAM_END_X+c]=point[c];
        b.handle=gameRenderWorld->AddEntityDef(&b.render); beams.Append(b);
    }
    BecomeActive(TH_THINK);
    if(cvarSystem->GetCVarBool("d3_bfgTrace")) gameLocal.Printf("BFG launch power=%.0f targets=%d\n",damagePower,beams.Num());
}
void hhDoomBFGProjectile::Think() {
    hhProjectile::Think();
    if(state!=LAUNCHED) { if(beamEnd && gameLocal.time>=beamEnd) {ClearBeams();beamEnd=0;} return; }
    if(spawnArgs.GetBool("rw_bfg_retarget")) {
        spawnArgs.SetBool("rw_bfg_retarget",false);
        AcquireTargets(GetOrigin(),GetAxis());
    }
    SetAxis(idAngles((gameLocal.time&4095)*-360.0f/4096,(gameLocal.time&4095)*-360.0f/4096,0).ToMat3());
    const bool tick=gameLocal.time>=nextDamage;
    for(int i=0;i<beams.Num();++i) {
        idEntity *ent=beams[i].target.GetEntity(); idVec3 point;
        bool visible=ent && ent->health>0 && !ent->IsHidden() && ent->CanDamage(GetOrigin(),point);
        beams[i].render.origin=GetOrigin();
        if(ent) for(int c=0;c<3;++c) beams[i].render.shaderParms[SHADERPARM_BEAM_END_X+c]=ent->GetPhysics()->GetAbsBounds().GetCenter()[c];
        for(int c=0;c<4;++c) beams[i].render.shaderParms[c]=visible ? 1 : 0;
        if(tick && visible) ent->Damage(this,owner.GetEntity(),(point-GetOrigin()).ToNormal(),spawnArgs.GetString("def_damageFreq"),damagePower,INVALID_JOINT);
        gameRenderWorld->UpdateEntityDef(beams[i].handle,&beams[i].render);
    }
    if(tick) nextDamage=gameLocal.time+333;
    if(shellHandle>=0) {
        shell.origin=GetOrigin(); shell.axis=idAngles((gameLocal.time&2047)*-360.0f/2048, (gameLocal.time&2047)*-360.0f/2048,0).ToMat3();
        gameRenderWorld->UpdateEntityDef(shellHandle,&shell);
    }
}
void hhDoomBFGProjectile::Explode(const trace_t *collision,const idVec3 &velocity,int delay) {
    if(!collision || state==EXPLODED || state==FIZZLED || state==COLLIDED) return;
    for(int i=0;i<beams.Num();++i) {
        idEntity *ent=beams[i].target.GetEntity(); idVec3 point;
        if(!ent || ent->IsHidden() || !ent->CanDamage(GetOrigin(),point)) continue;
        ent->Damage(this,owner.GetEntity(),(point-GetOrigin()).ToNormal(),spawnArgs.GetString("def_damage"),damagePower,
            collision->c.id<0 ? CLIPMODEL_ID_TO_JOINT_HANDLE(collision->c.id) : INVALID_JOINT);
        beams[i].render.shaderParms[SHADERPARM_BEAM_WIDTH]=128;
        gameRenderWorld->UpdateEntityDef(beams[i].handle,&beams[i].render);
    }
    const int hitNum = collision->c.entityNum;
    idEntity *hit = hitNum >= 0 && hitNum < MAX_GENTITIES ? gameLocal.entities[hitNum] : NULL;
    spawnArgs.SetBool("rw_bfg_actor_impact",hit && hit->fl.takedamage);
    if(shellHandle>=0) gameRenderWorld->FreeEntityDef(shellHandle);
    shellHandle=-1; beamEnd=gameLocal.time+750;
    hhProjectile::Explode(collision,velocity,Max(delay,750));
    BecomeActive(TH_THINK);
    if(cvarSystem->GetCVarBool("d3_bfgTrace")) gameLocal.Printf("BFG explode power=%.0f actor=%d\n",damagePower,spawnArgs.GetBool("rw_bfg_actor_impact"));
}
void hhDoomBFGProjectile::SplashDamage(const idVec3 &origin,idEntity *attacker,idEntity *ignoreDamage,idEntity *ignorePush,const char *def) {
    // Retail BFG has the additional short-range splash only on damageable hits.
    if(spawnArgs.GetBool("rw_bfg_actor_impact")) gameLocal.RadiusDamage(origin,this,attacker,ignoreDamage,ignorePush,def,damagePower);
}
void hhDoomBFGProjectile::Fizzle() {ClearBeams();hhProjectile::Fizzle();}

void hhDoomBFGProjectile::Portalled(idEntity *portal) {
    hhProjectile::Portalled(portal);
    // Refresh in Think after the portal has finished moving the physics object.
    spawnArgs.SetBool("rw_bfg_retarget",true);
    if(cvarSystem->GetCVarBool("d3_bfgTrace")) gameLocal.Printf("BFG portalled via=%s\n",portal->GetName());
}
