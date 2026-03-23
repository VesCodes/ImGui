#pragma once

#ifndef IMGUI_DISABLE

namespace ImGui
{
	/// Converts ImGui 32-bit color to UE color
	IMGUI_API FORCEINLINE FColor ConvertColor(ImU32 Color)
	{
		return FColor(Color);
	}

	/// Copies ImGui array to UE array
	template<typename SrcType, typename DstType UE_REQUIRES(std::is_constructible_v<DstType, SrcType>)>
	void CopyArray(const ImVector<SrcType>& SrcArray, TArray<DstType>& DstArray)
	{
		DstArray = TArrayView<SrcType>(SrcArray.Data, SrcArray.Size);
	}
}

#endif // #ifndef IMGUI_DISABLE
