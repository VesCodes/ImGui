#pragma once

#ifndef IMGUI_DISABLE

#include <Math/Color.h>
#include <Math/IntPoint.h>
#include <Math/IntVector.h>
#include <Math/Vector2D.h>
#include <Math/Vector4.h>
#include <Misc/AssertionMacros.h>
#include <Misc/EngineVersionComparison.h>

class FImGuiContext;
class UTexture;
struct FInputChord;
struct FKey;
struct FSlateBrush;
enum ImGuiKey : int;
typedef int ImGuiKeyChord;
struct ImGuiContext;
struct ImPlotContext;

#define IM_ASSERT(Expr) ensure(Expr)

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DISABLE_WIN32_FUNCTIONS
#define IMGUI_DISABLE_DEFAULT_ALLOCATORS
#define IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS
#define IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS

#ifdef IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS
typedef IFileHandle* ImFileHandle;
ImFileHandle ImFileOpen(const char* FileName, const char* Mode);
bool ImFileClose(ImFileHandle File);
uint64 ImFileGetSize(ImFileHandle File);
uint64 ImFileRead(void* Data, uint64 Size, uint64 Count, ImFileHandle File);
uint64 ImFileWrite(const void* Data, uint64 Size, uint64 Count, ImFileHandle File);
#endif

#define IM_VEC2_CLASS_EXTRA \
	operator FVector2f() const { return FVector2f(x, y); } \
	constexpr ImVec2(const FVector2f& V) : x(V.X), y(V.Y) {} \
	operator FVector2d() const { return FVector2d(x, y); } \
	constexpr ImVec2(const FVector2d& V) : x(V.X), y(V.Y) {} \
	operator FIntPoint() const { return FIntPoint(x, y); } \
	constexpr ImVec2(const FIntPoint& V) : x(V.X), y(V.Y) {} \
	operator FIntVector2() const { return FIntVector2(x, y); } \
	constexpr ImVec2(const FIntVector2& V) : x(V.X), y(V.Y) {}

#define IM_VEC4_CLASS_EXTRA \
	operator FVector4() const { return FVector4(x, y, z, w); } \
	constexpr ImVec4(const FVector4& V) : x(V.X), y(V.Y), z(V.Z), w(V.W) {} \
	operator FIntVector4() const { return FIntVector4(x, y, z, w); } \
	constexpr ImVec4(const FIntVector4& V) : x(V.X), y(V.Y), z(V.Z), w(V.W) {} \
	operator FLinearColor() const { return FLinearColor(x, y, z, w); } \
	constexpr ImVec4(const FLinearColor& C) : x(C.R), y(C.G), z(C.B), w(C.A) {}

#define IM_COL32_R_SHIFT ImGui::ImCol32RShift
#define IM_COL32_G_SHIFT ImGui::ImCol32GShift
#define IM_COL32_B_SHIFT ImGui::ImCol32BShift
#define IM_COL32_A_SHIFT ImGui::ImCol32AShift
#define IM_COL32_A_MASK ImGui::ImCol32AMask

#if WITH_ENGINE
#define ImTextureID UTexture*
#else
#define ImTextureID FSlateBrush*
#endif

namespace ImGui
{
	// Pack ImGui 32-bit colors so they're bit compatible with FColor
	inline constexpr uint32 ImCol32RShift = offsetof(FColor, R) * 8;
	inline constexpr uint32 ImCol32GShift = offsetof(FColor, G) * 8;
	inline constexpr uint32 ImCol32BShift = offsetof(FColor, B) * 8;
	inline constexpr uint32 ImCol32AShift = offsetof(FColor, A) * 8;
	inline constexpr uint32 ImCol32AMask = (0xFF << ImCol32AShift);

	/// Helper to safely scope ImGui drawing to a specific context; in most cases you should
	/// use the default constructor to switch to the current game, editor, or PIE context:
	/// @code
	///	ImGui::FScopedContext ScopedContext;
	///	if (ScopedContext)
	///	{
	///		ImGui::ShowDemoWindow();
	///	}
	/// @endcode
	struct IMGUI_API FScopedContext
	{
#if UE_VERSION_OLDER_THAN(5, 5, 0)
		UE_NODISCARD_CTOR explicit FScopedContext(const int32 PieSessionId = GPlayInEditorID);
#else
		UE_NODISCARD_CTOR explicit FScopedContext(const int32 PieSessionId = UE::GetPlayInEditorID());
#endif
		UE_NODISCARD_CTOR explicit FScopedContext(const TSharedPtr<FImGuiContext>& InContext);
		~FScopedContext();

		/// Returns true if the underlying ImGui context is ready for drawing
		explicit operator bool() const;

		/// Returns true if the managed ImGui context is valid
		bool IsValid() const;

		/// Access to the managed ImGui context
		FImGuiContext* operator->() const;

	private:
		TSharedPtr<FImGuiContext> Context = nullptr;

		ImGuiContext* PrevContext = nullptr;

#if WITH_IMPLOT
		ImPlotContext* PrevPlotContext = nullptr;
#endif
	};

	/// Converts from UE to ImGui key
	IMGUI_API ImGuiKey ConvertKey(const FKey& Key);

	/// Converts from ImGui to UE key
	IMGUI_API FKey ConvertKey(const ImGuiKey Key);

	/// Converts from UE to ImGui key chord
	IMGUI_API ImGuiKeyChord ConvertKeyChord(const FInputChord& Chord);

	/// Converts from ImGui to UE key chord
	IMGUI_API FInputChord ConvertKeyChord(const ImGuiKeyChord Chord);
}

#define IMGUI_INCLUDE_IMGUI_USER_H
#define IMGUI_USER_H_FILENAME "ImGuiConfig.inl"

#endif // #ifndef IMGUI_DISABLE
