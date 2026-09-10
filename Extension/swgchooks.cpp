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

#include "swgchooks.h"

enum
{
	eUnhooked = 0,
	eHooking,
	eHooked
};

#if !defined(STEAMWORKS_KHOOK)
SH_DECL_HOOK3(ISteamGameCoordinator, SendMessage, SH_NOATTRIB, 0, EGCResults, uint32, const void *, uint32);
SH_DECL_HOOK1(ISteamGameCoordinator, IsMessageAvailable, SH_NOATTRIB, 0, bool, uint32 *);
SH_DECL_HOOK4(ISteamGameCoordinator, RetrieveMessage, SH_NOATTRIB, 0, EGCResults, uint32 *, void *, uint32, uint32 *);
#endif

static ISteamGameCoordinator *GetSteamGCPointer()
{
	return g_SteamWorks.pSWGameServer->GetGameCoordinator();
}

#if defined(STEAMWORKS_KHOOK)
SteamWorksGCHooks::SteamWorksGCHooks()
	: hookSendMessage(&ISteamGameCoordinator::SendMessage, this, &SteamWorksGCHooks::Hook_SendMessage, nullptr),
	  hookIsMessageAvailable(&ISteamGameCoordinator::IsMessageAvailable, this, &SteamWorksGCHooks::Hook_IsMessageAvailable, nullptr),
	  hookRetrieveMessage(&ISteamGameCoordinator::RetrieveMessage, this, &SteamWorksGCHooks::Hook_RetrieveMessage, nullptr)
{
	this->uHooked = eHooking;
	this->pGCSendMsg = forwards->CreateForward("SteamWorks_GCSendMessage", ET_Event, 3, NULL, Param_Cell, Param_String, Param_Cell);
	this->pGCMsgAvail = forwards->CreateForward("SteamWorks_GCMsgAvailable", ET_Ignore, 1, NULL, Param_Cell);
	this->pGCRetMsg = forwards->CreateForward("SteamWorks_GCRetrieveMessage", ET_Event, 4, NULL, Param_Cell, Param_String, Param_Cell, Param_Cell);
	
	ISteamGameCoordinator *pGC = GetSteamGCPointer();
	if (pGC)
	{
		this->AddHooks(pGC);
	}
	else
	{
		smutils->AddGameFrameHook(OurGCGameFrameHook);
	}
}
#else
SteamWorksGCHooks::SteamWorksGCHooks()
{
	this->uHooked = eHooking;
	this->pGCSendMsg = forwards->CreateForward("SteamWorks_GCSendMessage", ET_Event, 3, NULL, Param_Cell, Param_String, Param_Cell);
	this->pGCMsgAvail = forwards->CreateForward("SteamWorks_GCMsgAvailable", ET_Ignore, 1, NULL, Param_Cell);
	this->pGCRetMsg = forwards->CreateForward("SteamWorks_GCRetrieveMessage", ET_Event, 4, NULL, Param_Cell, Param_String, Param_Cell, Param_Cell);
	
	ISteamGameCoordinator *pGC = GetSteamGCPointer();
	if (pGC)
	{
		this->AddHooks(pGC);
	}
	else
	{
		smutils->AddGameFrameHook(OurGCGameFrameHook);
	}
}
#endif

SteamWorksGCHooks::~SteamWorksGCHooks()
{
	this->RemoveHooks(GetSteamGCPointer(), true);
	smutils->RemoveGameFrameHook(OurGCGameFrameHook);
	forwards->ReleaseForward(this->pGCSendMsg);
	forwards->ReleaseForward(this->pGCMsgAvail);
	forwards->ReleaseForward(this->pGCRetMsg);
}

#if defined(STEAMWORKS_KHOOK)
KHook::Return<EGCResults> SteamWorksGCHooks::Hook_SendMessage(ISteamGameCoordinator *pGC, uint32 unMsgType, const void *pubData, uint32 cubData)
{
	(void)pGC;
	if (this->pGCSendMsg->GetFunctionCount() == 0)
	{
		return {KHook::Action::Ignore, k_EGCResultOK};
	}

	cell_t Result = k_EGCResultOK;
	this->pGCSendMsg->PushCell(unMsgType);
	this->pGCSendMsg->PushStringEx(reinterpret_cast<char *>(const_cast<void *>(pubData)), cubData, SM_PARAM_STRING_BINARY | SM_PARAM_STRING_COPY, 0);
	this->pGCSendMsg->PushCell(cubData);
	this->pGCSendMsg->Execute(&Result);

	if (Result != k_EGCResultOK)
	{
		if (Result == -1)
		{
			Result = k_EGCResultOK;
		}
		return {KHook::Action::Supersede, static_cast<EGCResults>(Result)};
	}
	return {KHook::Action::Ignore, k_EGCResultOK};
}

KHook::Return<bool> SteamWorksGCHooks::Hook_IsMessageAvailable(ISteamGameCoordinator *pGC, uint32 *pcubMsgSize)
{
	bool res = hookIsMessageAvailable.CallOriginal(pGC, pcubMsgSize);
	if (!res)
	{
		return {KHook::Action::Supersede, false};
	}

	if (this->pGCMsgAvail->GetFunctionCount() == 0)
	{
		return {KHook::Action::Supersede, true};
	}

	uint32_t shill;
	uint32_t *pSize = pcubMsgSize;
	if (!pSize)
	{
		shill = 0;
		pSize = &shill;
	}

	this->pGCMsgAvail->PushCell(*pSize);
	this->pGCMsgAvail->Execute(NULL);
	return {KHook::Action::Supersede, true};
}

KHook::Return<EGCResults> SteamWorksGCHooks::Hook_RetrieveMessage(ISteamGameCoordinator *pGC, uint32 *punMsgType, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize)
{
	EGCResults res = hookRetrieveMessage.CallOriginal(pGC, punMsgType, pubDest, cubDest, pcubMsgSize);

	if (this->pGCRetMsg->GetFunctionCount() == 0)
	{
		return {KHook::Action::Supersede, res};
	}

	cell_t Result = k_EGCResultOK;
	if (punMsgType)
		this->pGCRetMsg->PushCell(*punMsgType);
	else
		this->pGCRetMsg->PushCell(0);

	if (pubDest)
		this->pGCRetMsg->PushStringEx(reinterpret_cast<char *>(pubDest), cubDest, SM_PARAM_STRING_BINARY | SM_PARAM_STRING_COPY, 0);
	else
		this->pGCRetMsg->PushStringEx(const_cast<char *>(""), 1, SM_PARAM_STRING_BINARY | SM_PARAM_STRING_COPY, 0);

	this->pGCRetMsg->PushCell(cubDest);

	if (pcubMsgSize)
		this->pGCRetMsg->PushCell(*pcubMsgSize);
	else
		this->pGCRetMsg->PushCell(0);

	this->pGCRetMsg->Execute(&Result);

	if (Result != k_EGCResultOK)
	{
		if (Result == -1)
		{
			Result = k_EGCResultOK;
		}
		return {KHook::Action::Supersede, static_cast<EGCResults>(Result)};
	}
	return {KHook::Action::Supersede, res};
}
#else
EGCResults SteamWorksGCHooks::SendMessage(uint32 unMsgType, const void *pubData, uint32 cubData)
{
	if (this->pGCSendMsg->GetFunctionCount() == 0)
	{
		RETURN_META_VALUE(MRES_IGNORED, k_EGCResultOK);
	}

	cell_t Result = k_EGCResultOK;
	this->pGCSendMsg->PushCell(unMsgType);
	this->pGCSendMsg->PushStringEx(reinterpret_cast<char *>(const_cast<void *>(pubData)), cubData, SM_PARAM_STRING_BINARY | SM_PARAM_STRING_COPY, 0);
	this->pGCSendMsg->PushCell(cubData);
	this->pGCSendMsg->Execute(&Result);

	if (Result != k_EGCResultOK)
	{
		if (Result == -1)
		{
			Result = k_EGCResultOK;
		}

		RETURN_META_VALUE(MRES_SUPERCEDE, static_cast<EGCResults>(Result));
	}

	RETURN_META_VALUE(MRES_IGNORED, k_EGCResultOK);
}

bool SteamWorksGCHooks::IsMessageAvailable(uint32_t *pcubMsgSize)
{
	if (this->pGCMsgAvail->GetFunctionCount() == 0)
	{
		RETURN_META_VALUE(MRES_IGNORED, false);
	}

	bool res = META_RESULT_ORIG_RET(bool);
	if (!res)
	{
		RETURN_META_VALUE(MRES_IGNORED, false);
	}

	uint32_t shill;
	if (!pcubMsgSize)
	{
		shill = 0;
		SH_CALL(GetSteamGCPointer(), &ISteamGameCoordinator::IsMessageAvailable)(&shill);
		pcubMsgSize = &shill;
	}

	this->pGCMsgAvail->PushCell(*pcubMsgSize);
	this->pGCMsgAvail->Execute(NULL);
	RETURN_META_VALUE(MRES_IGNORED, true);
}

EGCResults SteamWorksGCHooks::RetrieveMessage(uint32 *punMsgType, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize)
{
	if (this->pGCRetMsg->GetFunctionCount() == 0)
	{
		RETURN_META_VALUE(MRES_IGNORED, k_EGCResultOK);
	}

	/* Don't trust a bitch, except for 2GD. https://www.youtube.com/watch?v=MgePh_YJgrc */
	cell_t Result = k_EGCResultOK;
	EGCResults res = SH_CALL(GetSteamGCPointer(), &ISteamGameCoordinator::RetrieveMessage)(punMsgType, pubDest, cubDest, pcubMsgSize);
	if (punMsgType)
		this->pGCRetMsg->PushCell(*punMsgType);
	else
		this->pGCRetMsg->PushCell(0);

	if (pubDest)
		this->pGCRetMsg->PushStringEx(reinterpret_cast<char *>(pubDest), cubDest, SM_PARAM_STRING_BINARY | SM_PARAM_STRING_COPY, 0);
	else
		this->pGCRetMsg->PushStringEx(const_cast<char *>(""), 1, SM_PARAM_STRING_BINARY | SM_PARAM_STRING_COPY, 0);

	this->pGCRetMsg->PushCell(cubDest);

	if (pcubMsgSize)
		this->pGCRetMsg->PushCell(*pcubMsgSize);
	else
		this->pGCRetMsg->PushCell(0);

	this->pGCRetMsg->Execute(&Result);

	if (Result != k_EGCResultOK)
	{
		if (Result == -1)
		{
			Result = k_EGCResultOK;
		}

		RETURN_META_VALUE(MRES_SUPERCEDE, static_cast<EGCResults>(Result));
	}

	RETURN_META_VALUE(MRES_SUPERCEDE, res);
}
#endif

void SteamWorksGCHooks::AddHooks(ISteamGameCoordinator *pGC)
{
	if (this->uHooked == eHooked || pGC == NULL)
	{
		return;
	}

	this->uHooked = eHooked;
#if defined(STEAMWORKS_KHOOK)
	hookSendMessage.Add(pGC);
	hookIsMessageAvailable.Add(pGC);
	hookRetrieveMessage.Add(pGC);
#else
	SH_ADD_HOOK(ISteamGameCoordinator, SendMessage, pGC, SH_MEMBER(this, &SteamWorksGCHooks::SendMessage), false);
	SH_ADD_HOOK(ISteamGameCoordinator, IsMessageAvailable, pGC, SH_MEMBER(this, &SteamWorksGCHooks::IsMessageAvailable), true);
	SH_ADD_HOOK(ISteamGameCoordinator, RetrieveMessage, pGC, SH_MEMBER(this, &SteamWorksGCHooks::RetrieveMessage), false);
#endif
}

void SteamWorksGCHooks::RemoveHooks(ISteamGameCoordinator *pGC, bool destroyed)
{
	if (this->uHooked != eHooked || pGC == NULL)
	{
		return;
	}

#if defined(STEAMWORKS_KHOOK)
	hookSendMessage.Remove(pGC);
	hookIsMessageAvailable.Remove(pGC);
	hookRetrieveMessage.Remove(pGC);
#else
	SH_REMOVE_HOOK(ISteamGameCoordinator, SendMessage, pGC, SH_MEMBER(this, &SteamWorksGCHooks::SendMessage), false);
	SH_REMOVE_HOOK(ISteamGameCoordinator, IsMessageAvailable, pGC, SH_MEMBER(this, &SteamWorksGCHooks::IsMessageAvailable), true);
	SH_REMOVE_HOOK(ISteamGameCoordinator, RetrieveMessage, pGC, SH_MEMBER(this, &SteamWorksGCHooks::RetrieveMessage), false);
#endif

	if (destroyed)
	{
		this->uHooked = eUnhooked;
		return;
	}

	this->uHooked = eHooking;
	smutils->AddGameFrameHook(OurGCGameFrameHook);
}

void OurGCGameFrameHook(bool simulating) /* What we do for SDK independence. */
{
	ISteamGameCoordinator *pGC = GetSteamGCPointer();
	if (pGC == NULL)
	{
		return;
	}

	g_SteamWorks.pGCHooks->AddHooks(pGC);
	smutils->RemoveGameFrameHook(OurGCGameFrameHook);
}
