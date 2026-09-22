//*****************************************************************************
//**
//** GAME_SUNCORONA.CPP
//**
//** Game code for Sun Coronas
//**
//** TODO:
//**	- Implement corona scale (or just leave this for the shader?)
//**	- Check if 4096 is good enough for distance -- issues in very large rooms?
//**	- Verify if the noFragment check is valid for checking for skyboxes
//**	- Make it function corrected with the rifle zoom view
//*****************************************************************************

// HEADER FILES ---------------------------------------------------------------

#include "precompiled.h"
#pragma hdrstop

#include "prey_local.h"

// MACROS ---------------------------------------------------------------------

// TYPES ----------------------------------------------------------------------

// CLASS DECLARATIONS ---------------------------------------------------------

CLASS_DECLARATION( idEntity, hhSunCorona )
END_CLASS

// STATE DECLARATIONS ---------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES -----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ------------------------------------------------

// EXTERNAL DATA DECLARATIONS -------------------------------------------------

// PUBLIC DATA DEFINITIONS ----------------------------------------------------

// PRIVATE DATA DEFINITIONS ---------------------------------------------------

// CODE -----------------------------------------------------------------------

//=============================================================================
//
// hhSunCorona::Spawn
//
//=============================================================================

void hhSunCorona::Spawn(void) {
	corona = declManager->FindMaterial( spawnArgs.GetString( "mtr_corona" ) );
	scale = spawnArgs.GetFloat( "scale", "1" );
	sunVector = spawnArgs.GetVector( "sunVector", "0 0 -1" );
	sunVector.Normalize();
	sunDistance = spawnArgs.GetFloat( "sunDistance", "4096" );

	GetPhysics()->SetContents(0);
	Hide();
	BecomeInactive(TH_THINK);

	gameLocal.SetSunCorona( this );
}

void hhSunCorona::Save(idSaveGame *savefile) const {
	savefile->WriteMaterial( corona );
	savefile->WriteFloat( scale );
	savefile->WriteVec3( sunVector );
	savefile->WriteFloat( sunDistance );
}

void hhSunCorona::Restore( idRestoreGame *savefile ) {
	savefile->ReadMaterial( corona );
	savefile->ReadFloat( scale );
	savefile->ReadVec3( sunVector );
	savefile->ReadFloat( sunDistance );
}

//=============================================================================
//
// hhSunCorona::~hhSunCorona
//
//=============================================================================

hhSunCorona::~hhSunCorona() {
	corona = NULL;
}

//=============================================================================
//
// hhhSunCorona::Draw
//
//=============================================================================

void hhSunCorona::Draw( hhPlayer *player ) {
	// Retail PC's hhSunCorona::Draw is a no-op. The sky materials already
	// provide the sun; this abandoned fullscreen overlay adds a second glare.
	// Keep the entity and saved fields compatible with existing maps/saves.
}
