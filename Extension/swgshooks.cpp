/*
    This file is part of SourcePawn SteamWorks.

    SourcePawn SteamWorks is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, as per version 3 of the License.

    SourcePawn SteamWorks is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with SourcePawn SteamWorks.  If not, see <http://www.gnu.org/licenses/>.
	
	Author: Kyle Sanderson (KyleS).
*/

#include "swgshooks.h"
#include "steamtools/ticket.h"

enum
{
	eUnhooked = 0,
	eHooking,
	eHooked
};

#if !defined(STEAMWORKS_KHOOK)
SH_DECL_HOOK0(ISteamGameServer, WasRestartRequested, SH_NOATTRIB, 0, bool);
SH_DECL_HOOK3(ISteamGameServer, BeginAuthSession, SH_NOATTRIB, 0, EBeginAuthSessionResult, const void *, int, CSteamID);
SH_DECL_HOOK0_void(ISteamGameServer, LogOnAnonymous, SH_NOATTRIB, 0);
#endif

static ISteamGameServer *GetGameServerPointer()
{
	return g_SteamWorks.pSWGameServer->GetGameServer();
}

#if defined(STEAMWORKS_KHOOK)
SteamWorksGSHooks::SteamWorksGSHooks()
	: hookWasRestartRequested(&ISteamGameServer::WasRestartRequested, this, &SteamWorksGSHooks::Hook_WasRestartRequested, nullptr),
	  hookLogOnAnonymous(&ISteamGameServer::LogOnAnonymous, this, &SteamWorksGSHooks::Hook_LogOnAnonymous, nullptr),
	  hookBeginAuthSession(&ISteamGameServer::BeginAuthSession, this, &SteamWorksGSHooks::Hook_BeginAuthSession, nullptr)
{
	this->uHooked = eHooking;
	this->pFORR = forwards->CreateForward("SteamWorks_RestartRequested", ET_Hook, 0, NULL);
	this->pFOTR = forwards->CreateForward("SteamWorks_TokenRequested", ET_Ignore, 2, NULL, Param_String, Param_Cell);
	this->pOBAS = forwards->CreateForward("SteamWorks_BeginAuthSession", ET_Ignore, 3, NULL, Param_Array, Param_Cell, Param_Cell);
	
	ISteamGameServer *pGameServer = GetGameServerPointer();
	if (pGameServer)
	{
		this->AddHooks(pGameServer);
	}
	else
	{
		smutils->AddGameFrameHook(OurGameFrameHook);
	}
}
#else
SteamWorksGSHooks::SteamWorksGSHooks()
{
	this->uHooked = eHooking;
	this->pFORR = forwards->CreateForward("SteamWorks_RestartRequested", ET_Hook, 0, NULL);
	this->pFOTR = forwards->CreateForward("SteamWorks_TokenRequested", ET_Ignore, 2, NULL, Param_String, Param_Cell);
	this->pOBAS = forwards->CreateForward("SteamWorks_BeginAuthSession", ET_Ignore, 3, NULL, Param_Array, Param_Cell, Param_Cell);
	
	ISteamGameServer *pGameServer = GetGameServerPointer();
	if (pGameServer)
	{
		this->AddHooks(pGameServer);
	}
	else
	{
		smutils->AddGameFrameHook(OurGameFrameHook);
	}
}
#endif

SteamWorksGSHooks::~SteamWorksGSHooks()
{
	this->RemoveHooks(GetGameServerPointer(), true);
	smutils->RemoveGameFrameHook(OurGameFrameHook);
	forwards->ReleaseForward(this->pFORR);
	forwards->ReleaseForward(this->pFOTR);
	forwards->ReleaseForward(this->pOBAS);
}

#if defined(STEAMWORKS_KHOOK)
KHook::Return<void> SteamWorksGSHooks::Hook_LogOnAnonymous(ISteamGameServer *pServer)
{
	ISteamGameServer *pGameServer = pServer;
	if (pGameServer == NULL)
	{
		pGameServer = GetGameServerPointer();
		if (pGameServer == NULL)
		{
			return {KHook::Action::Supersede};
		}
	}

	if (this->pFOTR->GetFunctionCount() == 0)
	{
		return {KHook::Action::Ignore};
	}

	char pToken[256];
	pToken[0] = '\0';
	this->pFOTR->PushStringEx(pToken, sizeof(pToken), SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);
	this->pFOTR->PushCell(sizeof(pToken));
	this->pFOTR->Execute(NULL);

	pGameServer->LogOn(pToken);
	return {KHook::Action::Supersede};
}

KHook::Return<EBeginAuthSessionResult> SteamWorksGSHooks::Hook_BeginAuthSession(ISteamGameServer *pServer, const void *pAuthTicket, int cbAuthTicket, CSteamID steamID)
{
	(void)pServer;
	if (this->pOBAS->GetFunctionCount() != 0)
	{
		char *pszAuthTicket = reinterpret_cast<char *>(const_cast<void *>(pAuthTicket));
		this->pOBAS->PushStringEx(pszAuthTicket, cbAuthTicket, SM_PARAM_STRING_BINARY | SM_PARAM_STRING_COPY, 0);
		this->pOBAS->PushCell(cbAuthTicket);
		this->pOBAS->PushCell(steamID.GetAccountID());
		this->pOBAS->Execute(NULL);
	}
	return {KHook::Action::Ignore, k_EBeginAuthSessionResultOK};
}

KHook::Return<bool> SteamWorksGSHooks::Hook_WasRestartRequested(ISteamGameServer *pServer)
{
	bool bWasRestartRequested = hookWasRestartRequested.CallOriginal(pServer);
	if (bWasRestartRequested && this->pFORR->GetFunctionCount() != 0)
	{
		cell_t Result = Pl_Continue;
		this->pFORR->Execute(&Result);
		bWasRestartRequested = (Result >= Pl_Handled);
	}
	return {KHook::Action::Supersede, bWasRestartRequested};
}
#else
void SteamWorksGSHooks::LogOnAnonymous(void)
{
	ISteamGameServer *pGameServer = GetGameServerPointer();
	if (pGameServer == NULL)
	{
		/* Go away, this wrecks us if we want to use it later. Also; impossible. */
		RETURN_META(MRES_SUPERCEDE);
	}

	if (this->pFOTR->GetFunctionCount() == 0)
	{
		/* No plugin was loaded to handle this. Anon away; we can't break them. */
		RETURN_META(MRES_IGNORED);
	}

	char pToken[256];
	pToken[0] = '\0';
	this->pFOTR->PushStringEx(pToken, sizeof(pToken), SM_PARAM_STRING_UTF8 | SM_PARAM_STRING_COPY, SM_PARAM_COPYBACK);
	this->pFOTR->PushCell(sizeof(pToken));
	this->pFOTR->Execute(NULL);

	pGameServer->LogOn(pToken);
	RETURN_META(MRES_SUPERCEDE);
}

EBeginAuthSessionResult SteamWorksGSHooks::BeginAuthSession(const void *pAuthTicket, int cbAuthTicket, CSteamID steamID)
{
	if (this->pOBAS->GetFunctionCount() != 0)
	{
		char *pszAuthTicket = reinterpret_cast<char *>(const_cast<void *>(pAuthTicket));

		this->pOBAS->PushStringEx(pszAuthTicket, cbAuthTicket, SM_PARAM_STRING_BINARY | SM_PARAM_STRING_COPY, 0);
		this->pOBAS->PushCell(cbAuthTicket);
		this->pOBAS->PushCell(steamID.GetAccountID());
		this->pOBAS->Execute(NULL);
	}

	RETURN_META_VALUE(MRES_IGNORED, k_EBeginAuthSessionResultOK);
}

bool SteamWorksGSHooks::WasRestartRequested(void) /* Mimic SteamTools. */
{
	bool bWasRestartRequested = SH_CALL(GetGameServerPointer(), &ISteamGameServer::WasRestartRequested)();
	if (bWasRestartRequested && this->pFORR->GetFunctionCount() != 0)
	{
		cell_t Result = Pl_Continue;
		this->pFORR->Execute(&Result);
		bWasRestartRequested = (Result >= Pl_Handled);
	}

	/* With how this function works, all following will be given poisoned values from SH_Call. */
	RETURN_META_VALUE(MRES_SUPERCEDE, bWasRestartRequested); 
}
#endif

void SteamWorksGSHooks::AddHooks(ISteamGameServer *pGameServer)
{
	if (this->uHooked == eHooked || pGameServer == NULL)
	{
		return;
	}

	this->uHooked = eHooked;
#if defined(STEAMWORKS_KHOOK)
	hookWasRestartRequested.Add(pGameServer);
	hookLogOnAnonymous.Add(pGameServer);
	hookBeginAuthSession.Add(pGameServer);
#else
	SH_ADD_HOOK(ISteamGameServer, WasRestartRequested, pGameServer, SH_MEMBER(this, &SteamWorksGSHooks::WasRestartRequested), false);
	SH_ADD_HOOK(ISteamGameServer, LogOnAnonymous, pGameServer, SH_MEMBER(this, &SteamWorksGSHooks::LogOnAnonymous), false);
	SH_ADD_HOOK(ISteamGameServer, BeginAuthSession, pGameServer, SH_MEMBER(this, &SteamWorksGSHooks::BeginAuthSession), false);
#endif
}

void SteamWorksGSHooks::RemoveHooks(ISteamGameServer *pGameServer, bool destroyed)
{
	if (this->uHooked != eHooked || pGameServer == NULL)
	{
		return;
	}

#if defined(STEAMWORKS_KHOOK)
	hookWasRestartRequested.Remove(pGameServer);
	hookLogOnAnonymous.Remove(pGameServer);
	hookBeginAuthSession.Remove(pGameServer);
#else
	SH_REMOVE_HOOK(ISteamGameServer, WasRestartRequested, pGameServer, SH_MEMBER(this, &SteamWorksGSHooks::WasRestartRequested), false);
	SH_REMOVE_HOOK(ISteamGameServer, LogOnAnonymous, pGameServer, SH_MEMBER(this, &SteamWorksGSHooks::LogOnAnonymous), false);
	SH_REMOVE_HOOK(ISteamGameServer, BeginAuthSession, pGameServer, SH_MEMBER(this, &SteamWorksGSHooks::BeginAuthSession), false);
#endif
	if (destroyed)
	{
		this->uHooked = eUnhooked;
		return;
	}

	this->uHooked = eHooking;
	smutils->AddGameFrameHook(OurGameFrameHook);
}

void OurGameFrameHook(bool simulating) /* What we do for SDK independence. */
{
	ISteamGameServer *pGameServer = GetGameServerPointer();
	if (pGameServer == NULL)
	{
		return;
	}

	g_SteamWorks.pGSHooks->AddHooks(pGameServer);
	smutils->RemoveGameFrameHook(OurGameFrameHook);
}
