#include "ImGuiConfig.h"

#include <InputCoreTypes.h>
#include <Engine/Texture2D.h>

THIRD_PARTY_INCLUDES_START
#include <imgui.cpp>
#include <imgui_demo.cpp>
#include <imgui_draw.cpp>
#include <imgui_tables.cpp>
#include <imgui_widgets.cpp>
#include <implot.cpp>
#include <implot_demo.cpp>
#include <implot_items.cpp>
THIRD_PARTY_INCLUDES_END

#include "ImGuiModule.h"

ImGui::FScopedContextSwitcher::FScopedContextSwitcher(const int32 PieInstance)
	: FScopedContextSwitcher(FImGuiModule::Get().GetContext(PieInstance))
{
}

ImGui::FScopedContextSwitcher::FScopedContextSwitcher(ImGuiContext* Context)
{
	PrevContext = GetCurrentContext();
	SetCurrentContext(Context);
}

ImGui::FScopedContextSwitcher::~FScopedContextSwitcher()
{
	SetCurrentContext(PrevContext);
}

ImGui::FScopedContextSwitcher::operator bool() const
{
	const ImGuiContext* Context = GetCurrentContext();
	return Context && Context->Initialized;
}

ImGuiKey ImGui::ConvertKey(const FKey& Key)
{
	static const TMap<FKey, ImGuiKey> LookupMap = {
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
		{ EKeys::Period, ImGuiKey_Period },
		{ EKeys::Slash, ImGuiKey_Slash },
		{ EKeys::Semicolon, ImGuiKey_Semicolon },
		{ EKeys::LeftBracket, ImGuiKey_LeftBracket },
		{ EKeys::Backslash, ImGuiKey_Backslash },
		{ EKeys::RightBracket, ImGuiKey_RightBracket },

		{ EKeys::CapsLock, ImGuiKey_CapsLock },
		{ EKeys::ScrollLock, ImGuiKey_ScrollLock },
		{ EKeys::NumLock, ImGuiKey_NumLock },
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
		{ EKeys::Equals, ImGuiKey_KeypadEqual },

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

	const ImGuiKey* Result = LookupMap.Find(Key);
	return (Result != nullptr) ? *Result : ImGuiKey_None;
}

FColor ImGui::ConvertColor(const uint32 Color)
{
	return FColor(
		(Color >> IM_COL32_R_SHIFT) & 0xFF,
		(Color >> IM_COL32_G_SHIFT) & 0xFF,
		(Color >> IM_COL32_B_SHIFT) & 0xFF,
		(Color >> IM_COL32_A_SHIFT) & 0xFF
	);
}
