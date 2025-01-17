//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "cbase.h"
#include "basehlcombatweapon_shared.h"

#include "hl2_player_shared.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "engine/IEngineSound.h"
#include "in_buttons.h" 

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS(basehlcombatweapon, CBaseHLCombatWeapon);

IMPLEMENT_NETWORKCLASS_ALIASED(BaseHLCombatWeapon, DT_BaseHLCombatWeapon)

BEGIN_NETWORK_TABLE(CBaseHLCombatWeapon, DT_BaseHLCombatWeapon)
#if !defined( CLIENT_DLL )
//	SendPropInt( SENDINFO( m_bReflectViewModelAnimations ), 1, SPROP_UNSIGNED ),
#else
//	RecvPropInt( RECVINFO( m_bReflectViewModelAnimations ) ),
#endif
END_NETWORK_TABLE()


#if !defined( CLIENT_DLL )

#include "globalstate.h"

//---------------------------------------------------------
// Save/Restore
//---------------------------------------------------------
BEGIN_DATADESC(CBaseHLCombatWeapon)

DEFINE_FIELD(m_bLowered, FIELD_BOOLEAN),
DEFINE_FIELD(m_flRaiseTime, FIELD_TIME),
DEFINE_FIELD(m_flHolsterTime, FIELD_TIME),
DEFINE_FIELD(m_iPrimaryAttacks, FIELD_INTEGER),
DEFINE_FIELD(m_iSecondaryAttacks, FIELD_INTEGER),





END_DATADESC()

#endif

BEGIN_PREDICTION_DATA(CBaseHLCombatWeapon)
END_PREDICTION_DATA()

ConVar sk_auto_reload_time("sk_auto_reload_time", "3", FCVAR_REPLICATED);

ConVar sv_recoil_override("sv_recoil_override", "", FCVAR_CHEAT, "USAGE: min:[pitch yaw roll] max:[pitch yaw roll]. Use just min if you want no randomness.");



#ifdef GAME_DLL
QAngle CBaseHLCombatWeapon::GetRecoil()
{
	const FileWeaponInfo_t& info = GetWpnData();

	if (V_strcmp(sv_recoil_override.GetString(), "") != 0)
	{
		QAngle recoil[2]; // min and max
		UTIL_StringToFloatArray(recoil[0].Base(), 6, sv_recoil_override.GetString());

		if (recoil[1] != vec3_angle) // This weapon wants random min/max-based recoil
		{
			return QAngle(RandomFloat(recoil[0].x, recoil[1].x),
				RandomFloat(recoil[0].y, recoil[1].y),
				RandomFloat(recoil[0].z, recoil[1].z));
		}
		else // This weapon wants fixed recoil
		{
			return recoil[0];
		}
	}

	if (info.recoilMax != vec3_angle) // This weapon wants random min/max-based recoil
	{
		return QAngle(RandomFloat(info.recoilMin.x, info.recoilMax.x),
			RandomFloat(info.recoilMin.y, info.recoilMax.y),
			RandomFloat(info.recoilMin.z, info.recoilMax.z));
	}
	else // This weapon wants fixed recoil
	{
		return info.recoilMin;
	}
}
#endif


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseHLCombatWeapon::ItemHolsterFrame(void)
{
	BaseClass::ItemHolsterFrame();

	// Must be player held
	if (GetOwner() && GetOwner()->IsPlayer() == false)
		return;

	// We can't be active
	if (GetOwner()->GetActiveWeapon() == this)
		return;

	// Detect the "use" key press (IN_USE represents the use key)
	CHL2_Player* pPlayer = assert_cast<CHL2_Player*>(GetOwner());
	if (pPlayer && (pPlayer->m_nButtons & IN_USE) && !m_bLowered)
	{
		// Lower the weapon if it's not already lowered
		Lower();
		m_flLowerTime = gpGlobals->curtime + 1.0f; // Lower time lasts 1 second
	}

	// If the weapon has been lowered for 1 second, raise it back up
	if (m_bLowered && gpGlobals->curtime >= m_flLowerTime)
	{
		Ready(); // Raise the weapon after 1 second
	}

	// If it's been longer than the reload time, reload
	if ((gpGlobals->curtime - m_flHolsterTime) > sk_auto_reload_time.GetFloat())
	{
		// Just load the clip with no animations
		FinishReload();
		m_flHolsterTime = gpGlobals->curtime;
	}
}

bool CBaseHLCombatWeapon::CanSprint()
{
	CHL2_Player *pPlayer = assert_cast<CHL2_Player *>(GetOwner());
	if (pPlayer && SelectWeightedSequence(ACT_VM_SPRINT) == ACTIVITY_NOT_AVAILABLE)
		return false;

	return true;
}

bool CBaseHLCombatWeapon::CanWalkBob()
{
	CHL2_Player *pPlayer = assert_cast<CHL2_Player *>(GetOwner());
	if (pPlayer && SelectWeightedSequence(ACT_VM_WALK) == ACTIVITY_NOT_AVAILABLE)
		return false;

	return true;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::CanLower()
{
	if (SelectWeightedSequence(ACT_VM_IDLE_LOWERED) == ACTIVITY_NOT_AVAILABLE)
		return false;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Drops the weapon into a lowered pose
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::Lower(void)
{
	//Don't bother if we don't have the animation
	if (SelectWeightedSequence(ACT_VM_IDLE_LOWERED) == ACTIVITY_NOT_AVAILABLE)
		return false;

	m_bLowered = true;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Brings the weapon up to the ready position
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::Ready(void)
{
	//Don't bother if we don't have the animation
	if (SelectWeightedSequence(ACT_VM_LOWERED_TO_IDLE) == ACTIVITY_NOT_AVAILABLE)
		return false;

	m_bLowered = false;
	m_flRaiseTime = gpGlobals->curtime + 0.5f;
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::Deploy(void)
{
	// If we should be lowered, deploy in the lowered position
	// We have to ask the player if the last time it checked, the weapon was lowered
	if (GetOwner() && GetOwner()->IsPlayer())
	{
		CHL2_Player *pPlayer = assert_cast<CHL2_Player*>(GetOwner());
		if (pPlayer->IsWeaponLowered())
		{
			if (SelectWeightedSequence(ACT_VM_IDLE_LOWERED) != ACTIVITY_NOT_AVAILABLE)
			{
				if (DefaultDeploy((char*)GetViewModel(), (char*)GetWorldModel(), ACT_VM_IDLE_LOWERED, (char*)GetAnimPrefix()))
				{
					m_bLowered = true;

					// Stomp the next attack time to fix the fact that the lower idles are long
					pPlayer->SetNextAttack(gpGlobals->curtime + 1.0);
					m_flNextPrimaryAttack = gpGlobals->curtime + 1.0;
					m_flNextSecondaryAttack = gpGlobals->curtime + 1.0;
					return true;
				}
			}
		}
	}

	m_bLowered = false;
	return BaseClass::Deploy();
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::Holster(CBaseCombatWeapon *pSwitchingTo)
{
	if (BaseClass::Holster(pSwitchingTo))
	{
		m_flHolsterTime = gpGlobals->curtime;
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CBaseHLCombatWeapon::WeaponShouldBeLowered(void)
{
	// Can't be in the middle of another animation
	if (GetIdealActivity() != ACT_VM_IDLE_LOWERED && GetIdealActivity() != ACT_VM_IDLE &&
		GetIdealActivity() != ACT_VM_IDLE_TO_LOWERED && GetIdealActivity() != ACT_VM_LOWERED_TO_IDLE)
		return false;

	if (m_bLowered)
		return true;

#if !defined( CLIENT_DLL )

	if (GlobalEntity_GetState("friendly_encounter") == GLOBAL_ON)
		return true;

#endif

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Allows the weapon to choose proper weapon idle animation
//-----------------------------------------------------------------------------
void CBaseHLCombatWeapon::WeaponIdle(void)
{
	CHL2_Player* pPlayer = static_cast<CHL2_Player*>(GetOwner());
	if (!pPlayer)
		return;

	float speed = pPlayer->GetLocalVelocity().Length2D();

	// If the player is sprinting but cannot sprint, lower the weapon
	if (pPlayer->IsSprinting() && !CanSprint())  
	{
		if (GetActivity() != GetIdleLoweredActivity() && GetActivity() != ACT_VM_IDLE_TO_LOWERED)
		{
			SendWeaponAnim(GetIdleLoweredActivity());
		}
		return;  
	}

	if (pPlayer->IsSprinting() && speed >= 290)
	{
		int iActivity = GetActivity();
		if (HasWeaponIdleTimeElapsed() ||
			(GetActivity() == GetIdleActivity() ||
				GetActivity() == GetWalkActivity() ||
				GetActivity() == GetIdleLoweredActivity()) ||
			GetActivity() == ACT_VM_IDLE_TO_LOWERED ||
			GetActivity() == ACT_VM_LOWERED_TO_IDLE)
		{
			iActivity = GetSprintActivity();
		}

		int iSequence = SelectWeightedSequence(GetIdleActivity());
		if (iSequence >= 0 && iActivity != GetActivity())
		{
			SendWeaponAnim(iActivity);
		}
	}
	else if (WeaponShouldBeLowered())
	{
#if !defined(CLIENT_DLL)
		pPlayer->Weapon_Lower();
#endif
		// Move to lowered position if we're not there yet
		if (GetActivity() != GetIdleLoweredActivity() && GetActivity() != ACT_VM_IDLE_TO_LOWERED && GetActivity() != ACT_TRANSITION)
		{
			SendWeaponAnim(GetIdleLoweredActivity());
		}
		else if (HasWeaponIdleTimeElapsed())
		{
			// Keep idling low
			SendWeaponAnim(GetIdleLoweredActivity());
		}
	}
	else if (CanWalkBob() && speed >= 110 && pPlayer->GetWaterLevel() != 3 && (pPlayer->GetFlags() & FL_ONGROUND))
	{
		if (GetActivity() != GetWalkActivity() && (GetActivity() == GetIdleActivity() || GetActivity() == GetSprintActivity()))
		{
			SendWeaponAnim(GetWalkActivity());
		}
		else if (HasWeaponIdleTimeElapsed())
		{
			SendWeaponAnim(GetWalkActivity());
		}
	}
	else
	{
		// See if we need to raise immediately
		if (m_flRaiseTime < gpGlobals->curtime && GetActivity() == GetIdleLoweredActivity())
		{
			SendWeaponAnim(GetIdleActivity());
		}
		else if (speed <= (pPlayer->IsSuitEquipped() ? 300 : 200) && GetActivity() == GetSprintActivity())
		{
			SendWeaponAnim(GetIdleActivity());
		}
		else if (speed <= 100 && GetActivity() == GetWalkActivity())
		{
			SendWeaponAnim(GetIdleActivity());
		}
		else if (HasWeaponIdleTimeElapsed())
		{
			if (gpGlobals->curtime >= m_flNextFidgetTime)
			{
				SendWeaponAnim(ACT_VM_FIDGET);
				m_flNextFidgetTime = gpGlobals->curtime + 9.0f;

				// Player will say random voice lines (old code)
				CPASAttenuationFilter filter(this);
				filter.UsePredictionRules();
				EmitSound(filter, entindex(), "Player.Rand");
			}
			else
			{
				SendWeaponAnim(GetIdleActivity());
			}
		}
	}
	}



float	g_lateralBob;
float	g_verticalBob;

#if defined( CLIENT_DLL ) && ( !defined( HL2MP ) && !defined( ARSENIO ) )

#define	HL2_BOB_CYCLE_MIN	1.0f
#define	HL2_BOB_CYCLE_MAX	0.45f
#define	HL2_BOB			0.002f
#define	HL2_BOB_UP		0.5f


static ConVar	cl_bobcycle( "cl_bobcycle","0.8" );
static ConVar	cl_bob( "cl_bob","0.002" );
static ConVar	cl_bobup( "cl_bobup","0.5" );

// Register these cvars if needed for easy tweaking
static ConVar	v_iyaw_cycle( "v_iyaw_cycle", "2"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_iroll_cycle( "v_iroll_cycle", "0.5"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_ipitch_cycle( "v_ipitch_cycle", "1"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_iyaw_level( "v_iyaw_level", "0.3"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_iroll_level( "v_iroll_level", "0.1"/*, FCVAR_UNREGISTERED*/ );
static ConVar	v_ipitch_level( "v_ipitch_level", "0.3"/*, FCVAR_UNREGISTERED*/ );



//-----------------------------------------------------------------------------
// Purpose: 
// Output: float
//-----------------------------------------------------------------------------
float CBaseHLCombatWeapon::CalcViewmodelBob(void)
{
	CBasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (!pPlayer)
		return 0.0f;



	static float bobtime;
	static float lastbobtime;
	float cycle;

	// Find the speed of the player
	float speed = pPlayer->GetLocalVelocity().Length2D();

	// NOTENOTE: For now, let this cycle continue when in the air, because it snaps badly without it
	if ((!gpGlobals->frametime) || (speed == 0) || !pPlayer->GetGroundEntity())
	{
		// Just use the old value if not moving or on the ground
		return g_verticalBob;
	}

	float flRunningSpeed = MAX(1, pPlayer->GetSequenceGroundSpeed(pPlayer->GetSequence()));
	float flDeltaTime = (gpGlobals->curtime - lastbobtime) * flRunningSpeed;
	bobtime += flDeltaTime;

	// Calculate the vertical bob
	cycle = bobtime - (int)(bobtime / HL2_BOB_CYCLE_MAX) * HL2_BOB_CYCLE_MAX;
	cycle /= HL2_BOB_CYCLE_MAX;

	if (cycle < HL2_BOB_UP)
	{
		cycle = M_PI * cycle / HL2_BOB_UP;
	}
	else
	{
		cycle = M_PI + M_PI * (cycle - HL2_BOB_UP) / (1.0 - HL2_BOB_UP);
	}

	g_verticalBob = speed * 0.005f;
	g_verticalBob = g_verticalBob * 0.3 + g_verticalBob * 0.7 * sin(cycle);

	// Calculate the lateral bob
	cycle = bobtime - (int)(bobtime / HL2_BOB_CYCLE_MAX * 2) * HL2_BOB_CYCLE_MAX * 2;
	cycle /= HL2_BOB_CYCLE_MAX * 2;

	if (cycle < HL2_BOB_UP)
	{
		cycle = M_PI * cycle / HL2_BOB_UP;
	}
	else
	{
		cycle = M_PI + M_PI * (cycle - HL2_BOB_UP) / (1.0 - HL2_BOB_UP);
	}

	g_lateralBob = speed * 0.01f;
	g_lateralBob = g_lateralBob * 0.3 + g_lateralBob * 0.7 * sin(cycle);

	lastbobtime = gpGlobals->curtime;

	return g_verticalBob;
}
//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&angles - 
//			viewmodelindex - 
//-----------------------------------------------------------------------------
void CBaseHLCombatWeapon::AddViewmodelBob(CBaseViewModel *viewmodel, Vector &origin, QAngle &angles)
{



		Vector    forward, right;
		AngleVectors(angles, &forward, &right, NULL);

		CalcViewmodelBob();

		// Apply bob, but scaled down to 40%
		VectorMA(origin, g_verticalBob * 0.3f, forward, origin);

		// Z bob a bit more
		origin[2] += g_verticalBob * 0.3f;

	// bob the angles
	angles[ROLL] += g_verticalBob * 0.3f;
	angles[PITCH] -= g_verticalBob * 0.3f;

	angles[YAW] -= g_lateralBob  * 0.3f;



	// special adjustment for crowbar slash attack
	C_BasePlayer* pPlayer = ToBasePlayer(GetOwner());
	if (pPlayer && pPlayer->m_nMeleeState == MELEE_SLASH &&
		FClassnameIs(STRING(pPlayer->GetActiveWeapon()), "weapon_crowbar"))
	{
		m_flRollAdj = 50.0f;
	}
	else
	{
		m_flRollAdj = MAX(m_flRollAdj - (gpGlobals->frametime * 360), 0.0f);
	}
	angles[ROLL] += m_flRollAdj;
	angles[YAW] -= m_flRollAdj / 4.0f;
	if (m_flBobKickZ < 5.0f && m_flBobKickZ > -5.0f)
		angles[PITCH] -= m_flBobKickZ;

	VectorMA(origin, g_lateralBob * 0.3f, right, origin);

	//ConVar g_lateralBob("g_lateralBob", "0.3", FCVAR_REPLICATED);

	// Command won't work


}

//-----------------------------------------------------------------------------
Vector CBaseHLCombatWeapon::GetBulletSpread(WeaponProficiency_t proficiency)
{
	return BaseClass::GetBulletSpread(proficiency);
}

//-----------------------------------------------------------------------------
float CBaseHLCombatWeapon::GetSpreadBias(WeaponProficiency_t proficiency)
{
	return BaseClass::GetSpreadBias(proficiency);
}
//-----------------------------------------------------------------------------

const WeaponProficiencyInfo_t *CBaseHLCombatWeapon::GetProficiencyValues()
{
	return NULL;
}

#else

// Server stubs
float CBaseHLCombatWeapon::CalcViewmodelBob(void)
{
	return 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &origin - 
//			&angles - 
//			viewmodelindex - 
//-----------------------------------------------------------------------------
void CBaseHLCombatWeapon::AddViewmodelBob(CBaseViewModel *viewmodel, Vector &origin, QAngle &angles)
{
}


//-----------------------------------------------------------------------------
Vector CBaseHLCombatWeapon::GetBulletSpread(WeaponProficiency_t proficiency)
{
	Vector baseSpread = BaseClass::GetBulletSpread(proficiency);

	const WeaponProficiencyInfo_t *pProficiencyValues = GetProficiencyValues();
	float flModifier = (pProficiencyValues)[proficiency].spreadscale;
	return (baseSpread * flModifier);
}

//-----------------------------------------------------------------------------
float CBaseHLCombatWeapon::GetSpreadBias(WeaponProficiency_t proficiency)
{
	const WeaponProficiencyInfo_t *pProficiencyValues = GetProficiencyValues();
	return (pProficiencyValues)[proficiency].bias;
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CBaseHLCombatWeapon::GetProficiencyValues()
{
	return GetDefaultProficiencyValues();
}

//-----------------------------------------------------------------------------
const WeaponProficiencyInfo_t *CBaseHLCombatWeapon::GetDefaultProficiencyValues()
{
	// Weapon proficiency table. Keep this in sync with WeaponProficiency_t enum in the header!!
	static WeaponProficiencyInfo_t g_BaseWeaponProficiencyTable[] =
	{
		{ 2.50, 1.0 },
		{ 2.00, 1.0 },
		{ 1.50, 1.0 },
		{ 1.25, 1.0 },
		{ 1.00, 1.0 },
	};

	COMPILE_TIME_ASSERT(ARRAYSIZE(g_BaseWeaponProficiencyTable) == WEAPON_PROFICIENCY_PERFECT + 1);

	return g_BaseWeaponProficiencyTable;
}

#endif