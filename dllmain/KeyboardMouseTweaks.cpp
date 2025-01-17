#include <iostream>
#include "..\includes\stdafx.h"
#include "dllmain.h"
#include "Settings.h"
#include "ConsoleWnd.h"
#include "MouseTurning.h"
#include "KeyboardMouseTweaks.h"

uintptr_t* ptrRifleMovAddr;
uintptr_t* ptrInvMovAddr;
uintptr_t* ptrFocusAnimFldAddr;
uintptr_t ptrRetryLoadDLGstate;

static uint32_t* ptrLastUsedController;

int iMinFocusTime;

LastDevice GetLastUsedDevice()
{
	// 0 = Keyboard?
	// 1 = Mouse
	// 2 = Xinput Controller
	// 3 = Dinput Controller

	switch (*(int32_t*)(ptrLastUsedController))
	{
	case 0:
		return LastDevice::Keyboard;
	case 1:
		return LastDevice::Mouse;
	case 2:
		return LastDevice::XinputController;
	case 3:
		return LastDevice::DinputController;
	default:
		return LastDevice::Keyboard;
	}
}

HKL __stdcall GetKeyboardLayout_Hook(DWORD idThread)
{
	auto userLanguage = GetKeyboardLayout(idThread);

	// Return user language if it's one that the game should have support for
	switch (PRIMARYLANGID(LOWORD(userLanguage)))
	{
	case LANG_CHINESE:
	case LANG_ENGLISH:
	case LANG_FRENCH:
	case LANG_GERMAN:
	case LANG_ITALIAN:
	case LANG_JAPANESE:
	case LANG_SPANISH:
		return userLanguage;
	}

	return (HKL)MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
}

void InventoryFlipBindings(UINT uMsg, WPARAM wParam)
{
	switch (uMsg) {
	case WM_KEYDOWN:
		if (wParam == cfg.KeyMap(cfg.sFlipItemLeft.data(), true) || wParam == cfg.KeyMap(cfg.sFlipItemRight.data(), true))
			bShouldFlipX = true;
		else if (wParam == cfg.KeyMap(cfg.sFlipItemUp.data(), true) || wParam == cfg.KeyMap(cfg.sFlipItemDown.data(), true))
			bShouldFlipY = true;
		break;

	case WM_KEYUP:
		if (wParam == cfg.KeyMap(cfg.sFlipItemLeft.data(), true) || wParam == cfg.KeyMap(cfg.sFlipItemRight.data(), true))
			bShouldFlipX = false;
		else if (wParam == cfg.KeyMap(cfg.sFlipItemUp.data(), true) || wParam == cfg.KeyMap(cfg.sFlipItemDown.data(), true))
			bShouldFlipY = false;
		break;
	}
}

void Init_KeyboardMouseTweaks()
{
	auto pattern = hook::pattern("A1 ? ? ? ? 85 C0 74 ? 83 F8 ? 74 ? 81 F9");
	ptrLastUsedController = *pattern.count(1).get(0).get<uint32_t*>(1);

	Init_MouseTurning();

	// Inventory item flip binding
	pattern = hook::pattern("A1 ? ? ? ? 75 ? A8 ? 74 ? 6A ? 8B CE E8 ? ? ? ? BB");
	ptrInvMovAddr = *pattern.count(1).get(0).get<uint32_t*>(1);
	struct InvFlip
	{
		void operator()(injector::reg_pack& regs)
		{
			regs.eax = *(int32_t*)ptrInvMovAddr;

			if (bShouldFlipX)
			{
				regs.eax = 0x00300000;
				bShouldFlipX = false;
			}

			if (bShouldFlipY)
			{
				regs.eax = 0x00C00000;
				bShouldFlipY = false;
			}
		}
	}; injector::MakeInline<InvFlip>(pattern.count(1).get(0).get<uint32_t>(0), pattern.count(1).get(0).get<uint32_t>(5));

	// Prevent the game from overriding your selection in the "Retry/Load" screen when moving the mouse before confirming an action.
	{
		// Get pointer for the state. Only reliable way I found to achieve this.
		pattern = hook::pattern("C7 06 ? ? ? ? A1 ? ? ? ? F7 80 ? ? ? ? ? ? ? ? 74"); //0x48C1C0
		struct RLDLGState
		{
			void operator()(injector::reg_pack& regs)
			{
				*(int*)(regs.esi) = 0;
				ptrRetryLoadDLGstate = regs.esi + 0x3;
			}
		}; injector::MakeInline<RLDLGState>(pattern.count(1).get(0).get<uint32_t>(0), pattern.count(1).get(0).get<uint32_t>(6));

		// Check if the "yes/no" prompt is open before sending any index updates
		pattern = hook::pattern("89 8F ? ? ? ? FF 85 ? ? ? ? 83 C6 ? 3B 77");
		struct MouseMenuSelector
		{
			void operator()(injector::reg_pack& regs)
			{
				if (cfg.bFixRetryLoadMouseSelector)
				{
					if (*(int32_t*)ptrRetryLoadDLGstate != 1)
					{
						*(int32_t*)(regs.edi + 0x160) = regs.ecx;
					}
				}
				else
				{
					*(int32_t*)(regs.edi + 0x160) = regs.ecx;
				}
			}
		}; injector::MakeInline<MouseMenuSelector>(pattern.count(1).get(0).get<uint32_t>(0), pattern.count(1).get(0).get<uint32_t>(6));
	}

	// Fix camera after zooming with the sniper
	{
		auto pattern = hook::pattern("A2 ? ? ? ? A2 ? ? ? ? EB ? 81 FB ? ? ? ? 75 ? 83");
		ptrRifleMovAddr = *pattern.count(1).get(0).get<uint32_t*>(1);
		struct FixSniperZoom
		{
			void operator()(injector::reg_pack& regs)
			{
				if (!cfg.bFixSniperZoom)
					*(int32_t*)ptrRifleMovAddr = regs.eax;
			}
		}; injector::MakeInline<FixSniperZoom>(pattern.count(1).get(0).get<uint32_t>(0), pattern.count(1).get(0).get<uint32_t>(5));
	}

	// Fix the "focus animation" not looking as strong as when triggered with a controller
	if (cfg.bFixSniperFocus)
	{
		pattern = hook::pattern("8B F1 8B 4D ? 57 85 C9 74 ? D9 56 ? 88 56");
		struct FixScopeZoomFocus
		{
			void operator()(injector::reg_pack& regs)
			{
				regs.esi = regs.ecx;
				regs.ecx = *(int32_t*)(regs.ebp + 0x8);

				// This makes it so the focus animation has to play for at least X ammount of time
				if ((GetLastUsedDevice() == LastDevice::Keyboard) || (GetLastUsedDevice() == LastDevice::Mouse))
				{
					if (regs.ecx == 1)
					{
						iMinFocusTime = 5;
					}
					else if (regs.ecx == 0)
					{
						iMinFocusTime--;
					}

					if (iMinFocusTime > 0)
						regs.ecx = 1;
					else
						regs.ecx = 0;
				}
			}
		}; injector::MakeInline<FixScopeZoomFocus>(pattern.count(1).get(0).get<uint32_t>(0), pattern.count(1).get(0).get<uint32_t>(5));
	
		// This jl instruction makes the focus animation stop almost immediately when using the mouse. Noping it doesn't seem to affect the controller at all.
		pattern = hook::pattern("7C ? C6 06 ? EB ? C7 46 ? ? ? ? ? EB ? DD D8 83 3D");
		injector::MakeNOP(pattern.count(1).get(0).get<uint32_t>(0), 2, true);
	}

	if (cfg.bFallbackToEnglishKeyIcons)
	{
		// Replace GetKeyboardLayout IAT to point to our GetKeyboardLayout_Hook
		pattern = hook::pattern("68 FF 00 00 00 8D 85 ? ? ? ? 6A 00 50 C6 85 ? ? ? ? 00 E8 ? ? ? ? 8B 3D ? ? ? ?");
		injector::WriteMemory(*pattern.count(1).get(0).get<uintptr_t>(0x1C), GetKeyboardLayout_Hook, true);
	}
}