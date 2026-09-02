#pragma once

#ifndef IMGUI_DISABLE

#include <Math/Vector2D.h>

THIRD_PARTY_INCLUDES_START
#include <imgui.h>
THIRD_PARTY_INCLUDES_END

struct FImGuiDrawList
{
	void Update(ImDrawList* Source);

	ImVector<ImDrawVert> VtxBuffer;
	ImVector<ImDrawIdx> IdxBuffer;
	ImVector<ImDrawCmd> CmdBuffer;
	ImDrawListFlags Flags = ImDrawListFlags_None;
};

struct FImGuiDrawData
{
	void Update(ImDrawData* Source);

	bool bValid = false;

	int32 TotalIdxCount = 0;
	int32 TotalVtxCount = 0;

	TArray<FImGuiDrawList> DrawLists;

	FVector2f DisplayPos = FVector2f::ZeroVector;
	FVector2f DisplaySize = FVector2f::ZeroVector;
	FVector2f FrameBufferScale = FVector2f::ZeroVector;
};

#endif // #ifndef IMGUI_DISABLE
