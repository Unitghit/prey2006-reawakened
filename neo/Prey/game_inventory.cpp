
#include "precompiled.h"
#pragma hdrstop

#include "prey_local.h"

void hhInventory::Clear() {
	memset( &requirements, 0, sizeof( requirements ) );
	maxSpirit = 0;
	bHasDeathwalked = false;
	storedHealth = 0;
	energyType = "energy_plasma";
	memset( altMode, 0, sizeof( altMode ) );
	memset( weaponRaised, 0, sizeof( weaponRaised ) );
	memset( lastShot, 0, sizeof( lastShot ) );		//HUMANHEAD bjk PATCH 7-27-06
	zoomFov = 0;

	idInventory::Clear();
}

void hhInventory::Save(idSaveGame *savefile) const {
	idInventory::Save( savefile );

	savefile->WriteBool(bHasDeathwalked);
	savefile->Write(&maxSpirit, sizeof(maxSpirit));
	savefile->Write(&requirements, sizeof(requirements));
	savefile->WriteInt( storedHealth );
	savefile->WriteString( energyType.c_str() );
	savefile->Write(&altMode, sizeof(altMode));
	savefile->Write(&weaponRaised, sizeof(weaponRaised));
	savefile->WriteInt( zoomFov );
}

void hhInventory::Restore( idRestoreGame *savefile ) {
	idInventory::Restore( savefile );

	savefile->ReadBool(bHasDeathwalked);
	savefile->Read(&maxSpirit, sizeof(maxSpirit));
	savefile->Read(&requirements, sizeof(requirements));
	savefile->ReadInt( storedHealth );
	savefile->ReadString( energyType );
	savefile->Read(&altMode, sizeof(altMode));
	savefile->Read(&weaponRaised, sizeof(weaponRaised));
	memset( lastShot, 0, sizeof( lastShot ) );		//HUMANHEAD bjk PATCH 7-27-06
	savefile->ReadInt( zoomFov );
}

/*
==============
hhInventory::GetPersistantData
	PDMMERGE PERSISTENTMERGE: Overridden, Done for 6-03-05 merge
==============
*/
void hhInventory::GetPersistantData( idDict &dict ) {
	int		i;
	int		num;
	idDict	*item;
	idStr	key;
	const idKeyValue *kv;
	const char *name;

    // don't bother with powerups or the clip

	// maxhealth, maxspirit
	dict.SetInt( "maxhealth", maxHealth);
	dict.SetInt( "max_ammo_spiritpower", maxSpirit);
	dict.SetBool( "bHasDeathwalked", bHasDeathwalked );
	dict.SetInt( "storedHealth", storedHealth );
	dict.Set( "energyType", energyType.c_str() );	//HUMANHEAD bjk
	dict.SetInt( "zoomFov", zoomFov );	//HUMANHEAD bjk

	// ammo
	for( i = 0; i < AMMO_NUMTYPES; i++ ) {
		name = idWeapon::GetAmmoNameForNum( ( ammo_t )i );
		if ( name ) {
			dict.SetInt( name, ammo[ i ] );
		}
	}

	//HUMANHEAD bjk: weapons
	for( i = 0; i < MAX_WEAPONS; i++ ) {
		sprintf( key, "altMode_%i", i );
		dict.SetBool( key, altMode[i] );
		sprintf( key, "weaponRaised_%i", i );
		dict.SetBool( key, weaponRaised[i] );
	}
	//HUMANHEAD END

	// items
	num = 0;
	for( i = 0; i < items.Num(); i++ ) {
		item = items[ i ];

		// copy all keys with "inv_"
		kv = item->MatchPrefix( "inv_" );
		if ( kv ) {
			while( kv ) {
				sprintf( key, "item_%i %s", num, kv->GetKey().c_str() );
				dict.Set( key, kv->GetValue() );
				kv = item->MatchPrefix( "inv_", kv );
			}

			// HUMANHEAD CJR: copy all keys with "def_"
			// Needed for Hunter Hand GUI
			kv = item->MatchPrefix( "def_" );
			if ( kv ) {
				while( kv ) {
					sprintf( key, "item_%i %s", num, kv->GetKey().c_str() );
					dict.Set( key, kv->GetValue() );
					kv = item->MatchPrefix( "def_", kv );
				}
			} // HUMANHEAD END	

			// HUMANHEAD CJR: copy all keys with "passtogui_"
			// Needed for Hunter Hand GUI
			kv = item->MatchPrefix( "passtogui_" );
			if ( kv ) {
				while( kv ) {
					sprintf( key, "item_%i %s", num, kv->GetKey().c_str() );
					dict.Set( key, kv->GetValue() );
					kv = item->MatchPrefix( "passtogui_", kv );
				}
			} // HUMANHEAD END	

			num++;
		}
	}
	dict.SetInt( "items", num );

	// weapons
	dict.SetInt( "weapon_bits", weapons );
	
	dict.SetInt( "levelTriggers", levelTriggers.Num() );
	for ( i = 0; i < levelTriggers.Num(); i++ ) {
		sprintf( key, "levelTrigger_Level_%i", i );
		dict.Set( key, levelTriggers[i].levelName );
		sprintf( key, "levelTrigger_Trigger_%i", i );
		dict.Set( key, levelTriggers[i].triggerName );
	}	
}


/*
==============
hhInventory::RestoreInventory
	PDMMERGE PERSISTENTMERGE: Overridden, Done for 6-03-05 merge
==============
*/
void hhInventory::RestoreInventory( idPlayer *owner, const idDict &dict ) {
	int			i;
	int			num;
	idDict		*item;
	idStr		key;
	idStr		itemname;
	const idKeyValue *kv;
	const char	*name;

	Clear();

	// health
	maxHealth		= dict.GetInt( "maxhealth", "100" );
	bHasDeathwalked = dict.GetBool( "bHasDeathwalked" );
	storedHealth	= dict.GetInt( "storedHealth", "0" );
	energyType		= dict.GetString( "energyType", "energy_plasma" );
	zoomFov			= dict.GetInt( "zoomFov" );

	// the clip and powerups aren't restored

	// max spirit
	maxSpirit		= dict.GetInt( "max_ammo_spiritpower" );

	// ammo
	for( i = 0; i < AMMO_NUMTYPES; i++ ) {
		name = idWeapon::GetAmmoNameForNum( ( ammo_t )i );
		if ( name ) {
			ammo[ i ] = dict.GetInt( name );
		}
	}

	//HUMANHEAD bjk: weapons
	for( i = 0; i < MAX_WEAPONS; i++ ) {
		sprintf( key, "altMode_%i", i );
		altMode[i] = dict.GetBool( key );
		sprintf( key, "weaponRaised_%i", i );
		weaponRaised[i] = dict.GetBool( key );
	}
	//HUMANHEAD END

	// items
	num = dict.GetInt( "items" );
	items.SetNum( num );
	for( i = 0; i < num; i++ ) {
		item = new idDict();
		items[ i ] = item;
		sprintf( itemname, "item_%i ", i );
		kv = dict.MatchPrefix( itemname );
		while( kv ) {
			key = kv->GetKey();
			key.Strip( itemname );
			item->Set( key, kv->GetValue() );
			kv = dict.MatchPrefix( itemname, kv );
		}
	}

	//HUMANHEAD aob: in addition to the persistent items, give hardcoded items from players.def
	Give( owner, dict, "item", dict.GetString( "item" ), NULL, true );
	//HUMANHEAD END

	// weapons are stored as a number for persistant data, but as strings in the entityDef
	weapons	= dict.GetInt( "weapon_bits", "0" );
	Give( owner, dict, "weapon", dict.GetString( "weapon" ), NULL, false );

	num = dict.GetInt( "levelTriggers" );
	for ( i = 0; i < num; i++ ) {
		sprintf( itemname, "levelTrigger_Level_%i", i );
		idLevelTriggerInfo lti;
		lti.levelName = dict.GetString( itemname );
		sprintf( itemname, "levelTrigger_Trigger_%i", i );
		lti.triggerName = dict.GetString( itemname );
		levelTriggers.Append( lti );
	}

	// Keep weapon switch HUD element from showing up at level load
	weaponPulse = false;
}

// Fractional pickup credit lives in the already-serialized player dictionary.
// Integer ammo totals include loaded rounds, as in the original engine.
static double WeaponAmmoFraction(const hhPlayer *player, int index) {
	return atof(player->spawnArgs.GetString(va("rw_weapon_ammo_fraction_%d", index), "0"));
}
static void StoreWeaponAmmo(hhPlayer *player, int index, double total) {
	const int whole = (int)floor(total + 1e-9);
	player->inventory.ammo[index] = whole;
	const double fraction = Max(0.0, total - whole);
	player->spawnArgs.Set(va("rw_weapon_ammo_fraction_%d", index), va("%.17g", fraction));
}

// Only installed, owned additions participate; older shotgun-only installs retain two pools.
static int RifleGroupAmmoCount(const idPlayer *owner) {
    const idDict *mg = gameLocal.FindEntityDefDict("weaponobj_d3machinegun", false);
    return mg && mg->GetBool("rw_saveCompatible") && owner->spawnArgs.GetBool("rw_weapon_d3machinegun_owned") ? 3 : 2;
}

bool hhInventory::UsesIndependentWeaponAmmo(const idPlayer *owner) const {
	if (!owner || gameLocal.isMultiplayer || *cvarSystem->GetCVarString("fs_game")) { return false; }
	const idDict *addon = gameLocal.FindEntityDefDict("weaponobj_d3shotgun", false);
	return addon && addon->GetBool("rw_saveCompatible") && addon->GetBool("rw_splitAmmo");
}
bool hhInventory::SplitRifleAmmo(const idPlayer *owner) const {
	return UsesIndependentWeaponAmmo(owner) && cvarSystem->GetCVarBool("g_doom3Shotgun") &&
		owner->spawnArgs.GetBool("rw_weapon_ammo_initialized") &&
		owner->spawnArgs.GetBool("rw_weapon_d3shotgun_owned");
}
bool hhInventory::SplitAutocannonAmmo(const idPlayer *owner) const {
	if (!UsesIndependentWeaponAmmo(owner) || !cvarSystem->GetCVarBool("g_doom3Shotgun") ||
		!owner->spawnArgs.GetBool("rw_weapon_d3chaingun_owned")) { return false; }
	const idDict *addon = gameLocal.FindEntityDefDict("weaponobj_d3chaingun", false);
	return addon && addon->GetBool("rw_saveCompatible");
}
bool hhInventory::SplitAcidAmmo(const idPlayer *owner) const {
	if (!UsesIndependentWeaponAmmo(owner) || !cvarSystem->GetCVarBool("g_doom3Shotgun") ||
		!owner->spawnArgs.GetBool("rw_weapon_d3plasmagun_owned")) { return false; }
	const idDict *addon = gameLocal.FindEntityDefDict("weaponobj_d3plasmagun", false);
	return addon && addon->GetBool("rw_saveCompatible");
}
bool hhInventory::SplitRocketAmmo(const idPlayer *owner) const {
	if (!UsesIndependentWeaponAmmo(owner) || !cvarSystem->GetCVarBool("g_doom3Shotgun") ||
		!owner->spawnArgs.GetBool("rw_weapon_d3rocketlauncher_owned")) { return false; }
	const idDict *addon = gameLocal.FindEntityDefDict("weaponobj_d3rocketlauncher", false);
	return addon && addon->GetBool("rw_saveCompatible");
}
bool hhInventory::SynchronizeWeaponAmmo(hhPlayer *owner) {
	if (!UsesIndependentWeaponAmmo(owner)) { return false; }
	bool changed = false;
	if (cvarSystem->GetCVarBool("g_doom3Shotgun") && owner->spawnArgs.GetBool("rw_weapon_d3shotgun_owned") &&
		!owner->spawnArgs.GetBool("rw_weapon_ammo_initialized")) {
		const int rifle = AmmoIndexForAmmoClass("ammo_rifle");
		const int shells = AmmoIndexForAmmoClass("ammo_d3shells");
		const int rifleFull = idInventory::MaxAmmoForAmmoClass(owner, "ammo_rifle");
		const int count = RifleGroupAmmoCount(owner);
		const int shellFull = 16 * count;
		if (ammo[rifle] < 0) { ammo[shells] = -1; if (count == 3) { ammo[11] = -1; } }
		else if (rifleFull > 0 && shellFull > 0) {
			const double total = ammo[rifle] + WeaponAmmoFraction(owner, rifle);
			StoreWeaponAmmo(owner, rifle, total / count);
			StoreWeaponAmmo(owner, shells, total / count * shellFull / rifleFull);
			if (count == 3) { StoreWeaponAmmo(owner, 11, total * 180 / rifleFull); }
		}
		owner->spawnArgs.SetBool("rw_weapon_ammo_initialized", true);
		// Older prototype magazines referenced the same total. They are not
		// extra ammunition and must fit in the newly independent totals.
		if (ammo[rifle] >= 0 && clip[2] > ammo[rifle]) { clip[2] = ammo[rifle]; }
		if (ammo[shells] >= 0 && clip[8] > ammo[shells]) { clip[8] = ammo[shells]; }
		changed = true;
	}
	const bool active = SplitRifleAmmo(owner);
	if (active) {
		const int shells = AmmoIndexForAmmoClass("ammo_d3shells");
		const int capacity = MaxAmmoForAmmoClass(owner, "ammo_d3shells");
		const int count = RifleGroupAmmoCount(owner);
		if (owner->spawnArgs.GetInt("rw_weapon_machinegun_capacity", "-1") != 180) {
			owner->spawnArgs.SetInt("rw_weapon_machinegun_capacity", 180);
			changed = true;
		}
		if (ammo[11] >= 0 && ammo[11] + WeaponAmmoFraction(owner, 11) > 180) {
			StoreWeaponAmmo(owner, 11, 180);
			changed = true;
		}
		if (owner->spawnArgs.GetInt("rw_weapon_ammo_group_count") != count) {
			// Upgrading an already split inventory preserves all acquired ammunition.
			// The new gun receives subsequent pickup supply, without stealing shells.
			owner->spawnArgs.SetInt("rw_weapon_ammo_group_count", count);
			changed = true;
		}
		if (owner->spawnArgs.GetInt("rw_weapon_shell_capacity", "-1") != capacity) {
			owner->spawnArgs.SetInt("rw_weapon_shell_capacity", capacity);
			changed = true; // Refresh ammo bars restored from an older balance.
		}
		if (ammo[shells] >= 0 && ammo[shells] + WeaponAmmoFraction(owner, shells) > capacity) {
			StoreWeaponAmmo(owner, shells, capacity);
			changed = true;
		}
	}

	if (owner->spawnArgs.GetInt("rw_weapon_ammo_split_active", "-1") != int(active)) {
		owner->spawnArgs.SetBool("rw_weapon_ammo_split_active", active);
		changed = true;
	}
	const bool autocannonActive = SplitAutocannonAmmo(owner);
	if (owner->spawnArgs.GetInt("rw_weapon_autocannon_split_active", "-1") != int(autocannonActive)) {
		owner->spawnArgs.SetBool("rw_weapon_autocannon_split_active", autocannonActive);
		changed = true;
	}
	// Existing acquired rounds remain in their own reserve. Newly unlocked
	// Chainguns receive supply from subsequent primary Autocannon pickups.
	if (autocannonActive && ammo[12] >= 0 && ammo[12] + WeaponAmmoFraction(owner, 12) > 300) {
		StoreWeaponAmmo(owner, 12, 300);
		changed = true;
	}
	const bool acidActive = SplitAcidAmmo(owner);
	if (owner->spawnArgs.GetInt("rw_weapon_acid_split_active", "-1") != int(acidActive)) {
		owner->spawnArgs.SetBool("rw_weapon_acid_split_active", acidActive);
		changed = true;
	}
	if (acidActive && ammo[13] >= 0 && ammo[13] + WeaponAmmoFraction(owner, 13) > 150) {
		StoreWeaponAmmo(owner, 13, 150);
		changed = true;
	}
	const bool rocketActive = SplitRocketAmmo(owner);
	if (owner->spawnArgs.GetInt("rw_weapon_rocket_split_active", "-1") != int(rocketActive)) {
		owner->spawnArgs.SetBool("rw_weapon_rocket_split_active", rocketActive);
		changed = true;
	}
	if (rocketActive && ammo[14] >= 0 && ammo[14] + WeaponAmmoFraction(owner, 14) > 12) {
		StoreWeaponAmmo(owner, 14, 12);
		changed = true;
	}
	return changed;
}

bool hhInventory::GiveRifleGroupAmmo(hhPlayer *owner, int amount) {
	if (amount <= 0) { return false; }
	const int count = RifleGroupAmmoCount(owner);
	const int indices[] = { AmmoIndexForAmmoClass("ammo_rifle"), AmmoIndexForAmmoClass("ammo_d3shells"), 11 };
	const double full[] = { double(idInventory::MaxAmmoForAmmoClass(owner, "ammo_rifle")), 30.0, 360.0 };
	if (full[0] <= 0) { return false; }
	bool accepted = false;
	// Pickup supply is fixed independently of capacity and fullness. Keep the
	// original three-way rates even before the Machine Gun has been unlocked.
	for (int i = 0; i < count; ++i) {
		if (ammo[indices[i]] < 0) { continue; }
		const double total = ammo[indices[i]] + WeaponAmmoFraction(owner, indices[i]);
		const double room = Max(0.0, double(MaxAmmoForAmmoClass(owner, idWeapon::GetAmmoNameForNum((ammo_t)indices[i]))) - total);
		const double grant = Min(room, amount / full[0] * full[i] / 3.0);
		if (grant > 1e-12) { StoreWeaponAmmo(owner, indices[i], total + grant); accepted = true; }
	}
	if (!accepted) { return false; }
	ammoPulse = true;
	return true;
}

bool hhInventory::GiveAutocannonGroupAmmo(hhPlayer *owner, int amount) {
	if (amount <= 0) { return false; }
	const int count = 2;
	const int indices[] = { AmmoIndexForAmmoClass("ammo_autocannon"), 12 };
	const double full[] = { double(idInventory::MaxAmmoForAmmoClass(owner, "ammo_autocannon")), 600.0 };
	if (full[0] <= 0) { return false; }
	bool accepted = false;
	// A full partner never donates its share: all slot-5 pickups give half
	// the original supply to each independent reserve that has room.
	for (int i = 0; i < count; ++i) {
		if (ammo[indices[i]] < 0) { continue; }
		const double total = ammo[indices[i]] + WeaponAmmoFraction(owner, indices[i]);
		const double room = Max(0.0, double(MaxAmmoForAmmoClass(owner, idWeapon::GetAmmoNameForNum((ammo_t)indices[i]))) - total);
		const double grant = Min(room, amount / full[0] * full[i] / 2.0);
		if (grant > 1e-12) { StoreWeaponAmmo(owner, indices[i], total + grant); accepted = true; }
	}
	if (!accepted) { return false; }
	ammoPulse = true;
	return true;
}

bool hhInventory::GiveAcidGroupAmmo(hhPlayer *owner, int amount) {
	if (amount <= 0) { return false; }
	const int count = 2;
	const int indices[] = { AmmoIndexForAmmoClass("ammo_acid"), 13 };
	const double full[] = { double(idInventory::MaxAmmoForAmmoClass(owner, "ammo_acid")), 500.0 };
	if (full[0] <= 0) { return false; }
	bool accepted = false;
	// A full partner never donates its share: all slot-6 pickups give half
	// the original supply to each independent reserve that has room.
	for (int i = 0; i < count; ++i) {
		if (ammo[indices[i]] < 0) { continue; }
		const double total = ammo[indices[i]] + WeaponAmmoFraction(owner, indices[i]);
		const double room = Max(0.0, double(MaxAmmoForAmmoClass(owner, idWeapon::GetAmmoNameForNum((ammo_t)indices[i]))) - total);
		const double grant = Min(room, amount / full[0] * full[i] / 2.0);
		if (grant > 1e-12) { StoreWeaponAmmo(owner, indices[i], total + grant); accepted = true; }
	}
	if (!accepted) { return false; }
	ammoPulse = true;
	return true;
}

bool hhInventory::GiveRocketGroupAmmo(hhPlayer *owner, int amount) {
	if (amount <= 0) { return false; }
	const int count = 2;
	const int indices[] = { AmmoIndexForAmmoClass("ammo_crawler_red"), 14 };
	const double full[] = { double(idInventory::MaxAmmoForAmmoClass(owner, "ammo_crawler_red")), 25.0 };
	if (full[0] <= 0) { return false; }
	bool accepted = false;
	// A full partner never donates its share: all slot-7 pickups give half
	// the original supply to each independent reserve that has room.
	for (int i = 0; i < count; ++i) {
		if (ammo[indices[i]] < 0) { continue; }
		const double total = ammo[indices[i]] + WeaponAmmoFraction(owner, indices[i]);
		const double room = Max(0.0, double(MaxAmmoForAmmoClass(owner, idWeapon::GetAmmoNameForNum((ammo_t)indices[i]))) - total);
		const double grant = Min(room, amount / full[0] * full[i] / 2.0);
		if (grant > 1e-12) { StoreWeaponAmmo(owner, indices[i], total + grant); accepted = true; }
	}
	if (!accepted) { return false; }
	ammoPulse = true;
	return true;
}

int hhInventory::MaxAmmoForAmmoClass( idPlayer *owner, const char *ammo_classname ) const {
	int max = 0;
	if (ammo_classname && UsesIndependentWeaponAmmo(owner)) {
		if (!idStr::Icmp(ammo_classname, "ammo_d3shells")) {
			return 16; // Stable total cap, including the eight loaded shells.
		}
		if (!idStr::Icmp(ammo_classname, "ammo_d3bullets")) { return 180; }
		if (!idStr::Icmp(ammo_classname, "ammo_d3belt")) { return 300; }
		if (!idStr::Icmp(ammo_classname, "ammo_d3cells")) { return 150; }
		if (!idStr::Icmp(ammo_classname, "ammo_d3rockets")) { return 12; }
		if (!idStr::Icmp(ammo_classname, "ammo_acid") && SplitAcidAmmo(owner)) {
			return idInventory::MaxAmmoForAmmoClass(owner, ammo_classname) / 2;
		}
		if (!idStr::Icmp(ammo_classname, "ammo_crawler_red") && SplitRocketAmmo(owner)) {
			return idInventory::MaxAmmoForAmmoClass(owner, ammo_classname) / 2;
		}
		if (!idStr::Icmp(ammo_classname, "ammo_autocannon") && SplitAutocannonAmmo(owner)) {
			return idInventory::MaxAmmoForAmmoClass(owner, ammo_classname) / 2;
		}
		if (!idStr::Icmp(ammo_classname, "ammo_rifle") && SplitRifleAmmo(owner)) {
			return idInventory::MaxAmmoForAmmoClass(owner, ammo_classname) / RifleGroupAmmoCount(owner);
		}
	}
	if (ammo_classname != NULL) {
		if (!idStr::Icmp(ammo_classname, "ammo_spiritpower")) {
			max = maxSpirit;
		}
		else {
			max = idInventory::MaxAmmoForAmmoClass(owner, ammo_classname);
		}
	}
	return max;
}

//	PDMMERGE PERSISTENTMERGE: Overridden, Done for 6-03-05 merge
void hhInventory::AddPickupName( const char *name, const char *icon, bool bIsWeapon) {
	if ( idStr::Length(icon) > 0 ) {
		idItemInfo &info = pickupItemNames.Alloc();

		if ( !idStr::Icmpn( name, STRTABLE_ID, strlen( STRTABLE_ID ) ) ) {
			info.name = common->GetLanguageDict()->GetString( name );
		} else {
			info.name = name;
		}
		info.icon = icon;
		info.time = 0;
		info.slotZeroTime = 0;
		info.matcolorAlpha = 0.0f;
		info.bDoubleWide = bIsWeapon;
	}
}

/*
==============
hhInventory::Give
	PDMMERGE PERSISTENTMERGE: Overridden, Done for 6-03-05 merge
==============
*/
bool hhInventory::Give( idPlayer *owner, const idDict &spawnArgs, const char *statname, const char *value, int *idealWeapon, bool updateHud ) {
	int						i;
	const char				*pos;
	const char				*end;
	int						len;
	idStr					weaponString;
	int						max;
	const idDeclEntityDef	*weaponDecl;
	bool					tookWeapon;
	int						amount;
	idItemInfo				info;
	hhPlayer*				playerOwner = NULL;


	if( owner && owner->IsType( hhPlayer::Type ) ) {
		playerOwner = static_cast<hhPlayer*>(owner);
	}

	if ( !idStr::Icmp( statname, "health" ) || !idStr::Icmp( statname, "healthspecial" ) ) { //healthspecial is for mp and indicates that this item can push a player's health up to the "real" maxhealth
		int localMaxHealth = maxHealth;
		if (gameLocal.isMultiplayer) { //rww - only pipes can put us above 100 in mp
			if (idStr::Icmp( statname, "healthspecial" )) {
				localMaxHealth = MAX_HEALTH_NORMAL_MP;
			}
		}
		if ( playerOwner->health >= localMaxHealth ) {
			return false;
		}
		int oldHealth = playerOwner->health;
		playerOwner->health += atoi( value );
		if ( playerOwner->health > localMaxHealth ) {
			playerOwner->health = localMaxHealth;
		}
		if (playerOwner) {
			playerOwner->healthPulse = true;
		}
	} else if ( !idStr::Icmpn( statname, "ammo_", 5 ) ) {
		if (playerOwner && !idStr::Icmp(statname, "ammo_rifle") && UsesIndependentWeaponAmmo(owner)) {
			playerOwner->SynchronizeDoom3Shotgun();
			if (SplitRifleAmmo(owner)) { return GiveRifleGroupAmmo(playerOwner, atoi(value)); }
		}
		if (playerOwner && !idStr::Icmp(statname, "ammo_autocannon") && UsesIndependentWeaponAmmo(owner)) {
			playerOwner->SynchronizeDoom3Shotgun();
			if (SplitAutocannonAmmo(owner)) { return GiveAutocannonGroupAmmo(playerOwner, atoi(value)); }
		}
		if (playerOwner && !idStr::Icmp(statname, "ammo_acid") && UsesIndependentWeaponAmmo(owner)) {
			playerOwner->SynchronizeDoom3Shotgun();
			if (SplitAcidAmmo(owner)) { return GiveAcidGroupAmmo(playerOwner, atoi(value)); }
		}
		if (playerOwner && !idStr::Icmp(statname, "ammo_crawler_red") && UsesIndependentWeaponAmmo(owner)) {
			playerOwner->SynchronizeDoom3Shotgun();
			if (SplitRocketAmmo(owner)) { return GiveRocketGroupAmmo(playerOwner, atoi(value)); }
		}
		i = AmmoIndexForAmmoClass( statname );
		max = MaxAmmoForAmmoClass( owner, statname );
		if ( ammo[ i ] >= max ) {
			return false;
		}
		amount = atoi( value );
		if ( amount ) {			
			ammo[ i ] += amount;
			if ( ( max > 0 ) && ( ammo[ i ] > max ) ) {
				ammo[ i ] = max;
			}
			ammoPulse = true;
		}
		if (playerOwner && !idStr::Icmp(statname, "ammo_spiritpower")) {
			playerOwner->spiritPulse = true;
		}
	} else if ( !idStr::Icmp( statname, "item" ) ) {
		pos = value;
		while( pos != NULL ) {
			end = strchr( pos, ',' );
			if ( end ) {
				len = end - pos;
				end++;
			} else {
				len = strlen( pos );
			}
		
			idStr itemName( pos, 0, len );
			
			GiveItem( spawnArgs, gameLocal.FindEntityDefDict(itemName.c_str(), false) );
		
			pos = end;
		}
	} else if ( !idStr::Icmp( statname, "weapon" ) ) {
		tookWeapon = false;
		for( pos = value; pos != NULL; pos = end ) {
			end = strchr( pos, ',' );
			if ( end ) {
				len = end - pos;
				end++;
			} else {
				len = strlen( pos );
			}

			idStr weaponName( pos, 0, len );

			// find the number of the matching weapon name
			for( i = 1; i < MAX_WEAPONS; i++ ) {
				if ( weaponName == playerOwner->GetWeaponName(i) ) {
					break;
				}
			}

			if ( i >= MAX_WEAPONS ) {
				gameLocal.Error( "Unknown weapon '%s'", weaponName.c_str() );
			}

			// cache the media for this weapon
			weaponDecl = gameLocal.FindEntityDef( weaponName, false );

			// don't pickup "no ammo" weapon types twice
			// not for D3 SP .. there is only one case in the game where you can get a no ammo
			// weapon when you might already have it, in that case it is more conistent to pick it up
			if ( gameLocal.isMultiplayer && weaponDecl && ( weapons & ( 1 << i ) ) && !weaponDecl->dict.GetInt( "ammoRequired" ) ) {
				continue;
			}

			if ( !gameLocal.world->spawnArgs.GetBool( "no_Weapons" ) || ( weaponName == "weaponobj_fists" ) ) {
				playerOwner->UnlockWeapon( i ); //TODO add key for disabling this
				if ( ( weapons & ( 1 << i ) ) == 0 || gameLocal.isMultiplayer ) {
					if ( (owner->GetUserInfo()->GetBool( "ui_autoSwitch" ) || !gameLocal.isMultiplayer) && idealWeapon ) {
						// HUMANHEAD pdm: added spirit check, so we don't autoswitch to weapons when picking them up in spriitwalk
						// HUMANHEAD pdm: also added check for spiritweapon, don't autoswitch to it.
						if (!static_cast<hhPlayer*>(owner)->IsSpiritOrDeathwalking() && weaponName.Icmp("weaponobj_bow")) {
							assert( !gameLocal.isClient );
							*idealWeapon = i;
						}
					}

					// Pulse if not picking up the spirit bow
					if (weaponName.Icmp("weaponobj_bow") != 0) {
						weaponPulse = true;
					}
					weapons |= ( 1 << i );
					tookWeapon = true;
				}
			}
		}
		return tookWeapon;
	}
	else if ( !idStr::Icmp( statname, "maxhealth" ) ) {
		if (owner) {
			if (gameLocal.GetLocalPlayer() == owner) { //sp, listen server
				if (owner->hud) {
					owner->hud->HandleNamedEvent( "maxHealthPulse" );
				}
			}
			else if (gameLocal.isMultiplayer && !gameLocal.isClient) { //otherwise, broadcast event to owner
				idBitMsg	msg;
				byte		msgBuf[MAX_EVENT_PARAM_SIZE];

				msg.Init(msgBuf, sizeof(msgBuf));
				msg.WriteBits((1<<idPlayer::MENU_NET_EVENT_MAXHEALTHPULSE), idPlayer::MENU_NET_EVENT_NUM);
				owner->ServerSendEvent(idPlayer::EVENT_MENUEVENT, &msg, false, -1, owner->entityNumber);
			}
		}
		if (gameLocal.isMultiplayer) { //rww - different behaviour for mp
			maxHealth = atoi(value);
		}
		else {
			maxHealth += atoi(value);
		}
	}
	else if ( !idStr::Icmp( statname, "maxspirit" ) ) {
		owner->hud->HandleNamedEvent( "maxSpiritPulse" );
		maxSpirit += atoi(value);
	}
	else {
		// unknown item
		return false;
	}

	return true;
}

/*
==============
hhInventory::GiveItem
==============
*/
bool hhInventory::GiveItem( const idDict& spawnArgs, const idDict* item ) {
	idStr	itemName;

	if( !item ) {
		return false;
	}

	idDict* dict = new idDict( *item );

	if( !FindItem(item) ) {
		items.Append( dict );
		return true;
	}

	SAFE_DELETE_PTR( dict );
	return false;
}

/*
===============
hhInventory::HasAmmo
===============
*/
int hhInventory::HasAmmo( ammo_t type, int amount ) {
	assert(type >= 0 && type < AMMO_NUMTYPES);

	if ( ( type == idWeapon::GetAmmoNumForName("ammo_none") ) || !amount ) {
		// always allow weapons that don't use ammo to fire
		return -1;
	}

	// check if we have infinite ammo
	if ( ammo[ type ] < 0 ) {
		return -1;
	}

	// return how many shots we can fire
	return ammo[ type ] / amount;
}

/*
===============
hhInventory::HasAmmo
===============
*/
int hhInventory::HasAmmo( const char *weapon_classname ) {
	int ammoRequired;
	ammo_t ammo_i = AmmoIndexForWeaponClass( weapon_classname, &ammoRequired );
	return HasAmmo( ammo_i, ammoRequired );
}

/*
===============
hhInventory::HasAltAmmo
HUMANHEAD bjk
===============
*/
int hhInventory::HasAltAmmo( const char *weapon_classname ) {
	int ammoRequired;
	ammo_t ammo_i = AltAmmoIndexForWeaponClass( weapon_classname, &ammoRequired );
	return HasAmmo( ammo_i, ammoRequired );
}

/*
===============
hhInventory::UseAmmo
===============
*/
bool hhInventory::UseAmmo( ammo_t type, int amount ) {
	if ( !HasAmmo( type, amount ) ) {
		return false;
	}

	// take an ammo away if not infinite
	if ( ammo[ type ] >= 0 ) {
		ammo[ type ] -= amount;
		//rww - don't forget this, it's important!
		ammoPredictTime = gameLocal.time; // mp client: we predict this. mark time so we're not confused by snapshots
	}

	return true;
}

float hhInventory::AmmoPercentage(idPlayer *player, ammo_t type) {
	if (SplitAutocannonAmmo(player) && type == AmmoIndexForAmmoClass("ammo_autocannon")) {
		const float autoMax = Max(1, MaxAmmoForAmmoClass(player, "ammo_autocannon"));
		const float autoPct = ammo[type] < 0 ? 1.0f : idMath::ClampFloat(0, 1, ammo[type] / autoMax);
		const float beltPct = ammo[12] < 0 ? 1.0f : idMath::ClampFloat(0, 1, ammo[12] / 300.0f);
		return 0.5f * (autoPct + beltPct);
	}
	if (SplitAcidAmmo(player) && type == AmmoIndexForAmmoClass("ammo_acid")) {
		const float autoMax = Max(1, MaxAmmoForAmmoClass(player, "ammo_acid"));
		const float autoPct = ammo[type] < 0 ? 1.0f : idMath::ClampFloat(0, 1, ammo[type] / autoMax);
		const float beltPct = ammo[13] < 0 ? 1.0f : idMath::ClampFloat(0, 1, ammo[13] / 150.0f);
		return 0.5f * (autoPct + beltPct);
	}
	if (SplitRocketAmmo(player) && type == AmmoIndexForAmmoClass("ammo_crawler_red")) {
		const float autoMax = Max(1, MaxAmmoForAmmoClass(player, "ammo_crawler_red"));
		const float autoPct = ammo[type] < 0 ? 1.0f : idMath::ClampFloat(0, 1, ammo[type] / autoMax);
		const float beltPct = ammo[14] < 0 ? 1.0f : idMath::ClampFloat(0, 1, ammo[14] / 12.0f);
		return 0.5f * (autoPct + beltPct);
	}
	if (SplitRifleAmmo(player) && type == AmmoIndexForAmmoClass("ammo_rifle")) {
		const int shells = AmmoIndexForAmmoClass("ammo_d3shells");
		const float rifleMax = Max(1, MaxAmmoForAmmoClass(player, "ammo_rifle"));
		const float shellMax = Max(1, MaxAmmoForAmmoClass(player, "ammo_d3shells"));
		const float riflePct = ammo[type] < 0 ? 1.0f : idMath::ClampFloat(0, 1, ammo[type] / rifleMax);
		const float shellPct = ammo[shells] < 0 ? 1.0f : idMath::ClampFloat(0, 1, ammo[shells] / shellMax);
		if (RifleGroupAmmoCount(player) == 3) {
			const float mgMax = Max(1, MaxAmmoForAmmoClass(player, "ammo_d3bullets"));
			const float mgPct = ammo[11] < 0 ? 1.0f : idMath::ClampFloat(0, 1, ammo[11] / mgMax);
			return (riflePct + shellPct + mgPct) / 3.0f;
		}
		return 0.5f * (riflePct + shellPct);
	}

	float amount = ammo[type];
	const char *ammoName = idWeapon::GetAmmoNameForNum( type );
	float max_percent = MaxAmmoForAmmoClass( player, ammoName );
	max_percent = Max(1.0f, max_percent);
	return amount / max_percent;
}

/*
===============
hhInventory::FindInventoryItem
===============
*/
idDict* hhInventory::FindItem( const idDict* dict ) {
	if( !dict ) {
		return NULL;
	}

	return FindItem( dict->GetString("inv_name") );
}

/*
===============
hhInventory::FindInventoryItem
===============
*/
idDict* hhInventory::FindItem( const char *name ) {
	const char* lname = NULL;

	for( int ix = 0; ix < items.Num(); ++ix ) {
		if( !items[ix] ) {
			continue;
		}
	
		lname = items[ix]->GetString( "inv_name" );
		if ( lname && *lname ) {
			if ( idStr::Icmp( name, lname ) == 0 ) {
				return items[ix];
			}
		}
	}
	return NULL;
}

/*
===============
hhInventory::EvaluateRequirements
===============
*/
void hhInventory::EvaluateRequirements(idPlayer *p) {
	if (p) {
		requirements.bCanDeathWalk =	gameLocal.RequirementMet(p, p->spawnArgs.GetString("requirement_deathwalk"), 0);
		requirements.bCanSpiritWalk =	gameLocal.RequirementMet(p, p->spawnArgs.GetString("requirement_spiritwalk"), 0);
		requirements.bCanSummonTalon =	gameLocal.RequirementMet(p, p->spawnArgs.GetString("requirement_talon"), 0);
		requirements.bCanUseBowVision =	gameLocal.RequirementMet(p, p->spawnArgs.GetString("requirement_bowvision"), 0);
		requirements.bCanUseLighter =	gameLocal.RequirementMet(p, p->spawnArgs.GetString("requirement_lighter"), 0);
		requirements.bCanWallwalk =		gameLocal.RequirementMet(p, p->spawnArgs.GetString("requirement_wallwalk"), 0);
		requirements.bHunterHand =		gameLocal.RequirementMet(p, p->spawnArgs.GetString("requirement_hunterhand"), 0);

		// See if talon should be spawned
		if (p->IsType(hhPlayer::Type)) {
			static_cast<hhPlayer*>(p)->TrySpawnTalon();
		}
	}
}

