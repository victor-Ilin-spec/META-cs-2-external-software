#pragma once

namespace offsets
{
	// Module: client.dll
	constexpr std::ptrdiff_t dwCSGOInput = 0x257B640;
	constexpr std::ptrdiff_t dwEntityList = 0x271A908;
	constexpr std::ptrdiff_t dwGameEntitySystem = 0x271A908;
	constexpr std::ptrdiff_t dwGameEntitySystem_highestEntityIndex = 0x2120;
	constexpr std::ptrdiff_t dwGameRules = 0x25651D0;
	constexpr std::ptrdiff_t dwGlobalVars = 0x2230090;
	constexpr std::ptrdiff_t dwGlowManager = 0x2561EF0;
	constexpr std::ptrdiff_t dwLocalPlayerController = 0x253CB78;
	constexpr std::ptrdiff_t dwLocalPlayerPawn = 0x2565718;
	constexpr std::ptrdiff_t dwPlantedC4 = 0x24CB3C0;
	constexpr std::ptrdiff_t dwPrediction = 0x2565620;
	constexpr std::ptrdiff_t dwViewAngles = 0x257BCC8;
	constexpr std::ptrdiff_t dwViewMatrix = 0x256B020;
	constexpr std::ptrdiff_t dwViewRender = 0x256B9E8;
	constexpr std::ptrdiff_t dwWeaponC4 = 0x24C9B30;

	// Runtime — заполняются дампером (Dumper.cpp) по сигнатурам при инициализации
	inline DWORD64 viewMatrix = 0;
	inline DWORD64 globalVars = 0;
	inline DWORD64 entityList = 0;
	inline DWORD64 localPlayerController = 0;
	inline DWORD64 plantedC4 = 0;
	inline DWORD64 weaponC4 = 0;
	inline DWORD64 gameRules = 0;
	inline DWORD64 autoAccept = 0;
	inline DWORD64 sensitivity = 0;
	inline DWORD64 sensitivity_value = 0;



	// engine2.dll
	inline DWORD buildNumber;
	namespace controller {
		constexpr std::ptrdiff_t dwBuildNumber = 0x61CFEC;
		constexpr std::ptrdiff_t dwNetworkGameClient = 0x91AFC0;
		constexpr std::ptrdiff_t dwNetworkGameClient_clientTickCount = 0x398;
		constexpr std::ptrdiff_t dwNetworkGameClient_deltaTick = 0x24C;
		constexpr std::ptrdiff_t dwNetworkGameClient_isBackgroundMap = 0x2C143F;
		constexpr std::ptrdiff_t dwNetworkGameClient_localPlayer = 0xF8;
		constexpr std::ptrdiff_t dwNetworkGameClient_maxClients = 0x240;
		constexpr std::ptrdiff_t dwNetworkGameClient_serverTickCount = 0x24C;
		constexpr std::ptrdiff_t dwNetworkGameClient_signOnState = 0x230;
		constexpr std::ptrdiff_t dwWindowHeight = 0x91F33C;
		constexpr std::ptrdiff_t dwWindowWidth = 0x91F338;

		// CBasePlayerController / CCSPlayerController
		constexpr std::ptrdiff_t m_hPawn = 0x6BC; // CHandle<C_BasePlayerPawn>
		constexpr std::ptrdiff_t m_iszPlayerName = 0x6FC; // char[128]
		constexpr std::ptrdiff_t m_steamID = 0x788; // uint64
		constexpr std::ptrdiff_t m_bIsLocalPlayerController = 0x790; // bool
		constexpr std::ptrdiff_t m_iPing = 0x838; // uint32 - CCSPlayerController
		constexpr std::ptrdiff_t m_iTeamNum = 0x3E7; // uint8 - C_BaseEntity
	}

	namespace pawn {
		constexpr std::ptrdiff_t m_vOldOrigin = 0x14A4; // Vector
		constexpr std::ptrdiff_t m_iHealth = 0x34C; // int32
		constexpr std::ptrdiff_t m_iTeamNum = 0x3E7; // uint8
		constexpr std::ptrdiff_t m_bIsScoped = 0x1E98; // bool
		constexpr std::ptrdiff_t m_ArmorValue = 0x1EC8; // int32
		constexpr std::ptrdiff_t m_bIsDefusing = 0x1E9A; // bool
		constexpr std::ptrdiff_t m_vecAbsVelocity = 0x3F8; // Vector
		constexpr std::ptrdiff_t m_fFlags = 0x3F4; // uint32
		constexpr std::ptrdiff_t m_iShotsFired = 0x1EB0; // int32
		constexpr std::ptrdiff_t m_pAimPunchServices = 0x1598; // CCSPlayer_AimPunchServices*
		constexpr std::ptrdiff_t m_aimPunchAngle = 0x50; // QAngle fallback (m_predictableBaseAngle)
		constexpr std::ptrdiff_t m_aimPunchCache = 0x88; // CUtlVector<QAngle> - removed from schema 2026-10-19, may be unreliable, fallback m_aimPunchAngle used
		constexpr std::ptrdiff_t m_iIDEntIndex = 0x36DC; // CEntityIndex
		constexpr std::ptrdiff_t m_angEyeAngles = 0x3600; // QAngle

		constexpr std::ptrdiff_t m_pGameSceneNode = 0x330; // CGameSceneNode*
		constexpr std::ptrdiff_t m_hOwnerEntity = 0x520; // CHandle<C_BaseEntity>

		
		constexpr std::ptrdiff_t m_entitySpottedState = 0x1E80; // EntitySpottedState_t
		constexpr std::ptrdiff_t m_bSpotted = 0x8; // bool - EntitySpottedState_t
		constexpr std::ptrdiff_t m_bSpottedByMask = 0xC; // uint32[2] - EntitySpottedState_t
		
		constexpr std::ptrdiff_t m_flFlashOverlayAlpha = 0x1504; // float32 - C_CSPlayerPawnBase
		constexpr std::ptrdiff_t m_flFlashDuration = 0x1510; // float32 - C_CSPlayerPawnBase
		constexpr std::ptrdiff_t m_pWeaponServices = 0x12F0; // CPlayer_WeaponServices*
		constexpr std::ptrdiff_t m_pItemServices = 0x12F8; // CPlayer_ItemServices*
		constexpr std::ptrdiff_t m_hActiveWeapon = 0x60; // CHandle<C_BasePlayerWeapon> - CPlayer_WeaponServices
		constexpr std::ptrdiff_t m_AttributeManager = 0x1290; // C_AttributeContainer - C_EconEntity (parent of C_BasePlayerWeapon)
		constexpr std::ptrdiff_t m_Item = 0x50; // C_EconItemView - C_AttributeContainer
		constexpr std::ptrdiff_t m_iItemDefinitionIndex = 0x1BA; // uint16 - C_EconItemView
		constexpr std::ptrdiff_t m_iClip1 = 0x1928; // int32 - C_BasePlayerWeapon
		constexpr std::ptrdiff_t m_bInReload = 0x1A3C; // bool - C_CSWeaponBase
		constexpr std::ptrdiff_t m_fAccuracyPenalty = 0x1A18; // float32 - C_CSWeaponBase
		constexpr std::ptrdiff_t m_flTurningInaccuracy = 0x1A14; // float32 - C_CSWeaponBase
		constexpr std::ptrdiff_t m_iRecoilIndex = 0x1A24; // int32 - C_CSWeaponBase
		constexpr std::ptrdiff_t m_zoomLevel = 0x1F20; // int32 - C_CSWeaponBaseGun
		constexpr std::ptrdiff_t m_pObserverServices = 0x1308; // CPlayer_ObserverServices*
		constexpr std::ptrdiff_t m_bGunGameImmunity = 0x3508; // bool - C_CSPlayerPawn
		constexpr std::ptrdiff_t m_bHasDefuser = 0x48; // bool - CCSPlayer_ItemServices
		constexpr std::ptrdiff_t m_bHasHelmet = 0x49; // bool - CCSPlayer_ItemServices
	}

	namespace bomb {
		constexpr std::ptrdiff_t m_isPlanted = 0x8; // unk
		constexpr std::ptrdiff_t m_bC4Activated = 0x12D0; // bool - C_PlantedC4
		constexpr std::ptrdiff_t m_nBombSite = 0x128C; // int32 - C_PlantedC4

		constexpr std::ptrdiff_t m_vecAbsOrigin = 0xC8; // VectorWS - CGameSceneNode 
	}

	namespace bone {
		constexpr std::ptrdiff_t m_modelState = 0x140; // CModelState
	}

	namespace observerServices {
		constexpr std::ptrdiff_t m_iObserverMode = 0x48;
		constexpr std::ptrdiff_t m_hObserverTarget = 0x4C;
	}

	namespace global {
		constexpr std::ptrdiff_t currentTime = 0x2C;
		constexpr std::ptrdiff_t tickCount = 0x40;
		constexpr std::ptrdiff_t maxClients = 0x10;
		constexpr std::ptrdiff_t currentMapName = 0x180;
	}

	namespace rules {
		constexpr std::ptrdiff_t m_iFreezeTime = 0x64; // int32
		constexpr std::ptrdiff_t m_bMapHasBombTarget = 0x99; // bool
		constexpr std::ptrdiff_t m_bMapHasRescueZone = 0x9A; // bool
		constexpr std::ptrdiff_t m_pGameModeRules = 0xD98; // CCSGameModeRules*
		constexpr std::ptrdiff_t m_flDMBonusStartTime = 0x30; // GameTime_t
		constexpr std::ptrdiff_t m_flDMBonusTimeLength = 0x34; // float32
	}

	namespace buttons {
		constexpr std::ptrdiff_t jump = 0x22343E0; // dwForceJump a2x 2026-10-19
	}

	namespace signatures
	{
		const std::string viewMatrix = "48 8D 0D ?? ?? ?? ?? 48 C1 E0 06";
		const std::string globalVars = "48 89 15 ?? ?? ?? ?? 48 89 42";
		const std::string entityList = "48 8B 0D ?? ?? ?? ?? 48 89 7C 24 ?? 8B FA C1 EB";
		const std::string localPlayerController = "48 8B 05 ?? ?? ?? ?? 41 89 BE";
		const std::string plantedC4 = "48 8B 1D ?? ?? ?? ?? 45 32 F6";
		const std::string gameRules = "48 8B 0D ?? ?? ?? ?? 48 85 C9 0F 84 ?? ?? ?? ?? 8B 81";
		const std::string autoAccept = "48 89 05 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 85 DB";
		const std::string weaponC4 =
			"48 89 05 ?? ?? ?? ?? "
			"F7 C1 ?? ?? ?? ?? "
			"74 ?? "
			"81 E1 ?? ?? ?? ?? "
			"89 0D ?? ?? ?? ?? "
			"8B 05 ?? ?? ?? ?? "
			"89 1D ?? ?? ?? ?? "
			"EB ?? "
			"48 8B 15 ?? ?? ?? ?? "
			"48 8B 5C 24 ?? "
			"FF C0 "
			"89 05 ?? ?? ?? ?? "
			"48 8B C6 48 89 34 EA 80 BE";

#if 0
		const std::string localPlayerPawn = "48 8D 05 ?? ?? ?? ?? C3 CC CC CC CC CC CC CC CC 48 83 EC ?? 8B 0D";

		const std::string csgoInput = "48 89 05 ?? ?? ?? ?? 0F 57 C0 0F 11 05";
		const std::string viewAngles = "F2 42 0F 10 84 28 ?? ?? ?? ??";
#endif

		const std::string buildNumber = "89 05 ?? ?? ?? ?? 48 8d 0d ?? ?? ?? ?? ff 15 ?? ?? ?? ?? 48 8b 0d";

	}
}