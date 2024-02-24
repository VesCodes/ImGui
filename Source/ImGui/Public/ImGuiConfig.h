#pragma once

#include <Math/Color.h>
#include <Math/IntPoint.h>
#include <Math/IntVector.h>
#include <Math/Vector2D.h>
#include <Math/Vector4.h>
#include <Misc/AssertionMacros.h>

#define IM_ASSERT(Expr) ensure(Expr)

#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DISABLE_WIN32_FUNCTIONS
#define IMGUI_DISABLE_DEFAULT_ALLOCATORS
#define IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS

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
	constexpr ImVec2(const FIntPoint& V) : x(V.X), y(V.Y) {}

#define IM_VEC4_CLASS_EXTRA \
	operator FVector4() const { return FVector4(x, y, z, w); } \
	constexpr ImVec4(const FVector4& V) : x(V.X), y(V.Y), z(V.Z), w(V.W) {} \
	operator FIntVector4() const { return FIntVector4(x, y, z, w); } \
	constexpr ImVec4(const FIntVector4& V) : x(V.X), y(V.Y), z(V.Z), w(V.W) {} \
	operator FLinearColor() const { return FLinearColor(x, y, z, w); } \
	constexpr ImVec4(const FLinearColor& C) : x(C.R), y(C.G), z(C.B), w(C.A) {}

#if WITH_ENGINE
#define ImTextureID class UTexture2D*
#else
#define ImTextureID struct FSlateBrush*
#endif

class FImGuiContext;
struct ImGuiContext;
struct ImPlotContext;
enum ImGuiKey : int;
struct FKey;

namespace ImGui
{
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
		UE_NODISCARD_CTOR explicit FScopedContext(const int32 PIEInstance = GPlayInEditorID);
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
		ImPlotContext* PrevPlotContext = nullptr;
	};

	/// Converts between Unreal and ImGui key types
	IMGUI_API ImGuiKey ConvertKey(const FKey& Key);

	/// Converts between Unreal and ImGui 32-bit color types
	IMGUI_API FColor ConvertColor(uint32 Color);
}
