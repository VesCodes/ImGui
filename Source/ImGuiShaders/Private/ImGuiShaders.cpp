#include "ImGuiShaders.h"

#ifndef IMGUI_DISABLE

#include <Interfaces/IPluginManager.h>

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FImGuiShadersModule, ImGuiShaders)

IMPLEMENT_GLOBAL_SHADER(FImGuiVertexShader, "/Plugin/ImGui/Private/ImGui.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FImGuiPixelShader, "/Plugin/ImGui/Private/ImGui.usf", "MainPS", SF_Pixel);

void FImGuiShadersModule::StartupModule()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("ImGui"));
	if (Plugin.IsValid())
	{
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/ImGui"), FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders")));
	}
}

FImGuiVertexShader::FImGuiVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
{
	ProjectionMatrix.Bind(Initializer.ParameterMap, TEXT("ProjectionMatrix"));
}

void FImGuiVertexShader::SetProjectionMatrix(FRHIBatchedShaderParameters& BatchedParameters, const FMatrix44f& InProjectionMatrix)
{
	SetShaderValue(BatchedParameters, ProjectionMatrix, InProjectionMatrix);
}

FImGuiPixelShader::FImGuiPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer)
{
	Texture.Bind(Initializer.ParameterMap, TEXT("Texture"));
	TextureSampler.Bind(Initializer.ParameterMap, TEXT("TextureSampler"));
	bSrgb.Bind(Initializer.ParameterMap, TEXT("bSrgb"));
}

void FImGuiPixelShader::SetTexture(FRHIBatchedShaderParameters& BatchedParameters, FRHITexture* InTexture, FRHISamplerState* InSampler, bool bInSrgb)
{
	SetTextureParameter(BatchedParameters, Texture, TextureSampler, InSampler, InTexture);
	SetShaderValue(BatchedParameters, bSrgb, bInSrgb);
}

#else // #ifndef IMGUI_DISABLE

#include <Modules/ModuleManager.h>

IMPLEMENT_MODULE(FDefaultModuleImpl, ImGuiShaders);

#endif // #ifndef IMGUI_DISABLE
