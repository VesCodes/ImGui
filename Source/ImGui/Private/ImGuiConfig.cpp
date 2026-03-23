#include "ImGuiConfig.h"

#ifndef IMGUI_DISABLE

#include <InputCoreTypes.h>
#include <Framework/Commands/InputChord.h>
#include <HAL/PlatformFileManager.h>

#if WITH_ENGINE
#include <Engine/Texture2D.h>
#endif

// ReSharper disable CppUnusedIncludeDirective
THIRD_PARTY_INCLUDES_START
#include <imgui.cpp>
#include <imgui_demo.cpp>
#include <imgui_draw.cpp>
#include <imgui_tables.cpp>
#include <imgui_widgets.cpp>
#if WITH_IMPLOT
#include <implot.cpp>
MSVC_PRAGMA(warning(push))
MSVC_PRAGMA(warning(disable: 4756)) // overflow in constant arithmetic
#include <implot_demo.cpp>
MSVC_PRAGMA(warning(pop))
#include <implot_items.cpp>
#endif
THIRD_PARTY_INCLUDES_END
// ReSharper restore CppUnusedIncludeDirective

#include "ImGuiContext.h"
#include "ImGuiModule.h"

#ifdef IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS
ImFileHandle ImFileOpen(const char* FileName, const char* Mode)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	bool bRead = false;
	bool bWrite = false;
	bool bAppend = false;
	bool bExtended = false;

	for (; *Mode; ++Mode)
	{
		if (*Mode == 'r')
		{
			bRead = true;
		}
		else if (*Mode == 'w')
		{
			bWrite = true;
		}
		else if (*Mode == 'a')
		{
			bAppend = true;
		}
		else if (*Mode == '+')
		{
			bExtended = true;
		}
	}

	if (bWrite || bAppend || bExtended)
	{
		return PlatformFile.OpenWrite(UTF8_TO_TCHAR(FileName), bAppend, bExtended);
	}

	if (bRead)
	{
		return PlatformFile.OpenRead(UTF8_TO_TCHAR(FileName), true);
	}

	return nullptr;
}

bool ImFileClose(ImFileHandle File)
{
	if (!File)
	{
		return false;
	}

	delete File;
	return true;
}

uint64 ImFileGetSize(ImFileHandle File)
{
	if (!File)
	{
		return MAX_uint64;
	}

	const uint64 FileSize = File->Size();
	return FileSize;
}

uint64 ImFileRead(void* Data, uint64 Size, uint64 Count, ImFileHandle File)
{
	if (!File)
	{
		return 0;
	}

	const int64 StartPos = File->Tell();
	File->Read(static_cast<uint8*>(Data), Size * Count);

	const uint64 ReadSize = File->Tell() - StartPos;
	return ReadSize;
}

uint64 ImFileWrite(const void* Data, uint64 Size, uint64 Count, ImFileHandle File)
{
	if (!File)
	{
		return 0;
	}

	const int64 StartPos = File->Tell();
	File->Write(static_cast<const uint8*>(Data), Size * Count);

	const uint64 WriteSize = File->Tell() - StartPos;
	return WriteSize;
}
#endif

ImGui::FScopedContext::FScopedContext(const int32 PieSessionId)
	: FScopedContext(FImGuiModule::Get().FindOrCreateSessionContext(PieSessionId))
{
}

ImGui::FScopedContext::FScopedContext(const TSharedPtr<FImGuiContext>& InContext)
	: Context(InContext)
{
	PrevContext = GetCurrentContext();
#if WITH_IMPLOT
	PrevPlotContext = ImPlot::GetCurrentContext();
#endif

	if (Context.IsValid())
	{
		SetCurrentContext(*Context);
#if WITH_IMPLOT
		ImPlot::SetCurrentContext(*Context);
#endif
	}
	else
	{
		SetCurrentContext(nullptr);
#if WITH_IMPLOT
		ImPlot::SetCurrentContext(nullptr);
#endif
	}
}

ImGui::FScopedContext::~FScopedContext()
{
	SetCurrentContext(PrevContext);
#if WITH_IMPLOT
	ImPlot::SetCurrentContext(PrevPlotContext);
#endif
}

ImGui::FScopedContext::operator bool() const
{
	const ImGuiContext* CurrContext = GetCurrentContext();
	return CurrContext && CurrContext->Initialized && CurrContext->WithinFrameScope;
}

bool ImGui::FScopedContext::IsValid() const
{
	return Context.IsValid();
}

FImGuiContext* ImGui::FScopedContext::operator->() const
{
	return Context.operator->();
}

ImGuiKey ImGui::ConvertKey(const FKey& Key)
{
	static const TMap<FKey, ImGuiKey> KeyLookupMap = {
		{ EKeys::Tab, ImGuiKey_Tab },

		{ EKeys::Left, ImGuiKey_LeftArrow },
		{ EKeys::Right, ImGuiKey_RightArrow },
		{ EKeys::Up, ImGuiKey_UpArrow },
		{ EKeys::Down, ImGuiKey_DownArrow },

		{ EKeys::PageUp, ImGuiKey_PageUp },
		{ EKeys::PageDown, ImGuiKey_PageDown },
		{ EKeys::Home, ImGuiKey_Home },
		{ EKeys::End, ImGuiKey_End },
		{ EKeys::Insert, ImGuiKey_Insert },
		{ EKeys::Delete, ImGuiKey_Delete },

		{ EKeys::BackSpace, ImGuiKey_Backspace },
		{ EKeys::SpaceBar, ImGuiKey_Space },
		{ EKeys::Enter, ImGuiKey_Enter },
		{ EKeys::Escape, ImGuiKey_Escape },

		{ EKeys::LeftControl, ImGuiKey_LeftCtrl },
		{ EKeys::LeftShift, ImGuiKey_LeftShift },
		{ EKeys::LeftAlt, ImGuiKey_LeftAlt },
		{ EKeys::LeftCommand, ImGuiKey_LeftSuper },
		{ EKeys::RightControl, ImGuiKey_RightCtrl },
		{ EKeys::RightShift, ImGuiKey_RightShift },
		{ EKeys::RightAlt, ImGuiKey_RightAlt },
		{ EKeys::RightCommand, ImGuiKey_RightSuper },

		{ EKeys::Zero, ImGuiKey_0 },
		{ EKeys::One, ImGuiKey_1 },
		{ EKeys::Two, ImGuiKey_2 },
		{ EKeys::Three, ImGuiKey_3 },
		{ EKeys::Four, ImGuiKey_4 },
		{ EKeys::Five, ImGuiKey_5 },
		{ EKeys::Six, ImGuiKey_6 },
		{ EKeys::Seven, ImGuiKey_7 },
		{ EKeys::Eight, ImGuiKey_8 },
		{ EKeys::Nine, ImGuiKey_9 },

		{ EKeys::A, ImGuiKey_A },
		{ EKeys::B, ImGuiKey_B },
		{ EKeys::C, ImGuiKey_C },
		{ EKeys::D, ImGuiKey_D },
		{ EKeys::E, ImGuiKey_E },
		{ EKeys::F, ImGuiKey_F },
		{ EKeys::G, ImGuiKey_G },
		{ EKeys::H, ImGuiKey_H },
		{ EKeys::I, ImGuiKey_I },
		{ EKeys::J, ImGuiKey_J },
		{ EKeys::K, ImGuiKey_K },
		{ EKeys::L, ImGuiKey_L },
		{ EKeys::M, ImGuiKey_M },
		{ EKeys::N, ImGuiKey_N },
		{ EKeys::O, ImGuiKey_O },
		{ EKeys::P, ImGuiKey_P },
		{ EKeys::Q, ImGuiKey_Q },
		{ EKeys::R, ImGuiKey_R },
		{ EKeys::S, ImGuiKey_S },
		{ EKeys::T, ImGuiKey_T },
		{ EKeys::U, ImGuiKey_U },
		{ EKeys::V, ImGuiKey_V },
		{ EKeys::W, ImGuiKey_W },
		{ EKeys::X, ImGuiKey_X },
		{ EKeys::Y, ImGuiKey_Y },
		{ EKeys::Z, ImGuiKey_Z },

		{ EKeys::F1, ImGuiKey_F1 },
		{ EKeys::F2, ImGuiKey_F2 },
		{ EKeys::F3, ImGuiKey_F3 },
		{ EKeys::F4, ImGuiKey_F4 },
		{ EKeys::F5, ImGuiKey_F5 },
		{ EKeys::F6, ImGuiKey_F6 },
		{ EKeys::F7, ImGuiKey_F7 },
		{ EKeys::F8, ImGuiKey_F8 },
		{ EKeys::F9, ImGuiKey_F9 },
		{ EKeys::F10, ImGuiKey_F10 },
		{ EKeys::F11, ImGuiKey_F11 },
		{ EKeys::F12, ImGuiKey_F12 },

		{ EKeys::Apostrophe, ImGuiKey_Apostrophe },
		{ EKeys::Comma, ImGuiKey_Comma },
		{ EKeys::Hyphen, ImGuiKey_Minus },
		{ EKeys::Period, ImGuiKey_Period },
		{ EKeys::Slash, ImGuiKey_Slash },
		{ EKeys::Semicolon, ImGuiKey_Semicolon },
		{ EKeys::Equals, ImGuiKey_Equal },
		{ EKeys::LeftBracket, ImGuiKey_LeftBracket },
		{ EKeys::Backslash, ImGuiKey_Backslash },
		{ EKeys::RightBracket, ImGuiKey_RightBracket },
		{ EKeys::Tilde, ImGuiKey_GraveAccent },

		{ EKeys::CapsLock, ImGuiKey_CapsLock },
		{ EKeys::ScrollLock, ImGuiKey_ScrollLock },
		{ EKeys::NumLock, ImGuiKey_NumLock },
		// No mappable key for ImGuiKey_PrintScreen
		{ EKeys::Pause, ImGuiKey_Pause },

		{ EKeys::NumPadZero, ImGuiKey_Keypad0 },
		{ EKeys::NumPadOne, ImGuiKey_Keypad1 },
		{ EKeys::NumPadTwo, ImGuiKey_Keypad2 },
		{ EKeys::NumPadThree, ImGuiKey_Keypad3 },
		{ EKeys::NumPadFour, ImGuiKey_Keypad4 },
		{ EKeys::NumPadFive, ImGuiKey_Keypad5 },
		{ EKeys::NumPadSix, ImGuiKey_Keypad6 },
		{ EKeys::NumPadSeven, ImGuiKey_Keypad7 },
		{ EKeys::NumPadEight, ImGuiKey_Keypad8 },
		{ EKeys::NumPadNine, ImGuiKey_Keypad9 },

		{ EKeys::Decimal, ImGuiKey_KeypadDecimal },
		{ EKeys::Divide, ImGuiKey_KeypadDivide },
		{ EKeys::Multiply, ImGuiKey_KeypadMultiply },
		{ EKeys::Subtract, ImGuiKey_KeypadSubtract },
		{ EKeys::Add, ImGuiKey_KeypadAdd },
		// No mappable key for ImGuiKey_KeypadEnter
		// No mappable key for ImGuiKey_KeypadEqual

		{ EKeys::Gamepad_Special_Right, ImGuiKey_GamepadStart },
		{ EKeys::Gamepad_Special_Left, ImGuiKey_GamepadBack },
		{ EKeys::Gamepad_FaceButton_Left, ImGuiKey_GamepadFaceLeft },
		{ EKeys::Gamepad_FaceButton_Right, ImGuiKey_GamepadFaceRight },
		{ EKeys::Gamepad_FaceButton_Top, ImGuiKey_GamepadFaceUp },
		{ EKeys::Gamepad_FaceButton_Bottom, ImGuiKey_GamepadFaceDown },
		{ EKeys::Gamepad_DPad_Left, ImGuiKey_GamepadDpadLeft },
		{ EKeys::Gamepad_DPad_Right, ImGuiKey_GamepadDpadRight },
		{ EKeys::Gamepad_DPad_Up, ImGuiKey_GamepadDpadUp },
		{ EKeys::Gamepad_DPad_Down, ImGuiKey_GamepadDpadDown },
		{ EKeys::Gamepad_LeftShoulder, ImGuiKey_GamepadL1 },
		{ EKeys::Gamepad_RightShoulder, ImGuiKey_GamepadR1 },
		{ EKeys::Gamepad_LeftTrigger, ImGuiKey_GamepadL2 },
		{ EKeys::Gamepad_RightTrigger, ImGuiKey_GamepadR2 },
		{ EKeys::Gamepad_LeftThumbstick, ImGuiKey_GamepadL3 },
		{ EKeys::Gamepad_RightThumbstick, ImGuiKey_GamepadR3 },
		{ EKeys::Gamepad_LeftStick_Left, ImGuiKey_GamepadLStickLeft },
		{ EKeys::Gamepad_LeftStick_Right, ImGuiKey_GamepadLStickRight },
		{ EKeys::Gamepad_LeftStick_Up, ImGuiKey_GamepadLStickUp },
		{ EKeys::Gamepad_LeftStick_Down, ImGuiKey_GamepadLStickDown },
		{ EKeys::Gamepad_RightStick_Left, ImGuiKey_GamepadRStickLeft },
		{ EKeys::Gamepad_RightStick_Right, ImGuiKey_GamepadRStickRight },
		{ EKeys::Gamepad_RightStick_Up, ImGuiKey_GamepadRStickUp },
		{ EKeys::Gamepad_RightStick_Down, ImGuiKey_GamepadRStickDown }
	};

	return KeyLookupMap.FindRef(Key, ImGuiKey_None);
}

FKey ImGui::ConvertKey(const ImGuiKey Key)
{
	static const TMap<ImGuiKey, FKey> KeyLookupMap = {
		{ ImGuiKey_Tab, EKeys::Tab },

		{ ImGuiKey_LeftArrow, EKeys::Left },
		{ ImGuiKey_RightArrow, EKeys::Right },
		{ ImGuiKey_UpArrow, EKeys::Up },
		{ ImGuiKey_DownArrow, EKeys::Down },

		{ ImGuiKey_PageUp, EKeys::PageUp },
		{ ImGuiKey_PageDown, EKeys::PageDown },
		{ ImGuiKey_Home, EKeys::Home },
		{ ImGuiKey_End, EKeys::End },
		{ ImGuiKey_Insert, EKeys::Insert },
		{ ImGuiKey_Delete, EKeys::Delete },

		{ ImGuiKey_Backspace, EKeys::BackSpace },
		{ ImGuiKey_Space, EKeys::SpaceBar },
		{ ImGuiKey_Enter, EKeys::Enter },
		{ ImGuiKey_Escape, EKeys::Escape },

		{ ImGuiKey_LeftCtrl, EKeys::LeftControl },
		{ ImGuiKey_LeftShift, EKeys::LeftShift },
		{ ImGuiKey_LeftAlt, EKeys::LeftAlt },
		{ ImGuiKey_LeftSuper, EKeys::LeftCommand },
		{ ImGuiKey_RightCtrl, EKeys::RightControl },
		{ ImGuiKey_RightShift, EKeys::RightShift },
		{ ImGuiKey_RightAlt, EKeys::RightAlt },
		{ ImGuiKey_RightSuper, EKeys::RightCommand },

		{ ImGuiKey_0, EKeys::Zero },
		{ ImGuiKey_1, EKeys::One },
		{ ImGuiKey_2, EKeys::Two },
		{ ImGuiKey_3, EKeys::Three },
		{ ImGuiKey_4, EKeys::Four },
		{ ImGuiKey_5, EKeys::Five },
		{ ImGuiKey_6, EKeys::Six },
		{ ImGuiKey_7, EKeys::Seven },
		{ ImGuiKey_8, EKeys::Eight },
		{ ImGuiKey_9, EKeys::Nine },

		{ ImGuiKey_A, EKeys::A },
		{ ImGuiKey_B, EKeys::B },
		{ ImGuiKey_C, EKeys::C },
		{ ImGuiKey_D, EKeys::D },
		{ ImGuiKey_E, EKeys::E },
		{ ImGuiKey_F, EKeys::F },
		{ ImGuiKey_G, EKeys::G },
		{ ImGuiKey_H, EKeys::H },
		{ ImGuiKey_I, EKeys::I },
		{ ImGuiKey_J, EKeys::J },
		{ ImGuiKey_K, EKeys::K },
		{ ImGuiKey_L, EKeys::L },
		{ ImGuiKey_M, EKeys::M },
		{ ImGuiKey_N, EKeys::N },
		{ ImGuiKey_O, EKeys::O },
		{ ImGuiKey_P, EKeys::P },
		{ ImGuiKey_Q, EKeys::Q },
		{ ImGuiKey_R, EKeys::R },
		{ ImGuiKey_S, EKeys::S },
		{ ImGuiKey_T, EKeys::T },
		{ ImGuiKey_U, EKeys::U },
		{ ImGuiKey_V, EKeys::V },
		{ ImGuiKey_W, EKeys::W },
		{ ImGuiKey_X, EKeys::X },
		{ ImGuiKey_Y, EKeys::Y },
		{ ImGuiKey_Z, EKeys::Z },

		{ ImGuiKey_F1, EKeys::F1 },
		{ ImGuiKey_F2, EKeys::F2 },
		{ ImGuiKey_F3, EKeys::F3 },
		{ ImGuiKey_F4, EKeys::F4 },
		{ ImGuiKey_F5, EKeys::F5 },
		{ ImGuiKey_F6, EKeys::F6 },
		{ ImGuiKey_F7, EKeys::F7 },
		{ ImGuiKey_F8, EKeys::F8 },
		{ ImGuiKey_F9, EKeys::F9 },
		{ ImGuiKey_F10, EKeys::F10 },
		{ ImGuiKey_F11, EKeys::F11 },
		{ ImGuiKey_F12, EKeys::F12 },

		{ ImGuiKey_Apostrophe, EKeys::Apostrophe },
		{ ImGuiKey_Comma, EKeys::Comma },
		{ ImGuiKey_Minus, EKeys::Hyphen },
		{ ImGuiKey_Period, EKeys::Period },
		{ ImGuiKey_Slash, EKeys::Slash },
		{ ImGuiKey_Semicolon, EKeys::Semicolon },
		{ ImGuiKey_Equal, EKeys::Equals },
		{ ImGuiKey_LeftBracket, EKeys::LeftBracket },
		{ ImGuiKey_Backslash, EKeys::Backslash },
		{ ImGuiKey_RightBracket, EKeys::RightBracket },
		{ ImGuiKey_GraveAccent, EKeys::Tilde },

		{ ImGuiKey_CapsLock, EKeys::CapsLock },
		{ ImGuiKey_ScrollLock, EKeys::ScrollLock },
		{ ImGuiKey_NumLock, EKeys::NumLock },
		// No mappable key for ImGuiKey_PrintScreen
		{ ImGuiKey_Pause, EKeys::Pause },

		{ ImGuiKey_Keypad0, EKeys::NumPadZero },
		{ ImGuiKey_Keypad1, EKeys::NumPadOne },
		{ ImGuiKey_Keypad2, EKeys::NumPadTwo },
		{ ImGuiKey_Keypad3, EKeys::NumPadThree },
		{ ImGuiKey_Keypad4, EKeys::NumPadFour },
		{ ImGuiKey_Keypad5, EKeys::NumPadFive },
		{ ImGuiKey_Keypad6, EKeys::NumPadSix },
		{ ImGuiKey_Keypad7, EKeys::NumPadSeven },
		{ ImGuiKey_Keypad8, EKeys::NumPadEight },
		{ ImGuiKey_Keypad9, EKeys::NumPadNine },

		{ ImGuiKey_KeypadDecimal, EKeys::Decimal },
		{ ImGuiKey_KeypadDivide, EKeys::Divide },
		{ ImGuiKey_KeypadMultiply, EKeys::Multiply },
		{ ImGuiKey_KeypadSubtract, EKeys::Subtract },
		{ ImGuiKey_KeypadAdd, EKeys::Add },
		// No mappable key for ImGuiKey_KeypadEnter
		// No mappable key for ImGuiKey_KeypadEqual

		{ ImGuiKey_GamepadStart, EKeys::Gamepad_Special_Right },
		{ ImGuiKey_GamepadBack, EKeys::Gamepad_Special_Left },
		{ ImGuiKey_GamepadFaceLeft, EKeys::Gamepad_FaceButton_Left },
		{ ImGuiKey_GamepadFaceRight, EKeys::Gamepad_FaceButton_Right },
		{ ImGuiKey_GamepadFaceUp, EKeys::Gamepad_FaceButton_Top },
		{ ImGuiKey_GamepadFaceDown, EKeys::Gamepad_FaceButton_Bottom },
		{ ImGuiKey_GamepadDpadLeft, EKeys::Gamepad_DPad_Left },
		{ ImGuiKey_GamepadDpadRight, EKeys::Gamepad_DPad_Right },
		{ ImGuiKey_GamepadDpadUp, EKeys::Gamepad_DPad_Up },
		{ ImGuiKey_GamepadDpadDown, EKeys::Gamepad_DPad_Down },
		{ ImGuiKey_GamepadL1, EKeys::Gamepad_LeftShoulder },
		{ ImGuiKey_GamepadR1, EKeys::Gamepad_RightShoulder },
		{ ImGuiKey_GamepadL2, EKeys::Gamepad_LeftTrigger },
		{ ImGuiKey_GamepadR2, EKeys::Gamepad_RightTrigger },
		{ ImGuiKey_GamepadL3, EKeys::Gamepad_LeftThumbstick },
		{ ImGuiKey_GamepadR3, EKeys::Gamepad_RightThumbstick },
		{ ImGuiKey_GamepadLStickLeft, EKeys::Gamepad_LeftStick_Left },
		{ ImGuiKey_GamepadLStickRight, EKeys::Gamepad_LeftStick_Right },
		{ ImGuiKey_GamepadLStickUp, EKeys::Gamepad_LeftStick_Up },
		{ ImGuiKey_GamepadLStickDown, EKeys::Gamepad_LeftStick_Down },
		{ ImGuiKey_GamepadRStickLeft, EKeys::Gamepad_RightStick_Left },
		{ ImGuiKey_GamepadRStickRight, EKeys::Gamepad_RightStick_Right },
		{ ImGuiKey_GamepadRStickUp, EKeys::Gamepad_RightStick_Up },
		{ ImGuiKey_GamepadRStickDown, EKeys::Gamepad_RightStick_Down }
	};

	return KeyLookupMap.FindRef(Key, EKeys::Invalid);
}

ImGuiKeyChord ImGui::ConvertKeyChord(const FInputChord& Chord)
{
	ImGuiKeyChord Result = ConvertKey(Chord.Key);

	Result |= Chord.bCtrl ? ImGuiMod_Ctrl : 0;
	Result |= Chord.bShift ? ImGuiMod_Shift : 0;
	Result |= Chord.bAlt ? ImGuiMod_Alt : 0;
	Result |= Chord.bCmd ? ImGuiMod_Super : 0;

	return Result;
}

FInputChord ImGui::ConvertKeyChord(const ImGuiKeyChord Chord)
{
	FInputChord Result = ConvertKey(static_cast<ImGuiKey>(Chord & ~ImGuiMod_Mask_));

	Result.bCtrl = (Chord & ImGuiMod_Ctrl) != 0;
	Result.bShift = (Chord & ImGuiMod_Shift) != 0;
	Result.bAlt = (Chord & ImGuiMod_Alt) != 0;
	Result.bCmd = (Chord & ImGuiMod_Super) != 0;

	return Result;
}

#endif // #ifndef IMGUI_DISABLE
