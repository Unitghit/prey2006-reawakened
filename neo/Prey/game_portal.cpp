//**************************************************************************
//**
//** GAME_PORTAL.CPP
//**
//** Game code for Prey-specific portals
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "precompiled.h"
#pragma hdrstop

#include "prey_local.h"

static idCVar g_portalPreserveMotion("g_portalPreserveMotion", "1", CVAR_GAME | CVAR_BOOL,
    "carry the remaining single-player movement step through a portal with exit collision checking");

#define MP_PORTAL_RANGE_DEFAULT		356.0f //rww - was 512, and then it was 256, and now it is 356
#define MAX_PORTAL_BOUNDS			256.0f //used for unchanging bounds in mp

//==========================================================================
//
// hhArtificialPortal
//
//==========================================================================

const idEventDef EV_SetGamePortalState("setPortalState", "dd");

#if GAMEPORTAL_PVS

CLASS_DECLARATION(idEntity, hhArtificialPortal)
	EVENT( EV_SetGamePortalState,	hhArtificialPortal::Event_SetPortalState )
END_CLASS

void hhArtificialPortal::Spawn() {
	areaPortal = gameRenderWorld->FindGamePortal( GetName() );
	if (areaPortal && !gameLocal.isClient) {
		SetPortalState(spawnArgs.GetBool("startOpenPVS"), spawnArgs.GetBool("startOpenSound"));
	}
	fl.networkSync = true;
}

void hhArtificialPortal::SetPortalState(bool openPVS, bool openSound) {
	if (areaPortal && !gameLocal.isClient) {
		int blockMask = (openPVS ? PS_BLOCK_NONE : PS_BLOCK_VIEW) | (openSound ? PS_BLOCK_NONE : PS_BLOCK_SOUND);
		gameLocal.SetPortalState( areaPortal, blockMask );
	}
}

void hhArtificialPortal::Event_SetPortalState(bool openPVS, bool openSound) {
	SetPortalState(openPVS, openSound);
}

void hhArtificialPortal::Save(idSaveGame *savefile) const {
	savefile->WriteInt(areaPortal);
	if ( areaPortal ) {
		savefile->WriteInt( gameRenderWorld->GetPortalState( areaPortal ) );
	}
}

void hhArtificialPortal::Restore(idRestoreGame *savefile) {
	savefile->ReadInt(areaPortal);
	if ( areaPortal ) {
		int portalState;
		savefile->ReadInt( portalState );
		gameLocal.SetPortalState( areaPortal, portalState );
	}
}


#endif


//==========================================================================
//
// hhPortal
//
//==========================================================================

const idEventDef EV_Opened("<portalopened>", NULL);
const idEventDef EV_Closed("<portalclosed>", NULL);
const idEventDef EV_PortalSpark("<portalSpark>", NULL );
const idEventDef EV_PortalSparkEnd("<portalSparkEnd>", NULL );
const idEventDef EV_ShowGlowPortal( "showGlowPortal", NULL );
const idEventDef EV_HideGlowPortal( "hideGlowPortal", NULL );

CLASS_DECLARATION(hhAnimatedEntity, hhPortal)
	EVENT( EV_PostSpawn,   			hhPortal::PostSpawn )
	EVENT( EV_Activate,	   			hhPortal::Event_Trigger)
	EVENT( EV_Opened,				hhPortal::Event_Opened )
	EVENT( EV_Closed,				hhPortal::Event_Closed )
	EVENT( EV_ResetGravity,			hhPortal::Event_ResetGravity )
	EVENT( EV_PortalSpark,			hhPortal::Event_PortalSpark )
	EVENT( EV_PortalSparkEnd,		hhPortal::Event_PortalSparkEnd )
	EVENT( EV_ShowGlowPortal,		hhPortal::Event_ShowGlowPortal )
	EVENT( EV_HideGlowPortal,		hhPortal::Event_HideGlowPortal )
END_CLASS

hhPortal::hhPortal(void) {
	areaPortal = 0;
}

hhPortal::~hhPortal() {
    // Gun endpoints can be removed independently. Unlink surviving endpoints
    // before the raw cameraTarget pointer becomes invalid.
    if (spawnArgs.GetBool("rw_portalGun")) {
        for (idEntity *ent = gameLocal.spawnedEntities.Next(); ent; ent = ent->spawnNode.Next()) {
            if (ent != this && ent->IsType(hhPortal::Type) &&
                ent->spawnArgs.GetBool("rw_portalGun") && ent->cameraTarget == this) {
                ent->cameraTarget = NULL;
                ent->GetRenderEntity()->remoteRenderView = NULL;
                ent->spawnArgs.Delete("cameraTarget");
                ent->UpdateVisuals();
            }
        }
    }

	proximityEntities.Clear(); // Clear the list of potential entities to be portalled
	SAFE_REMOVE(m_portalIdleFx);

#if GAMEPORTAL_PVS
	if (areaPortal && !gameLocal.isClient) {
		gameLocal.SetPortalState( areaPortal, PS_BLOCK_ALL );
	}
#endif
}

void hhPortal::Spawn(void) {
	idBounds bounds;

	fl.clientEvents = true;

	bNoTeleport = spawnArgs.GetBool( "noTeleport");
	bGlowPortal	= spawnArgs.GetBool( "glowPortal"); // This is a glow portal, and so will have fx, sound, particles, etc
	closeDelay = spawnArgs.GetFloat( "closeDelay");
	monsterportal = spawnArgs.GetBool( "monsterportal" );
	distanceToggle = spawnArgs.GetFloat( "distanceToggle" );
	if (gameLocal.isMultiplayer && bGlowPortal/* && distanceToggle == 0.0f*/) { //mp has a default shutoff distance for glow portals.
		//now ignoring distance toggle keys in mp, and forcing it.
		distanceToggle = MP_PORTAL_RANGE_DEFAULT;
	}
	if (!spawnArgs.GetFloat( "distanceCull", "0.0", distanceCull )) { //rww - distance to shut off the vis gameportal but not the render portal
		//if not value specified, default to shaderParm5+4
		if (!renderEntity.shaderParms[5]) {
			distanceCull = 0.0f;
		}
		else {
			distanceCull = renderEntity.shaderParms[5]+4.0f;
		}
	}
	areaPortalCulling = false;
	if ( !gameLocal.isMultiplayer && bGlowPortal ) {
		alertMonsters = spawnArgs.GetBool( "alertMonsters", "0" );
	} else {
		alertMonsters = false;
	}

	if ( bNoTeleport ) {
		GetPhysics()->SetContents( 0 );
	} else {
		GetPhysics()->SetContents( CONTENTS_SOLID );
	}

	// Setup the initial state for the portal
	if(spawnArgs.GetBool("startActive")) { // Start Active
		portalState = PORTAL_OPENED;

		if ( bGlowPortal ) {
			int anim = GetAnimator()->GetAnim("opened");
			GetAnimator()->CycleAnim( ANIMCHANNEL_ALL, anim, gameLocal.time, 0 );
			StartSound( "snd_loop", SND_CHANNEL_ANY );
			PostEventSec( &EV_PortalSpark, gameLocal.random.RandomFloat() );
			SetSkinByName( spawnArgs.GetString( "skin" ) );
		}
	} else { // Start closed
		Hide();
		portalState = PORTAL_CLOSED;
		GetPhysics()->SetContents( 0 ); //rww - if closed, ensure things will not hit and go through
	}

#if GAMEPORTAL_PVS
	const char *gamePortalName = spawnArgs.GetString("gamePortalName", GetName());
	areaPortal = gameRenderWorld->FindGamePortal( gamePortalName );
	if (areaPortal && !gameLocal.isClient) {
		gameLocal.SetPortalState( areaPortal, portalState == PORTAL_CLOSED ? PS_BLOCK_ALL : PS_BLOCK_NONE );
	}
#endif

	// Default to normal gravity.  This could be changed by any zones the portal is within
	SetGravity( gameLocal.GetGravity() );

	BecomeActive( TH_THINK | TH_UPDATEVISUALS | TH_ANIMATE );

	UpdateVisuals();

	PostEventMS( &EV_PostSpawn, 0 );

	fl.networkSync = true; //rww

	if (gameLocal.isMultiplayer) { //rww - if portals grab their renderEntity bounds from the animation, they can cause client-server pvs discrepancies
		renderEntity.bounds[0] = idVec3(-MAX_PORTAL_BOUNDS, -MAX_PORTAL_BOUNDS, -MAX_PORTAL_BOUNDS);
		renderEntity.bounds[1] = idVec3(MAX_PORTAL_BOUNDS, MAX_PORTAL_BOUNDS, MAX_PORTAL_BOUNDS);
	}

	proximityEntities.Clear(); // Clear the list of potential entities to be portalled
}


void hhPortal::PostSpawn( void ) {
	CheckForBuddy();
}

// Adapted from openPREY b6c2cde2.
renderView_t *hhPortal::GetRenderView() {
	// This is the destination coordinate frame, shared with PortalEntity and
	// TransformPortalPresentation. Biasing it by the portal's collision depth
	// makes the visible destination jump backward when the player arrives.
	// Any visibility-area probe offset belongs in the renderer, not this eye.
	return idEntity::GetRenderView();
}

void hhPortal::Save(idSaveGame *savefile) const {
#if GAMEPORTAL_PVS
	savefile->WriteInt( areaPortal );
	if ( areaPortal ) {
		savefile->WriteInt( gameRenderWorld->GetPortalState( areaPortal ) );
	}
#endif
	savefile->WriteInt( portalState );
	savefile->WriteBool( bNoTeleport );
	savefile->WriteBool( bGlowPortal );
	savefile->WriteVec3( portalGravity );
	savefile->WriteFloat( closeDelay );
	savefile->WriteBool( monsterportal );
	savefile->WriteBool( alertMonsters );

	savefile->WriteInt( slavePortals.Num() );		// idList<idEntityPtr<hhPortal> >
	for (int i=0; i<slavePortals.Num(); i++) {
		slavePortals[i].Save(savefile);
	}
	
	masterPortal.Save(savefile);

	savefile->WriteFloat(distanceToggle);
	savefile->WriteFloat(distanceCull);
	savefile->WriteBool(areaPortalCulling);

	savefile->WriteInt( proximityEntities.Num() );
	for( int i = 0; i < proximityEntities.Num(); i++ ) {
		proximityEntities[i].entity.Save( savefile );
		savefile->WriteVec3( proximityEntities[i].lastPortalPoint );
	}

	m_portalIdleFx.Save(savefile);
}

void hhPortal::Restore( idRestoreGame *savefile ) {
	int i, num;

#if GAMEPORTAL_PVS
	savefile->ReadInt( areaPortal );
	if ( areaPortal ) {
		int portalState;
		savefile->ReadInt( portalState );
		gameLocal.SetPortalState( areaPortal, portalState );
	}
#endif
	savefile->ReadInt( (int &)portalState );
	savefile->ReadBool( bNoTeleport );
	savefile->ReadBool( bGlowPortal );
	savefile->ReadVec3( portalGravity );
	savefile->ReadFloat( closeDelay );
	savefile->ReadBool( monsterportal );
	savefile->ReadBool( alertMonsters );

	slavePortals.Clear();
	savefile->ReadInt( num );						// idList<idEntityPtr<hhPortal> >
	slavePortals.SetNum( num );
	for (i=0; i<num; i++) {
		slavePortals[i].Restore(savefile);
	}

	masterPortal.Restore(savefile);

	savefile->ReadFloat(distanceToggle);
	savefile->ReadFloat(distanceCull);
	savefile->ReadBool(areaPortalCulling);

	savefile->ReadInt( num );
	proximityEntities.SetNum( num );
	for( i = 0; i < num; i++ ) {
		proximityEntities[i].entity.Restore( savefile );
		savefile->ReadVec3( proximityEntities[i].lastPortalPoint );
	}

	m_portalIdleFx.Restore(savefile);
}

void hhPortal::WriteToSnapshot( idBitMsgDelta &msg ) const {
	msg.WriteFloat(portalGravity.x);
	msg.WriteFloat(portalGravity.y);
	msg.WriteFloat(portalGravity.z);
	msg.WriteBits(masterPortal.GetSpawnId(), 32);

	msg.WriteFloat(renderEntity.shaderParms[SHADERPARM_MODE]);
	msg.WriteFloat(renderEntity.shaderParms[SHADERPARM_TIMEOFFSET]);

	msg.WriteBits(portalState, 4);
}

void hhPortal::ReadFromSnapshot( const idBitMsgDelta &msg ) {
	portalGravity.x = msg.ReadFloat();
	portalGravity.y = msg.ReadFloat();
	portalGravity.z = msg.ReadFloat();
	masterPortal.SetSpawnId(msg.ReadBits(32));

	renderEntity.shaderParms[SHADERPARM_MODE] = msg.ReadFloat();
	renderEntity.shaderParms[SHADERPARM_TIMEOFFSET] = msg.ReadFloat();

	portalStates_t newPortalState = (portalStates_t)msg.ReadBits(4);
	if ((newPortalState == PORTAL_CLOSED || newPortalState == PORTAL_OPENED) &&
		newPortalState != portalState && portalState != PORTAL_CLOSING && portalState != PORTAL_OPENING) {
		Trigger(this);
	}
}

void hhPortal::ClientPredictionThink( void ) {
	Think();
}

void hhPortal::CheckPlayerDistances(void) {
	// Scale at use time so old saves retain their authored values and changes
	// do not compound across save/load. Opening triggers remain gameplay-owned.
	const float viewDistanceScale = idMath::ClampFloat(1.0f, 16.0f, cvarSystem->GetCVarFloat("r_portalDistanceScale"));
	const float effectiveCull = distanceCull > 0.0f ? distanceCull * viewDistanceScale : distanceCull;
	float closest = idMath::INFINITY;
	hhPortal *targetPortal = NULL;

	if (cameraTarget && cameraTarget->IsType(hhPortal::Type)) { //we want to measure distance from the target portal too if we have one
		targetPortal = static_cast<hhPortal *>(cameraTarget);
	}

	for (int i = 0; i < gameLocal.numClients; i++) { //loop through any active clients and get the closest distance to one.
		idEntity *ent = gameLocal.entities[i];
		if (ent && ent->IsType(hhPlayer::Type)) {
			hhPlayer *pl = static_cast<hhPlayer *>(ent);

			if (pl->health > 0 && !pl->spectating && !pl->InVehicle()) { //don't open for spectators or dead players or players in vehicles
				float d = (pl->GetOrigin()-GetOrigin()).Length();
				if (d < closest) {
					closest = d;
				}
				if (targetPortal) { //check distance from the target portal
					d = (pl->GetOrigin()-targetPortal->GetOrigin()).Length();
					if (d < closest) {
						closest = d;
					}
				}
			}
		}
	}

	if (distanceToggle != 0.0f) { //toggling portal based on distance of player
		if (closest < distanceToggle) { //should be open
			if (portalState == PORTAL_CLOSED) {
				Trigger(this);
			}
		}
		else { //should be closed
			if (portalState == PORTAL_OPENED) {
				Trigger(this);
			}
		}
	}
	
	if (distanceCull != 0.0f) { //toggling only area portal based on distance of player
		if (closest >= effectiveCull) { //should be closed
			if (!areaPortalCulling) {
				areaPortalCulling = true;
				if (!gameLocal.isClient) {
					gameLocal.SetPortalState( areaPortal, PS_BLOCK_ALL );
				}
			}
		}
		else if (areaPortalCulling && (portalState == PORTAL_OPENING || portalState == PORTAL_OPENED)) { //otherwise make sure it's on (if the portal is open)
			if (!gameLocal.isClient) {
				gameLocal.SetPortalState( areaPortal, PS_BLOCK_NONE );
			}
			areaPortalCulling = false;
		}
	}
}


//==========================================================================
//
// hhPortal::Think
//	
//==========================================================================

#define NEAR_CLIP	0 //6.5

static void RW_UpdateGunPortalFloor(hhPortal *portal);
static bool RW_GunPortalEntity(const idEntity *ent);
static bool RW_PortalRagdoll(const idEntity *ent);
static bool RW_TransferPortalRagdoll(hhPortal *portal, idEntity *ent, const idVec3 *crossingPoint);
static void RW_AssistFloorExit(idEntity *, idEntity *, const idVec3 &, const idMat3 &, idVec3 &);
static bool RW_PortalFits(const hhPortal *, const idTraceModel *, const idMat3 &, const idVec3 &, bool);

void hhPortal::ResetGunPortalCrossings() {
    // Previous positions belong to the old portal frame. Do not interpret a
    // relocation as player movement across the newly placed portal plane.
    for (int i = 0; i < proximityEntities.Num(); ++i) {
        idEntity *ent = proximityEntities[i].entity.GetEntity();
        if (ent && ent->IsType(hhPlayer::Type)) static_cast<hhPlayer *>(ent)->SetPortalColliding(false);
    }
    proximityEntities.Clear();
}

static float RW_PortalEyeOffset(const hhPortal *portal, const idEntity *entity) {
    if (!portal->spawnArgs.GetBool("rw_portalGun") || !entity->IsType(hhPlayer::Type) ||
        idMath::Fabs(portal->GetAxis()[0] * -entity->GetPhysics()->GetGravityNormal()) <= 0.95f) return 0;
    const hhPlayer *player = static_cast<const hhPlayer *>(entity);
    return (player->GetEyePosition() - player->GetOrigin()) * portal->GetAxis()[0];
}
static float RW_GroundPortalEyeOffset(const hhPortal *portal, const idEntity *entity) {
    return Max(0.0f, RW_PortalEyeOffset(portal, entity));
}

static bool RW_GroundPortalPartialBlocked(hhPortal *, idEntity *, const idVec3 &, idVec3 * = NULL);
static void RW_GunPortalVisual(hhPortal *portal, bool restart);

void hhPortal::Think( void ) {
    if (spawnArgs.GetBool("rw_portalGun")) {
        if (!cvarSystem->GetCVarBool("g_portalGun")) { Hide(); GetPhysics()->SetContents(0); return; }
        RW_UpdateGunPortalFloor(this);
        if (cameraTarget) {
            idEntity *touching[MAX_GENTITIES];
            const int count=gameLocal.clip.EntitiesTouchingBounds(GetPhysics()->GetAbsBounds().Expand(4), -1, touching, MAX_GENTITIES);
            for (int n=0;n<count;++n) if (RW_PortalRagdoll(touching[n])) {
                idPhysics *af=touching[n]->GetPhysics();
                for (int b=0;b<af->GetNumClipModels();++b) {
                    const idClipModel *limb=af->GetClipModel(b);
                    if (limb && limb->IsTraceModel() && RW_PortalFits(this,limb->GetTraceModel(),af->GetAxis(b),af->GetOrigin(b),false)) {
                        if (af->IsAtRest()) af->Activate();
                        AddProximityEntity(touching[n]); break;
                    }
                }
            }
        }
        Show(); GetPhysics()->SetContents(cameraTarget ? CONTENTS_SOLID : 0);
        if (spawnArgs.GetBool("rw_energy_linked") != (cameraTarget != NULL))
            RW_GunPortalVisual(this, false);
        const int openingEnd = spawnArgs.GetInt("rw_open_end");
        if (openingEnd && gameLocal.time >= openingEnd) {
            spawnArgs.SetInt("rw_open_end", 0);
            RW_GunPortalVisual(this, false);
        }
    }

	int				i;
	idEntity		*hit;
	idPlane			plane;

	if ((distanceCull != 0.0f || distanceToggle != 0.0f) && areaPortal) { //check to turn the areaportal on and off based on distance.
		if (!gameLocal.isClient) {
			CheckPlayerDistances();
		}
		else { //since this is not sync'd, and we don't need to sync it, do an extra check here (for mp)
			if (portalState == PORTAL_OPENED && renderEntity.customSkin && renderEntity.customSkin == declManager->FindSkin(spawnArgs.GetString( "skin_onlyWarp" ))) {
				SetSkinByName( spawnArgs.GetString( "skin" ) );
			}
		}

		if (distanceToggle != 0.0f) { //for pop-open portals, check fx state
			if ((portalState == PORTAL_CLOSED || portalState == PORTAL_CLOSING) && bGlowPortal) {
				//rww - broadcast fx while closed
				if (!m_portalIdleFx.IsValid()) {
					const char *portalIdleFx = spawnArgs.GetString("fx_idleclosed", "fx/portal_closed_idle");
					if (portalIdleFx[0]) {
						hhFxInfo fxInfo;
						fxInfo.SetNormal( GetAxis()[2] );
						fxInfo.RemoveWhenDone(false);

						m_portalIdleFx = SpawnFxLocal(portalIdleFx, GetOrigin(), GetAxis(), &fxInfo, true);
						if (!m_portalIdleFx.IsValid()) { //spawn failure?
							gameLocal.Warning("hhPortal::Think: portal could not spawn fx for fx_idleclosed (%s).", portalIdleFx);
						}
					}
				}

				if (m_portalIdleFx.IsValid() && !m_portalIdleFx->IsActive(TH_THINK)) {
					m_portalIdleFx->Nozzle(true);
				}
			}
			else if (m_portalIdleFx.IsValid() && m_portalIdleFx->IsActive(TH_THINK)) { //if it's not closed/closing and has fx, then stop them
				m_portalIdleFx->Nozzle(false);
			}
		}
	}

	hhAnimatedEntity::Think();

	if (gameLocal.isMultiplayer) { //rww - if portals grab their renderEntity bounds from the animation, they can cause client-server pvs discrepancies
		renderEntity.bounds[0] = idVec3(-MAX_PORTAL_BOUNDS, -MAX_PORTAL_BOUNDS, -MAX_PORTAL_BOUNDS);
		renderEntity.bounds[1] = idVec3(MAX_PORTAL_BOUNDS, MAX_PORTAL_BOUNDS, MAX_PORTAL_BOUNDS);
	}

	if( portalState == PORTAL_CLOSED || bNoTeleport ) {
		return;
	}

	// Force visuals to update until a remote renderview has been created for this portal
	if ( !renderEntity.remoteRenderView ) {	
		BecomeActive( TH_UPDATEVISUALS );
	}

	// Bit of a hack for noclipping players:  If they are close to the portal, then add them to the proximity list automatically
	idPlayer *player = gameLocal.GetLocalPlayer();
	if ( player && player->noclip ) { //rww - note that the local player is NULL for dedicated servers.
		if ( (player->GetOrigin() - GetOrigin()).LengthFast() < 256.0f ) {
			AddProximityEntity( player );
		}
	}

    // Keep a partially entered floor/ceiling portal tracked until the viewpoint,
    // rather than the feet, crosses it. Saved overlaps seed only a genuinely
    // crossed viewpoint; ordinary partial entry keeps its real movement history.
    if (spawnArgs.GetBool("rw_portalGun") && cameraTarget && player &&
        idMath::Fabs(GetAxis()[0] * -player->GetPhysics()->GetGravityNormal()) > 0.95f) {
        const float depth = (player->GetOrigin() - GetOrigin()) * GetAxis()[0];
        const idClipModel *clip = player->GetPhysics()->GetClipModel();
        float frontDepth = depth;
        float backDepth = depth;
        const idBounds &bounds = player->GetPhysics()->GetBounds();
        for (int corner = 0; corner < 8; ++corner) {
            const idVec3 point(bounds[(corner&1)!=0].x, bounds[(corner&2)!=0].y, bounds[(corner&4)!=0].z);
            frontDepth = Max(frontDepth, depth + (point * player->GetPhysics()->GetAxis()) * GetAxis()[0]);
            backDepth = Min(backDepth, depth + (point * player->GetPhysics()->GetAxis()) * GetAxis()[0]);
        }
        if (backDepth <= 0 && frontDepth > 0.25f && clip && clip->IsTraceModel() &&
            RW_PortalFits(this, clip->GetTraceModel(), player->GetPhysics()->GetAxis(), player->GetOrigin(), true)) {
            AddProximityEntity(player);
            const float eyeDepth = depth + RW_PortalEyeOffset(this, player);
            if (eyeDepth <= 0 && player->GetPhysics()->GetLinearVelocity() * GetAxis()[0] <= 0.1f)
                for (int k = 0; k < proximityEntities.Num(); ++k) if (proximityEntities[k].entity.GetEntity() == player)
                    if ((proximityEntities[k].lastPortalPoint - GetOrigin()) * GetAxis()[0] + RW_PortalEyeOffset(this, player) <= 0)
                        proximityEntities[k].lastPortalPoint = player->GetOrigin() + GetAxis()[0] * (0.25f - eyeDepth);
        }
    }

	// Build a plane for the portal surface
	plane.SetNormal(GetPhysics()->GetAxis()[0]);
	plane.FitThroughPoint( GetPhysics()->GetOrigin() + plane.Normal() * (spawnArgs.GetBool("rw_portalGun") ? 0.0f : NEAR_CLIP) );

	idVec3 origin = GetOrigin();
	idMat3 axis = GetAxis();

	for ( i = 0; i < proximityEntities.Num(); i++ ) {
		if ( !proximityEntities[i].entity.IsValid() ) {
			// Remove this entity from the list
			proximityEntities.RemoveIndex( i );
			continue;
		}

		hit = proximityEntities[i].entity.GetEntity();
		idVec3 location = proximityEntities[i].lastPortalPoint;
		idVec3 nextLocation = hit->GetPortalPoint();
        idVec3 clearLocation = nextLocation;
        const bool partialBlocked = RW_GroundPortalEyeOffset(this, hit) > 0 &&
            (nextLocation-location)*GetAxis()[0] <= 0 &&
            RW_GroundPortalPartialBlocked(this, hit, nextLocation, &clearLocation);
        if (clearLocation != nextLocation) {
            hhPlayer *clearedPlayer = static_cast<hhPlayer *>(hit);
            const renderView_t oldView = *clearedPlayer->GetRenderView();
            hit->SetOrigin(clearLocation);
            idVec3 clearanceNormal = clearLocation-nextLocation;
            clearanceNormal.Normalize();
            idVec3 velocity = hit->GetPhysics()->GetLinearVelocity();
            velocity -= clearanceNormal * Min(0.0f, velocity*clearanceNormal);
            hit->GetPhysics()->SetLinearVelocity(velocity);
            clearedPlayer->cameraInterpolator.SetTargetPosition(clearLocation, INTERPOLATE_NONE);
            clearedPlayer->CalculateFirstPersonView();
            clearedPlayer->CalculateRenderView();
            gameLocal.SnapPortalViewModels(oldView);
            nextLocation = clearLocation;
        }
        // Test the portion already through the exit before committing another
        // inward step. Moving back out remains possible without a teleport.
        if (partialBlocked) {
            hhPlayer *blockedPlayer = static_cast<hhPlayer *>(hit);
            const renderView_t oldView = *blockedPlayer->GetRenderView();
            const idVec3 tangentEnd = nextLocation + GetAxis()[0] * ((location-nextLocation)*GetAxis()[0]);
            trace_t slide;
            gameLocal.clip.Translation(slide, location, tangentEnd, hit->GetPhysics()->GetClipModel(),
                hit->GetPhysics()->GetAxis(), hit->GetPhysics()->GetClipMask(), hit);
            // Preserve movement along a blocked entrance when both the source
            // hull and the already-emerged destination slice have clearance.
            const idVec3 stopped = RW_GroundPortalPartialBlocked(this, hit, slide.endpos) ? location : slide.endpos;
            hit->SetOrigin(stopped);
            idVec3 velocity = hit->GetPhysics()->GetLinearVelocity();
            velocity -= GetAxis()[0] * Min(0.0f, velocity*GetAxis()[0]);
            hit->GetPhysics()->SetLinearVelocity(velocity);
            blockedPlayer->cameraInterpolator.SetTargetPosition(stopped, INTERPOLATE_NONE);
            blockedPlayer->CalculateFirstPersonView();
            blockedPlayer->CalculateRenderView();
            gameLocal.SnapPortalViewModels(oldView);
            proximityEntities[i].lastPortalPoint = stopped;
            continue;
        }
		proximityEntities[i].lastPortalPoint = nextLocation;
		if ( !AttemptPortal( plane, hit, location, nextLocation ) ) {
			proximityEntities.RemoveIndex( i );

			// If the entity is a player, then inform the player that they are no longer close to a portal
			if ( hit->IsType( hhPlayer::Type ) ) {
				hhPlayer *player = static_cast<hhPlayer *>(hit);
				player->SetPortalColliding( false );
			}
		}	
	}
}

bool hhPortal::AttemptPortal( idPlane &plane, idEntity *hit, idVec3 location, idVec3 nextLocation ) {

    if (spawnArgs.GetBool("rw_portalGun") && !RW_GunPortalEntity(hit)) return false;

	// Don't try to portal self
	if( hit == this ) {
		return false;
	}		

	if ( hit->IsBound() ) {	// Do not portal bound objects -- let the master portalling handle bound entities
		return false;
	}

	// Don't allow idMovers to portal.  TODO:  Restrict other entities?
	if ( hit->IsType( idMover::Type ) || hit->IsType(hhVehicle::Type) ) { //rww - do not allow shuttles either
		return false;
	}

    const float eyeOffset = RW_PortalEyeOffset(this, hit);
    const bool sweptEyeCrossing = eyeOffset != 0 &&
        plane.Distance(location) + eyeOffset > 0 && plane.Distance(nextLocation) + eyeOffset <= 0;
    // Reorienting the player hull after a floor exit can move the tracked
    // portal point backwards even while the player is travelling outwards.
    // Gun portals are stationary: this is not a physical return crossing.
    if (spawnArgs.GetBool("rw_portalGun") && hit->IsType(hhPlayer::Type) && eyeOffset != 0 &&
        hit->GetPhysics()->GetLinearVelocity()*plane.Normal() > 0.1f) return false;

	if (hit->IsType(hhPlayer::Type)) { //rww - don't portal dead players
		hhPlayer *pl = static_cast<hhPlayer *>(hit);
		if (pl->health <= 0) {
			return false;
		}

		// Check if the player is intersecting this portal.  If not, then don't try to portal it
		if ( !sweptEyeCrossing && !GetPhysics()->GetAbsBounds().IntersectsBounds( pl->GetPhysics()->GetAbsBounds() ) ) {
			return false;
		}
	}

    idPlane crossingPlane = plane;
    if (eyeOffset != 0) crossingPlane.FitThroughPoint(GetOrigin() - plane.Normal()*eyeOffset);
    // NPC ownership changes at the hull center while the two visual pieces
    // remain joined at the opening. Feet entering alone no longer teleports
    // the whole model, and walking below the raised rim still crosses reliably.
    if (spawnArgs.GetBool("rw_portalGun") && hit->IsType(idAI::Type) &&
        hit->GetPhysics()->IsType(idPhysics_Monster::Type)) {
        const idVec3 center = hit->GetPhysics()->GetBounds().GetCenter() * hit->GetPhysics()->GetAxis();
        crossingPlane.FitThroughPoint(GetOrigin() - plane.Normal() * (center * plane.Normal()));
    }
    int side = crossingPlane.Side( location );
	if ( side == PLANESIDE_ON || side == PLANESIDE_CROSS ) {
		side = PLANESIDE_BACK;
	}

	int nextSide = crossingPlane.Side( nextLocation );
	if ( nextSide == PLANESIDE_ON || nextSide == PLANESIDE_CROSS ) {
		nextSide = PLANESIDE_BACK;
	}

	if( side == PLANESIDE_BACK ) { // On the backside, remove this entity from the list
		return false;
	} else if ( side == nextSide ) { // Entirely on one side
		// Check if the entity is too far from the plane and remove it from the list
		if ( !GetPhysics()->GetAbsBounds().IntersectsBounds( hit->GetPhysics()->GetAbsBounds() ) ) {
			return false;
		}

		return true;
	}		

	// Compute the location on the plane where the entity would hit
	float scale;
	idVec3 dir = nextLocation - location;
	crossingPlane.RayIntersection( location, dir, scale );

    if (hit->IsType(hhPlayer::Type) && cvarSystem->GetCVarBool("com_fpsTrace")) {
        const idVec3 v = hit->GetPhysics()->GetLinearVelocity();
        gameLocal.Printf("PORTAL_MOTION %d step %.6f fraction %.6f remaining %.6f speed %.6f\n",
            gameLocal.time, dir.Length(), scale, (1.0f-scale)*dir.Length(), v.Length());
    }
    // The legacy path drops the remainder of this tick's movement. Players
    // use the actual endpoint and the same rigid transform as their velocity.
    const idVec3 crossingPoint = location + dir * scale;
    if (eyeOffset != 0 && cvarSystem->GetCVarBool("com_fpsTrace"))
        gameLocal.Printf("PORTAL_EYE_CROSS feet=%.3f eye=%.3f\n", (crossingPoint-GetOrigin())*plane.Normal(), (crossingPoint-GetOrigin())*plane.Normal()+eyeOffset);
    if (spawnArgs.GetBool("rw_portalGun")) {
        const idClipModel *clip = hit->GetPhysics()->GetClipModel();
        if (!clip || !clip->IsTraceModel() || !RW_PortalFits(this, clip->GetTraceModel(), hit->GetPhysics()->GetAxis(), crossingPoint, hit->IsType(hhPlayer::Type))) return false;
    }
    const bool carryMotion = !gameLocal.isMultiplayer && ((hit->IsType(hhPlayer::Type) && g_portalPreserveMotion.GetBool()) ||
        (spawnArgs.GetBool("rw_portalGun") && !hit->IsType(hhProjectile::Type)));
    const bool portalled = carryMotion ? PortalEntity(hit, nextLocation, &crossingPoint) :
        PortalEntity(hit, location + dir * scale * 1.01f);
    if (!portalled) {
        if (spawnArgs.GetBool("rw_portalGun")) {
            // A blocked exit stops inward movement, not movement along the
            // opening. Rewinding the entire step traps an edge entrant because
            // gravity retries the rejected crossing every tick, also undoing
            // the player's attempt to move toward the clear center.
            idVec3 stopped = location;
            if (eyeOffset > 0) {
                const idVec3 tangentEnd = nextLocation + plane.Normal() * ((location-nextLocation)*plane.Normal());
                trace_t slide;
                gameLocal.clip.Translation(slide, location, tangentEnd, hit->GetPhysics()->GetClipModel(),
                    hit->GetPhysics()->GetAxis(), hit->GetPhysics()->GetClipMask(), hit);
                if (!RW_GroundPortalPartialBlocked(this, hit, slide.endpos)) stopped = slide.endpos;
            }
            hit->SetOrigin(stopped);
            idVec3 velocity = hit->GetPhysics()->GetLinearVelocity();
            velocity -= plane.Normal() * Min(0.0f, velocity * plane.Normal());
            hit->GetPhysics()->SetLinearVelocity(velocity);
            if (RW_PortalRagdoll(hit)) for (int b=1;b<hit->GetPhysics()->GetNumClipModels();++b) {
                idVec3 limbVelocity=hit->GetPhysics()->GetLinearVelocity(b);
                limbVelocity-=plane.Normal()*Min(0.0f,limbVelocity*plane.Normal());
                hit->GetPhysics()->SetLinearVelocity(limbVelocity,b);
            }
            if (eyeOffset != 0) {
                hhPlayer *player = static_cast<hhPlayer *>(hit);
                const renderView_t oldView = *player->GetRenderView();
                player->cameraInterpolator.SetTargetPosition(stopped, INTERPOLATE_NONE);
                player->CalculateFirstPersonView();
                player->CalculateRenderView();
                gameLocal.SnapPortalViewModels(oldView);
            }
        }
        return false;
    }

	// Add this entity to the destination portal's proximityEntity list
	if (cameraTarget && cameraTarget->IsType(hhPortal::Type)) {
		hhPortal *targetPortal = static_cast<hhPortal *>(cameraTarget);
		targetPortal->CollideWithPortal( hit ); // CJR PCF 04/26/06:  Previously CheckPortal
        if (spawnArgs.GetBool("rw_portalGun")) {
            // Exit occupancy queries may have registered the entity while it
            // was still in the source world. Rebase that history after transfer
            // so the coordinate change cannot look like a second crossing.
            for (int k = 0; k < targetPortal->proximityEntities.Num(); ++k)
                if (targetPortal->proximityEntities[k].entity.GetEntity() == hit)
                    targetPortal->proximityEntities[k].lastPortalPoint = hit->GetPortalPoint();
        }
	}

	return false;
}

void hhPortal::PortalProjectile( hhProjectile *projectile, idVec3 collideLocation, idVec3 nextLocation ) {
	idPlane plane;
	plane.SetNormal(GetPhysics()->GetAxis()[0]);
	plane.FitThroughPoint( GetPhysics()->GetOrigin() );

	AttemptPortal( plane, projectile, collideLocation, nextLocation );
}

//==========================================================================
//==========================================================================
void hhPortal::CheckForBuddy() {
	const char *	buddyName;
	idEntity * 		foundEntity;
	hhPortal*		buddy;
	
	// Check for a buddy portal name
	buddyName = spawnArgs.GetString( "partner", NULL );
	if ( buddyName ) {
		gameLocal.Warning( "Portal %s has 'partner' key.  Please change this to 'buddy'", GetName() );
	}
	else {
		buddyName = spawnArgs.GetString( "buddy" );	
	}
	if ( !buddyName || !buddyName[ 0 ] ) {
		return;
	}
	
	// Get the actual entity
	foundEntity = gameLocal.FindEntity( buddyName );	
	if ( !foundEntity ) {
		return;
	}

	if ( foundEntity->IsType( hhPortal::Type ) ) {

		buddy = (hhPortal *) foundEntity;

		if (buddy->spawnArgs.FindKey("buddy")) {
			gameLocal.Error( "Portals %s and %s both have 'buddy' key set.  Use only on master", GetName(), buddy->GetName() );
			return;
		}

		// Don't link up to buddy if a monster portal, just points to portal room
		if ( !monsterportal || closeDelay == 0.0f ) {
			//? OK, let's be lazy, assume only 2 portals are buddied.  Make us the master
			buddy->SetMasterPortal( this );
			//! Set the cameraTargets of each, then update each
			buddy->spawnArgs.Set( "cameraTarget", name.c_str() );
			buddy->ProcessEvent( &EV_UpdateCameraTarget );
		}

		AddSlavePortal( buddy );
	}

	spawnArgs.Set( "cameraTarget", buddyName );
	ProcessEvent( &EV_UpdateCameraTarget );
}


//=============================
// hhPortal::TriggerTargets
//=============================
void hhPortal::TriggerTargets() {
	const char *	key = "";
	idEntity *		entity = NULL;
	idList< idStr > entityNames;
	

	// Find the right key based on our state
	if ( portalState == PORTAL_OPENING ) {
		key = "targetOpening";
	}
	else if ( portalState == PORTAL_OPENED ) {
		key = "targetOpened";
	}
	else if ( portalState == PORTAL_CLOSING ) {
		key = "targetClosing";
	}
	else if ( portalState == PORTAL_CLOSED ) {
		key = "targetClosed";
	}

	//gameLocal.Printf( "Keying %s\n", key );
	
	// Loop through the entities, spawning each one
	hhUtils::GetValues( spawnArgs, key, entityNames, true );
	
	for ( int i = 0; i < entityNames.Num(); ++i ) {
		//gameLocal.Printf( "Gonna trigger %s\n", entityNames[ i ].c_str() );
		entity = gameLocal.FindEntity( entityNames[ i ] );
		if ( entity ) {
			entity->PostEventMS( &EV_Activate, 0, this );
		}
	} 
}


//==========================================================================
//
// hhPortal::CheckPortal
//
// If this entity can be portaled, typically called from the low-level physics clip functions
//
// CJR PCF 04/26/06:  Changed this function to separate CheckPortal and CollideWithPortal
//==========================================================================

bool hhPortal::CheckPortal( const idEntity *other, int contentMask ) {
	if ( contentMask & CONTENTS_GAME_PORTAL ) { // Check if the other entity should clip against the portal
		return false;
	}

	if ( !other ) {
		return true;
	}

	if ( other->fl.noPortal || (spawnArgs.GetBool("rw_portalGun") && !RW_GunPortalEntity(other)) ) {
		return false; // Do not allow this entity to portal, make it collide with the portal instead
	}

	return true;
}

bool hhPortal::CheckPortal( const idClipModel *mdl, int contentMask ) {
	if ( contentMask & CONTENTS_GAME_PORTAL ) { // Check if the other entity should clip against the portal
		return false;
	}

	if ( !mdl ) {
		return true;
	}

	return CheckPortal( mdl->GetEntity(), contentMask );
}

//==========================================================================
//
// hhPortal::CollideWithPortal
//
// Called from low-level clip functions, actually collide the entity with the portal
//
// CJR PCF 04/26/06:  Formerly part of CheckPortal
//==========================================================================

void hhPortal::CollideWithPortal( const idEntity *other ) {
	if ( !other ) {
		return;
	}

	if ( other->IsType( hhProjectile::Type ) ) { // Projectiles move so fast that they should get portaled next time they think
		// Only add this projectile to the collided list if it hits the front side of the portal
		idPlane plane;
		plane.SetNormal(GetPhysics()->GetAxis()[0]);
		plane.FitThroughPoint( GetPhysics()->GetOrigin() );

		int side = plane.Side( other->GetOrigin() );
		if ( side == PLANESIDE_FRONT ) { // It's on the front, so add this portal to the list
			hhProjectile *projectile = (hhProjectile *)(other);

			// Check if the projectile will actually impact the portal
			idVec3 end = projectile->GetOrigin() + projectile->GetPhysics()->GetLinearVelocity();	

			if ( plane.LineIntersection( projectile->GetOrigin(), end ) ) { // Projectile collides with the portal
				projectile->SetCollidedPortal( this, projectile->GetPortalPoint(), projectile->GetPhysics()->GetLinearVelocity() );
			}
		}
	} else {
		AddProximityEntity( other );
	}	
}

void hhPortal::CollideWithPortal( const idClipModel *mdl ) {
	if ( !mdl ) {
		return;
	}

	CollideWithPortal( mdl->GetEntity() );
}

//==========================================================================
//
// hhPortal::AddProximityEntity
//
// Saves this entity on a list, and will check if it can be portalled
// the next time the portal thinks
//==========================================================================

void hhPortal::AddProximityEntity( const idEntity *other) {
	// Go through the list and guarantee that this entity isn't in multiple times
	// note:  cannot use IdList::AddUnique, because the lastPortalPoint might be different during this add
	for( int i = 0; i < proximityEntities.Num(); i++ ) {
		if ( proximityEntities[i].entity.GetEntity() == other ) {
			return;
		}
	}

	// Add this entity to the potential portal list
	proximityEntity_t prox;
	prox.entity = other;
	prox.lastPortalPoint = ((idEntity *)(other))->GetPortalPoint();

	proximityEntities.Append( prox );

	// If the entity is a player, then inform the player that they are close to this portal
	// needed for weapon projectile firing
	if ( other->IsType( hhPlayer::Type ) ) {
		hhPlayer *player = (hhPlayer *)(other);

		player->SetPortalColliding( true );
	}
}

//==========================================================================
//
// hhPortal::PortalEntity
//
// To rotate a vector from one portal space into another:
//	    transform vector into Source Portal Space (mul by axis transpose)
//		flip X & Y (only flip X for angles, not for position)
//		transform vector into Destination Portal Space
//==========================================================================

void PortalRotate( idVec3 &vec, const idMat3 &sourceTranspose, const idMat3 &dest, const bool flipX ) {
	vec *= sourceTranspose;
	vec.y *= -1;
	if ( flipX ) {
		vec.x *= -1;
	}
	vec *= dest;
}

// Use the same linked transform as native projectiles, with an explicit
// front-facing aperture test for the portal gun's lightweight visual shot.
// Rendering-only volume: the portion behind the entrance plane and inside
// its aperture belongs at the linked exit. No collision or ownership changes.
bool hhPortal::GetBodyPortalTransform(const idBounds &worldBounds, const idVec3 &eye,
    idVec3 &source, idVec3 &destination, idMat3 &rotation, idPlane planes[17], int &count, float &distance) const {
    if (portalState != PORTAL_OPENED || bNoTeleport || !cameraTarget || IsHidden()) return false;
    source = GetOrigin(); destination = cameraTarget->GetOrigin();
    const idMat3 axis = GetAxis(), inverse = axis.Transpose();
    distance = (eye - source) * axis[0];
    if (distance < -1.0f) return false;
    idVec3 points[8]; worldBounds.ToPoints(points);
    idBounds local; local.Clear();
    for (int i = 0; i < 8; ++i) local.AddPoint((points[i] - source) * inverse);
    const idBounds &aperture = GetPhysics()->GetBounds();
    if (local[0].x >= 0.01f || local[1].x < -2.0f || local[1].y < aperture[0].y ||
        local[0].y > aperture[1].y || local[1].z < aperture[0].z || local[0].z > aperture[1].z) return false;
    rotation = mat3_identity;
    for (int i = 0; i < 3; ++i) PortalRotate(rotation[i], inverse, cameraTarget->GetAxis(), true);
    count = 1;
    planes[0].SetNormal(-axis[0]); planes[0].FitThroughPoint(source);
    if (spawnArgs.GetBool("rw_portalGun")) {
        // Match the visible oval rather than the deliberately lenient player hull.
        for (int i = 0; i < 16; ++i) {
            const float a = idMath::TWO_PI * i / 16.0f, b = idMath::TWO_PI * (i+1) / 16.0f;
            const idVec3 p = source + axis[1] * (39.0f * idMath::Cos(a)) + axis[2] * (49.0f * idMath::Sin(a));
            const idVec3 q = source + axis[1] * (39.0f * idMath::Cos(b)) + axis[2] * (49.0f * idMath::Sin(b));
            planes[count].FromPoints(p, q, p + axis[0]);
            if (planes[count].Distance(source) < 0) planes[count] = -planes[count];
            ++count;
        }
    } else {
        for (int d = 1; d <= 2; ++d) for (int side = 0; side < 2; ++side) {
            planes[count].SetNormal(axis[d] * (side ? -1.0f : 1.0f));
            planes[count++].FitThroughPoint(source + axis[d] * aperture[side][d]);
        }
    }
    return true;
}

bool hhPortal::TracePortalShot(const idVec3 &start, const idVec3 &end, float &fraction,
    idVec3 &remote, idMat3 &rotation) const {
    if (portalState != PORTAL_OPENED || bNoTeleport || !cameraTarget) return false;
    const idVec3 normal = GetAxis()[0];
    const float a = (start-GetOrigin())*normal, b = (end-GetOrigin())*normal;
    if (a < 0.001f || b > 0.001f || a-b < 0.001f) return false;
    fraction = idMath::ClampFloat(0, 1, a/(a-b));
    const idVec3 point = start+(end-start)*fraction;
    const idVec3 local = (point-GetOrigin())*GetAxis().Transpose();
    const idBounds &bounds = GetPhysics()->GetBounds();
    if (local.y < bounds[0].y || local.y > bounds[1].y || local.z < bounds[0].z || local.z > bounds[1].z) return false;
    if (spawnArgs.GetBool("rw_portalGun") && (Square(local.y/39.0f)+Square(local.z/49.0f) > 1.0f)) return false;
    rotation = mat3_identity;
    for (int i = 0; i < 3; ++i) PortalRotate(rotation[i], GetAxis().Transpose(), cameraTarget->GetAxis(), true);
    remote = (point-GetOrigin())*rotation+cameraTarget->GetOrigin();
    return true;
}

bool hhPortal::GetLighterTransform(const idVec3 &origin, const idVec3 &eye, float range, idVec3 &remote,
    idMat3 &rotation, idPlane planes[5]) const {
    if (portalState != PORTAL_OPENED || bNoTeleport || !cameraTarget) return false;
    const idMat3 inverse = GetAxis().Transpose();
    const idVec3 local = (origin - GetOrigin()) * inverse;
    // The flame can cross before the eye. Keep the remote contribution until
    // the rendered eye changes rooms, not until the offset flame hits the plane.
    const float eyeDistance = (eye - GetOrigin()) * GetAxis()[0];
    if (eyeDistance < -0.01f || eyeDistance >= range || idMath::Fabs(local.x) >= range) return false;
    const idBounds &bounds = GetPhysics()->GetBounds();
    const float y = idMath::ClampFloat(bounds[0].y, bounds[1].y, local.y);
    const float z = idMath::ClampFloat(bounds[0].z, bounds[1].z, local.z);
    if ((local - idVec3(0, y, z)).LengthSqr() >= range * range) return false;
    rotation = mat3_identity;
    for (int i = 0; i < 3; ++i) PortalRotate(rotation[i], inverse, cameraTarget->GetAxis(), true);
    remote = (origin - GetOrigin()) * rotation + cameraTarget->GetOrigin();
    idVec3 corners[4];
    for (int i = 0; i < 4; ++i) {
        const idVec3 corner(0, bounds[(i == 1 || i == 2) ? 1 : 0].y, bounds[i >= 2 ? 1 : 0].z);
        corners[i] = (corner * GetAxis()) * rotation + cameraTarget->GetOrigin();
    }
    const idVec3 forward = -GetAxis()[0] * rotation;
    const idVec3 inside = (corners[0] + corners[2]) * 0.5f + forward;
    // Keep the aperture cone on its entrance side at the singular crossing.
    // The shading origin remains the actual transformed flame position.
    idVec3 apertureLocal = local;
    apertureLocal.x = Max(0.1f, apertureLocal.x);
    const idVec3 apertureOrigin = (apertureLocal * GetAxis()) * rotation + cameraTarget->GetOrigin();
    planes[0].SetNormal(forward);
    planes[0].FitThroughPoint(cameraTarget->GetOrigin());
    for (int i = 0; i < 4; ++i) {
        if (!planes[i+1].FromPoints(apertureOrigin, corners[i], corners[(i+1)%4])) return false;
        if (planes[i+1].Distance(inside) < 0) planes[i+1] = -planes[i+1];
    }
    return true;
}

bool hhPortal::GetWeaponLightingTransform(const idVec3 &eye, float range, idVec3 &remoteEye,
    idVec3 &destination, idMat3 &rotation, float &distance) const {
    if (portalState != PORTAL_OPENED || bNoTeleport || !cameraTarget) return false;
    const idMat3 inverse = GetAxis().Transpose();
    const idVec3 local = (eye - GetOrigin()) * inverse;
    distance = local.x;
    if (distance < 0.0f || distance >= range) return false;
    const idBounds &bounds = GetPhysics()->GetBounds();
    if (local.y < bounds[0].y || local.y > bounds[1].y || local.z < bounds[0].z || local.z > bounds[1].z) return false;
    destination = cameraTarget->GetOrigin();
    rotation = mat3_identity;
    for (int i = 0; i < 3; ++i) PortalRotate(rotation[i], inverse, cameraTarget->GetAxis(), true);
    remoteEye = (eye - GetOrigin()) * rotation + destination;
    return true;
}

bool hhPortal::PortalEntity( idEntity *ent, const idVec3 &point, const idVec3 *crossingPoint ) {
	idMat3 sourceAxis;
	idMat3 destAxis;
	idMat3 newEntAxis;

	if ( !ent ) {
		return(false);
	}

	if ( cameraTarget ) {
        if (spawnArgs.GetBool("rw_portalGun") && RW_PortalRagdoll(ent))
            return RW_TransferPortalRagdoll(this,ent,crossingPoint);
		sourceAxis = GetAxis().Transpose();
		destAxis = cameraTarget->GetAxis();

		// Compute new location
		idVec3 newLocation = point - GetOrigin();
		PortalRotate( newLocation, sourceAxis, destAxis, crossingPoint != NULL );
		newLocation += cameraTarget->GetOrigin();

        const bool traceAlignment = ent->IsType(hhPlayer::Type) && cvarSystem->GetCVarBool("com_fpsTrace");
        const idVec3 renderBias = traceAlignment ? cameraTarget->GetRenderView()->vieworg - cameraTarget->GetOrigin() : vec3_origin;
        const idVec3 visibleLocation = newLocation + renderBias;

		// Compute new axis
        // Match the hull used by source cutouts and partial destination checks.
        // Looking around must not change whether that hull fits at the exit.
		newEntAxis = spawnArgs.GetBool("rw_portalGun") && ent->IsType(hhPlayer::Type) ?
            ent->GetPhysics()->GetAxis() : ent->GetAxis();

		// Rotate the vector into new portal space
		PortalRotate( newEntAxis[0], sourceAxis, destAxis, true );
		PortalRotate( newEntAxis[1], sourceAxis, destAxis, true );
		PortalRotate( newEntAxis[2], sourceAxis, destAxis, true );

        // Monster movement restores gravity alignment every tick. Commit the
        // same upright hull here rather than letting that correction rotate an
        // upside-down floor-exit body through the supporting floor next tick.
        const bool gunNPC = spawnArgs.GetBool("rw_portalGun") && ent->IsType(idAI::Type) &&
            ent->GetPhysics()->IsType(idPhysics_Monster::Type);
        idVec3 npcOriginShift = vec3_origin;
        if (gunNPC) {
            const idMat3 mappedAxis = newEntAxis;
            idVec3 up = -cameraTarget->GetGravity();
            if (up.Normalize() > 0.01f && up * destAxis[0] > 0.95f) {
                idVec3 forward = newEntAxis[0] - up * (newEntAxis[0] * up);
                if (forward.Normalize() < 0.01f) {
                    forward = newEntAxis[1] - up * (newEntAxis[1] * up);
                    forward.Normalize();
                }
                newEntAxis[0] = forward;
                newEntAxis[1] = up.Cross(forward);
                newEntAxis[2] = up;
            }
            const idVec3 center = ent->GetPhysics()->GetBounds().GetCenter();
            npcOriginShift = center * mappedAxis - center * newEntAxis;
            newLocation += npcOriginShift;
        }
		
        bool continuous = true;
        if (crossingPoint && ent->GetPhysics()->GetClipModel()) {
            idVec3 exitStart = *crossingPoint - GetOrigin();
            PortalRotate(exitStart, sourceAxis, destAxis, true);
            exitStart += cameraTarget->GetOrigin() + npcOriginShift;
            trace_t exitTrace;
            // Preserve the teleport's world/static collision and telefrag
            // semantics, while preventing the remaining step crossing a wall.
            const bool remainderBlocked = spawnArgs.GetBool("rw_portalGun") ?
                gameLocal.clip.Translation(exitTrace, exitStart, newLocation, ent->GetPhysics()->GetClipModel(), newEntAxis, ent->GetPhysics()->GetClipMask(), ent) :
                gameLocal.clip.TranslationWithExceptions(exitTrace, exitStart, newLocation, NULL, ent->GetPhysics()->GetClipModel(), newEntAxis, ent->GetPhysics()->GetClipMask(), ent);
            if (remainderBlocked) {
                newLocation = exitTrace.endpos;
                continuous = false;
            }
            if (cvarSystem->GetCVarBool("com_fpsTrace"))
                gameLocal.Printf("PORTAL_REMAINDER %d fraction %.6f distance %.6f\n", gameLocal.time,
                    exitTrace.fraction, (newLocation-exitStart).Length());
        }
        // Resolve a shallow edge snag before committing a gun-portal crossing.
        // Search only along the opening, and require clear source travel plus
        // destination occupancy and a reverse sweep back to the contact skin.
        // This cannot skip a wall to reach an unrelated empty space.
        if (spawnArgs.GetBool("rw_portalGun") && ent->IsType(hhPlayer::Type)) {
            idClipModel *hull = ent->GetPhysics()->GetClipModel();
            trace_t occupied;
            if (hull && gameLocal.clip.Translation(occupied, newLocation, newLocation, hull, newEntAxis,
                    ent->GetPhysics()->GetClipMask(), ent)) {
                bool cleared = false;
                const float stepLimit = Min(pm_stepsize.GetFloat(), pm_bboxwidth.GetFloat()*0.5f);
                const bool actorClip = (occupied.c.contents & CONTENTS_PLAYERCLIP) && !(occupied.c.contents & CONTENTS_SOLID);
                const float limit = actorClip ? pm_bboxwidth.GetFloat() : stepLimit;
                // A lower floor-aligned destination can need more than a normal
                // step to fit the standing hull. Extra travel is upward only and
                // must end on a verified walkable floor, with clear source travel.
                const float floorLimit = Max(stepLimit, Min(32.0f, pm_normalheight.GetFloat()*0.5f));
                for (float distance = 2; distance <= Max(limit, floorLimit) && !cleared; distance += 2) {
                    for (int sample = 0; sample < 16 && !cleared; ++sample) {
                        if (distance > limit && sample != 0) continue;
                        const float angle = sample*(idMath::TWO_PI/16.0f);
                        const idVec3 offset = (destAxis[2]*idMath::Cos(angle) + destAxis[1]*idMath::Sin(angle))*distance;
                        const idVec3 candidate = newLocation+offset;
                        trace_t test;
                        if (gameLocal.clip.Translation(test, candidate, candidate, hull, newEntAxis,
                                ent->GetPhysics()->GetClipMask(), ent)) continue;
                        gameLocal.clip.Translation(test, candidate, newLocation, hull, newEntAxis,
                            ent->GetPhysics()->GetClipMask(), ent);
                        const float unresolved = (test.endpos-newLocation).Length();
                        idVec3 exitUp = -cameraTarget->GetGravity();
                        exitUp.Normalize();
                        // A raised/sloping exit floor needs ordinary step-up
                        // clearance, not the two-unit allowance for wall skins.
                        // A near-upright wall may tilt the portal-plane up vector.
                        // Only its upward sample ending on a walkable
                        // supporting face qualifies; ceilings and walls do not.
                        const bool floorStep = sample == 0 && offset*exitUp > distance*0.95f &&
                            test.fraction > 0.0f && test.fraction < 1.0f &&
                            test.c.normal*exitUp > 0.7f && unresolved <= floorLimit;
                        // Some maps use tall invisible collision columns near
                        // ceiling trim. Allow a bounded offset within the opening,
                        // without exempting those columns from normal collision.
                        const bool clipClearance = actorClip && (test.c.contents & CONTENTS_PLAYERCLIP) &&
                            !(test.c.contents & CONTENTS_SOLID) && unresolved <= limit;
                        if (test.fraction <= 0.0f || (distance > limit && !floorStep) || (unresolved > 2.0f && !floorStep && !clipClearance)) continue;
                        idVec3 sourceOffset = offset;
                        PortalRotate(sourceOffset, destAxis.Transpose(), GetAxis(), true);
                        const idVec3 sourceCandidate = point+sourceOffset;
                        if (!RW_PortalFits(this, hull->GetTraceModel(), ent->GetPhysics()->GetAxis(), sourceCandidate, true)) continue;
                        if (gameLocal.clip.Translation(test, point, sourceCandidate, hull, ent->GetPhysics()->GetAxis(),
                                ent->GetPhysics()->GetClipMask(), ent)) continue;
                        newLocation = candidate;
                        cleared = true;
                        if (traceAlignment) gameLocal.Printf("PORTAL_EXIT_CLEARANCE distance=%.3f\n", distance);
                    }
                }
            }
        }
		// Actually attempt to portal the entity
		if ( PortalTeleport( ent, newLocation, newEntAxis, sourceAxis, destAxis, continuous ) ) {
            if (traceAlignment)
                gameLocal.Printf("PORTAL_ALIGNMENT %d render_bias %.6f landing_error %.6f\n", gameLocal.time,
                    renderBias.Length(), (ent->GetPhysics()->GetOrigin() - visibleLocation).Length());
			if ( alertMonsters && !gameLocal.isMultiplayer && ent->IsType( idPlayer::Type ) ) {
				gameLocal.SendMessageAI( this, GetOrigin(), 2000, MA_EnemyPortal );		
			}
			ent->Portalled( this ); // Inform the actor that it was just portalled
			return(true); // Portal succeeded
		}
	}

	return(false); // Portal failed
}

//==========================================================================
//
// hhPortal::PortalTeleport
//
//==========================================================================

bool hhPortal::PortalTeleport( idEntity *ent, const idVec3 &origin, const idMat3 &axis, const idMat3 &sourceAxis, const idMat3 &destAxis, bool continuous ) {
	idClipModel *clip = ent->GetPhysics()->GetClipModel();

	if ( !clip ) {
		return false;
	}

	// Properly set velocity relative to the new portal
	idVec3 vel = ent->GetPhysics()->GetLinearVelocity();
    const idVec3 originalVelocity = vel;
    const idMat3 originalActorAxis = ent->GetPhysics()->GetAxis();
    const idVec3 originalActorOrigin = ent->GetPhysics()->GetOrigin();
	PortalRotate( vel, sourceAxis, destAxis, true );
	ent->GetPhysics()->SetLinearVelocity( vel );	

	//rww - check if this new orientation is going to be in solid or not. if it is, try displacing the new origin based on the portal
	idVec3 useOrigin = origin;
	trace_t transCheck;
    const bool gunPortal = spawnArgs.GetBool("rw_portalGun");
    const bool blocked = gunPortal ? gameLocal.clip.Translation(transCheck, useOrigin, useOrigin, clip, axis, ent->GetPhysics()->GetClipMask(), ent) :
        gameLocal.clip.TranslationWithExceptions(transCheck, useOrigin, useOrigin, NULL, clip, axis, ent->GetPhysics()->GetClipMask(), ent);
    if (blocked && gunPortal) { ent->GetPhysics()->SetLinearVelocity(originalVelocity); gameLocal.Printf("PORTALGUN blocked exit\n"); if (cvarSystem->GetCVarBool("developer")) gameLocal.Printf("PORTAL_BLOCK entity=%d name=%s type=%d material=%s normal=%s\n", transCheck.c.entityNum, gameLocal.entities[transCheck.c.entityNum] ? gameLocal.entities[transCheck.c.entityNum]->GetName() : "none", transCheck.c.type, transCheck.c.material ? transCheck.c.material->GetName() : "none", transCheck.c.normal.ToString()); return false; }
	if (blocked) {
		if (cameraTarget) {
			bool safeSpot = true;
			const float distExtrusion = 2.0f;
			const float heightAdjust = (ent->GetPhysics()->GetBounds()[1].z-fabsf(ent->GetPhysics()->GetBounds()[0].z))/2.0f;
			idVec3 testOrigin = cameraTarget->GetOrigin();
			testOrigin += cameraTarget->GetAxis()[0]*distExtrusion;
			testOrigin -= cameraTarget->GetAxis()[2]*heightAdjust;
			if (gameLocal.clip.TranslationWithExceptions(transCheck, testOrigin, testOrigin, NULL, clip, axis, ent->GetPhysics()->GetClipMask(), ent)) {
				testOrigin += cameraTarget->GetAxis()[2]*(heightAdjust*2.0f); //then try going up further (could be upside-down or something)

				if (gameLocal.clip.TranslationWithExceptions(transCheck, testOrigin, testOrigin, NULL, clip, axis, ent->GetPhysics()->GetClipMask(), ent)) {
					//damn, this portal is really busted.
					safeSpot = false;
				}
			}
			if (safeSpot) {
				useOrigin = testOrigin; //got a safe spot
				if (ent->IsType(hhPlayer::Type)) {
					hhPlayer *plEnt = static_cast<hhPlayer *>(ent);
					if (plEnt->IsWallWalking()) { //if it's a wallwalking player, try to trace down on the other side for wallwalk
						idVec3 downPoint = useOrigin - (axis[2]*64.0f);
						if (gameLocal.clip.Translation(transCheck, useOrigin, downPoint, clip, axis, ent->GetPhysics()->GetClipMask(), ent)) {
							if (gameLocal.GetMatterType(transCheck, NULL) == SURFTYPE_WALLWALK) { //if we hit wallwalk, use the trace endpoint
								useOrigin = transCheck.endpos;
							}
						}
					}
				}
			}
			else {
				testOrigin = cameraTarget->GetOrigin();
				testOrigin += cameraTarget->GetAxis()[0]*distExtrusion;
				testOrigin -= cameraTarget->GetAxis()[2]*heightAdjust;

				useOrigin = testOrigin;

#if !GOLD
				if (developer.GetBool()) {
					const char *entName = ent->GetName();
					if (!entName || !entName[0]) {
						entName = "<unknown>";
					}
					gameLocal.Warning("Ent '%s' could not get a clean trace on the other side of gameportal at (%f %f %f).", entName, useOrigin.x, useOrigin.y, useOrigin.z);
					hhUtils::DebugAxis( testOrigin, cameraTarget->GetAxis(), 32.0f, 5000 );
				}
#endif
			}
		}
	}

    if (gunPortal) RW_AssistFloorExit(ent, cameraTarget, useOrigin, axis, vel);

	// Keep only continuous portal crossings in the presentation history. Exit
	// collision recovery is a real repositioning and must retain the snap.
	idMat3 presentationRotation = mat3_identity;
	for ( int i = 0; i < 3; ++i ) PortalRotate( presentationRotation[i], sourceAxis, destAxis, true );
	const bool preservePresentation = continuous && (useOrigin - origin).LengthSqr() < 0.01f &&
		gameLocal.TransformPortalPresentation(ent, GetOrigin(), cameraTarget->GetOrigin(), presentationRotation, destAxis[0]);
	if ( !preservePresentation ) gameLocal.InvalidateEntityPresentation(ent);

	// If the actor is a player, then disable camera interpolation for this move.  This must be done before linking
	if( ent->IsType(hhPlayer::Type) ) {
		hhPlayer* player = static_cast<hhPlayer*>( ent );


		idVec3 viewDir = player->GetAxis()[0];
		PortalRotate( viewDir, sourceAxis, destAxis, true );
        const renderView_t oldView = *player->GetRenderView();
		player->TeleportNoKillBox( useOrigin, axis, viewDir, player->GetUntransformedViewAngles(), preservePresentation );
        if (!preservePresentation && player->entityNumber == gameLocal.localClientNum) {
            // A blocked remainder can land exactly on the exit plane. Camera
            // basis rounding can put the eye microscopically behind it and
            // expose the back of the doorway for this tick. Resolve only that
            // numerical boundary, without moving physics or real offsets.
            renderView_t *exitView = player->GetRenderView();
            const float exitDistance = (exitView->vieworg - cameraTarget->GetOrigin()) * destAxis[0];
            const float eyePlaneEpsilon = 0.01f;
            if (exitDistance >= -eyePlaneEpsilon && exitDistance < eyePlaneEpsilon)
                exitView->vieworg += destAxis[0] * (eyePlaneEpsilon - exitDistance);
            gameLocal.SnapPortalViewModels(oldView);
        }
	} else {
		// Valid move.  This code is done in SetOrientation for the player.
		ent->SetOrigin( useOrigin );
		ent->SetAxis( axis );
        if (gunPortal && ent->IsType(idMoveable::Type)) {
            idVec3 angularVelocity = ent->GetPhysics()->GetAngularVelocity();
            PortalRotate(angularVelocity, sourceAxis, destAxis, true);
            ent->GetPhysics()->SetAngularVelocity(angularVelocity);
        }
	}

    if (gunPortal && ent->IsType(idAI::Type) && ent->GetPhysics()->IsType(idPhysics_Monster::Type)) {
        gameLocal.BeginNPCPortalPresentation(ent, cameraTarget, originalActorAxis * presentationRotation, axis, presentationRotation, originalActorOrigin);
    }

	// Re-link the actor into the clip tree
	clip->Link( gameLocal.clip );

	if ( ent->IsType( idActor::Type ) ) {
		static_cast<idActor *>(ent)->LinkCombat();
	}

	// players telefrag anything at the new position
	if ( ent->IsType( hhPlayer::Type ) && !gunPortal ) {
		gameLocal.KillBox( ent );
	}

	// Set the gravity correctly on the entity that was portaled, based upon the destination portal's gravity
	ent->CancelEvents( &EV_ResetGravity );

	if (!ent->IsType(hhVehicle::Type)) {
		ent->SetGravity( cameraTarget->GetGravity() );
	}

	// If the actor is a player, then check for wallwalk.  This should be done after linking and setting gravity
	if( ent->IsType(hhPlayer::Type) ) {
		hhPlayer* player = static_cast<hhPlayer*>(ent);
		if (player->GetPhysics() && player->GetPhysics()->IsType(hhPhysics_Player::Type)) {
			static_cast<hhPhysics_Player *>(player->GetPhysics())->CheckWallWalk( true );
		}
	}

    if (gunPortal && cvarSystem->GetCVarBool("com_fpsTrace"))
        gameLocal.Printf("PORTAL_ENTITY_EXIT %s name=%s\n", ent->GetClassname(), ent->GetName());
    if (ent->IsType(hhPlayer::Type) && cvarSystem->GetCVarBool("com_fpsTrace"))
        gameLocal.Printf("PORTAL_EXIT %d speed %.6f collision_adjustment %.6f\n", gameLocal.time,
            ent->GetPhysics()->GetLinearVelocity().Length(), (useOrigin-origin).Length());
	// Sound issues
	if ( bGlowPortal ) {
		StartSound( "snd_portal_entity", SND_CHANNEL_ANY ); // Play the portal sound at the origin portal
		if(this->cameraTarget) { // Play the portal sound at the destination portal as well
			const idSoundShader *def = declManager->FindSound(spawnArgs.GetString("snd_portal_entity"));//gameSoundWorld->FinishShader(spawnArgs.GetString("snd_portal_entity"));
			this->cameraTarget->StartSoundShader( def, SND_CHANNEL_ANY );
		}
	}

	return(true);
}

//==========================================================================
//
// hhPortal::Event_Trigger
//
// Toggle portal state.
//==========================================================================

void hhPortal::Event_Trigger(idEntity *activator) {

	// If we have a master portal, have him trigger everyone
	if ( masterPortal.IsValid() ) {
		masterPortal->Event_Trigger( activator );
		return;
	}
	
	// If we have slave portals, trigger them first
	if ( slavePortals.Num() > 0 ) {
		for ( int i = 0; i < slavePortals.Num(); ++i ) {
			slavePortals[ i ]->Trigger( activator );
		}
	}

	Trigger( activator );
}


void hhPortal::Trigger( idEntity *activator ) {

	hhFxInfo fxInfo;

	fxInfo.SetNormal( GetAxis()[2] );
	fxInfo.RemoveWhenDone( true );

	if(portalState == PORTAL_CLOSED) {
		portalState = PORTAL_OPENING;
		if (!bNoTeleport) { //rww - if teleporting, ensure things collide with me while i am opening
			GetPhysics()->SetContents( CONTENTS_SOLID );
		}
#if GAMEPORTAL_PVS
		if (areaPortal && !gameLocal.isClient) {
			gameLocal.SetPortalState( areaPortal, PS_BLOCK_NONE );
		}
#endif
		TriggerTargets();		// nla	
		renderEntity.shaderParms[SHADERPARM_TIMEOFFSET] = MS2SEC( gameLocal.time );

		if ( bGlowPortal ) {
			if (!gameLocal.isMultiplayer) { //rww - superfast portals don't use fx
				BroadcastFxInfo( spawnArgs.GetString("fx_open"), GetOrigin(), GetAxis(), &fxInfo );
			}

			StartSound( "snd_open", SND_CHANNEL_ANY );

			if ( spawnArgs.GetBool( "fast_open", "0" ) ) {
				SetSkinByName( spawnArgs.GetString( "skin" ) );
			} else {
				SetSkinByName( spawnArgs.GetString( "skin_onlyWarp" ) ); // Only show the warp when first opening
			}

			int anim = GetAnimator()->GetAnim(spawnArgs.GetString("open_anim", "open"));
			GetAnimator()->CycleAnim( ANIMCHANNEL_ALL, anim, gameLocal.time, 0 );
			PostEventMS( &EV_Opened, GetAnimator()->AnimLength( anim ) );
			SetShaderParm( SHADERPARM_MODE, 0 ); // ensure that sparking is off

			fl.neverDormant = true; // Don't allow the portal to go dormant while opening/closing

		} else {
			PostEventMS( &EV_Opened, 500 );		
		}

		PostEventMS( &EV_Show, 10 ); // Delay showing for a frame so the skin and registers are properly set beforehand
	} else if(portalState == PORTAL_OPENED || portalState == PORTAL_OPENING) {
		portalState = PORTAL_CLOSING;
		TriggerTargets();		// nla

		renderEntity.shaderParms[SHADERPARM_TIMEOFFSET] = -MS2SEC( gameLocal.time );

		if ( bGlowPortal ) {
			if (!gameLocal.isMultiplayer) { //rww - superfast portals don't use fx
				BroadcastFxInfo( spawnArgs.GetString("fx_close"), GetOrigin(), GetAxis(), &fxInfo );
			}
	
			StopSound( SND_CHANNEL_ANY );
			StartSound( "snd_close", SND_CHANNEL_ANY );

			int anim = GetAnimator()->GetAnim("close");
			GetAnimator()->PlayAnim( ANIMCHANNEL_ALL, anim, gameLocal.time, 0.5 );
			PostEventMS( &EV_Closed, GetAnimator()->AnimLength( anim ) );

			fl.neverDormant = true; // Don't allow the portal to go dormant while opening/closing

		} else {
			PostEventMS( &EV_Closed, 400 );
		}
	}
}

//==========================================================================
//
// hhPortal::Event_Opened
//
//==========================================================================

void hhPortal::Event_Opened( void ) {
	portalState = PORTAL_OPENED;
	if (!bNoTeleport) { //rww - if teleporting, ensure things collide with me while i am open
		GetPhysics()->SetContents( CONTENTS_SOLID );
	}
	TriggerTargets();	// nla

	if ( bGlowPortal ) {
		StartSound( "snd_loop", SND_CHANNEL_ANY );
		int anim = GetAnimator()->GetAnim("opened");
		GetAnimator()->CycleAnim( ANIMCHANNEL_ALL, anim, gameLocal.time, 0 );

		fl.neverDormant = false; // The portal can go dormant once opened/closed

		PostEventSec( &EV_PortalSpark, gameLocal.random.RandomFloat() );
	}	

	UpdateVisuals();
	
	// If we are open, and should close automatically, post an event to trigger us closed - nla
	if ( closeDelay > 0.0f ) {
		PostEventSec( &EV_Activate, closeDelay, 0 );

		// Remove the portal now that it has done it's job
		if (monsterportal) {
#ifdef _DEBUG
			// If another portal is camera targetting me it will cause a crash so check for it
			for( idEntity *ent = gameLocal.spawnedEntities.Next(); ent != NULL; ent = ent->spawnNode.Next() ) {
				if (ent->cameraTarget && ent->cameraTarget == this) {
					assert(0);
				}
			}

#endif
			PostEventSec( &EV_Remove, closeDelay+5.0f );
		}
	}
}

//==========================================================================
//
// hhPortal::Event_Closed
//
//==========================================================================

void hhPortal::Event_Closed( void ) {
	renderEntity.shaderParms[SHADERPARM_MODE] = 1.0f;
	portalState = PORTAL_CLOSED;
	GetPhysics()->SetContents( 0 ); //rww - do not collide with things while closed
#if GAMEPORTAL_PVS
	if (areaPortal && !gameLocal.isClient) {
		gameLocal.SetPortalState( areaPortal, PS_BLOCK_ALL );
	}
#endif
	TriggerTargets();	// nla

	fl.neverDormant = false; // The portal can go dormant once opened/closed
	
	Hide();
	UpdateVisuals();
	
	//? Have an option to remove the portals? - nla
	if ( spawnArgs.GetBool( "remove_on_close", "0" ) ) {
		PostEventMS( &EV_Remove, 0 );		
	}
}

//==========================================================================
//
// hhPortal::Event_PortalSpark
//
//==========================================================================

void hhPortal::Event_PortalSpark( void ) {
	int		spark;
	float	nextTime;

	if ( this->IsHidden() ) { // No sparking if hidden
		SetShaderParm( SHADERPARM_MODE, 0 ); // set the material parm (one-based)
		return;
	}

	spark = gameLocal.random.RandomInt( spawnArgs.GetInt( "sparkCount" ) );

	// Set the shader parm as needed
	SetShaderParm( SHADERPARM_MODE, spark + 1 ); // set the material parm (one-based)

	if ( gameLocal.random.RandomFloat() < 0.1f ) { // 1/10th of a chance that another spark will happen quickly after this one
		nextTime = 0.2f;
	} else { // Normal random time between sparks.
		nextTime = spawnArgs.GetFloat( "sparkTimeMin" ) + gameLocal.random.RandomFloat() * spawnArgs.GetFloat( "sparkTimeRnd" );
	}

	StartSound( "snd_portal_spark", SND_CHANNEL_ANY );
	
	PostEventSec( &EV_PortalSpark, nextTime );
	PostEventSec( &EV_PortalSparkEnd, 0.1f ); // spark only lasts for 0.1 sec
	
}

//==========================================================================
//
// hhPortal::Event_PortalSparkEnd
//
//==========================================================================

void hhPortal::Event_PortalSparkEnd( void ) {
	SetShaderParm( SHADERPARM_MODE, 0 ); // Disable the spark parm
}

//==========================================================================
//
// hhPortal::Event_ShowGlowPortal
//
// Simply sets the skin on the portal back to the default or to the
// specified "skin"
//==========================================================================

void hhPortal::Event_ShowGlowPortal( void ) {
	SetSkinByName( spawnArgs.GetString( "skin" ) );
}

//==========================================================================
//
// hhPortal::Event_HideGlowPortal
//
// Sets the skin on the portal to a specific skin that hides the glowy parts
//==========================================================================

void hhPortal::Event_HideGlowPortal( void ) {
	SetSkinByName( spawnArgs.GetString( "skin_onlyWarp" ) );
}

#include "portalgun.inl"

// Validate every limb before changing any physics state. A blocked exit must
// never transfer only the torso or abandon the rest of the constrained figure.
static bool RW_TransferPortalRagdoll(hhPortal *portal, idEntity *ent, const idVec3 *crossingPoint) {
    hhPortal *destination=static_cast<hhPortal *>(portal->cameraTarget);
    if (!destination) return false;
    idPhysics_AF *af=static_cast<idPhysics_AF *>(ent->GetPhysics());
    idMat3 rotation=mat3_identity;
    for (int i=0;i<3;++i) PortalRotate(rotation[i],portal->GetAxis().Transpose(),destination->GetAxis(),true);
    for (int i=0;i<af->GetNumClipModels();++i) {
        const idVec3 target=(af->GetOrigin(i)-portal->GetOrigin())*rotation+destination->GetOrigin();
        // Sweep the remaining tick at the exit as well as checking occupancy,
        // so a fast corpse cannot skip a thin obstacle beyond the opening.
        const idVec3 start=crossingPoint ? target+(*crossingPoint-af->GetOrigin())*rotation : target;
        trace_t trace;
        if (gameLocal.clip.Translation(trace,target,target,af->GetClipModel(i),af->GetAxis(i)*rotation,
                af->GetBody(i)->GetClipMask(),ent) ||
            gameLocal.clip.Translation(trace,start,target,af->GetClipModel(i),af->GetAxis(i)*rotation,
                af->GetBody(i)->GetClipMask(),ent)) {
            if (cvarSystem->GetCVarBool("developer")) gameLocal.Printf("PORTAL_AF_BLOCK %s body=%d hit=%d\n",ent->GetName(),i,trace.c.entityNum);
            return false;
        }
    }
    ent->CancelEvents(&EV_ResetGravity);
    af->SetGravity(destination->GetGravity());
    af->TransformThroughPortal(portal->GetOrigin(),destination->GetOrigin(),rotation);
    gameLocal.InvalidateEntityPresentation(ent);
    static_cast<idAFEntity_Base *>(ent)->UpdateAnimationControllers();
    ent->UpdateVisuals();
    static_cast<idAFEntity_Base *>(ent)->LinkCombat();
    ent->Portalled(portal);
    if (cvarSystem->GetCVarBool("developer")) gameLocal.Printf("PORTAL_AF_CROSS %s bodies=%d origin=%s\n",ent->GetName(),af->GetNumClipModels(),af->GetOrigin().ToString());
    return true;
}

