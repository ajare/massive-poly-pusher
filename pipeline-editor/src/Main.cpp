#if defined(__SANITIZE_ADDRESS__)
// Redirect MemCheck's ASan reports to a log file instead of stderr, which is
// otherwise the only place they go and is easy to lose — PipelineEditor is a
// WIN32 GUI app with no visible console.
extern "C" const char* __asan_default_options()
{
	return "log_path=PipelineEditor.asan";
}
#endif

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iterator>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <Windows.h>
#include <SDL3/SDL.h>
#include <renderdoc/renderdoc_app.h>
#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "fontawesome/IconsFontAwesome5.h"
#include "ProcessFlowModel.h"
#include "ProcessFlowLayout.h"
#include "ProcessFlowView.h"
#include "mpp/data/StructuredData.h"
#include "mpp/BufferRenderer.h"
#include "mpp/Camera.h"
#include "mpp/Colour.h"
#include "mpp/Logger.h"
#include "mpp/PbrMaterial.h"
#include "mpp/PbrMaterialTests.h"
#include "mpp/ParticleSystemTests.h"
#include "mpp/RenderGraphGpuTests.h"
#include "mpp/RenderGraphTests.h"
#include "mpp/RenderSystem.h"
#include "mpp/RenderGraphStream.h"
#include "mpp/RenderGraphBuiltInPasses.h"
#include "mpp/RenderGraphPassFactoryRegistry.h"
#include "mpp/RenderGraphTargets.h"
#include "mpp/RenderPipeline.h"
#include "mpp/RenderTexture.h"
#include "mpp/ResourceManager.h"
#include "mpp/Scene.h"
#include "mpp/SceneRuntime.h"
#include "mpp/app/BackgroundWork.h"
#include "mpp/app/CommandStack.h"
#include "mpp/app/DocumentFile.h"
#include "mpp/app/FileDialog.h"
#include "mpp/app/ImGuiBackendData.h"
#include "mpp/app/ImageLoader.h"
#include "mpp/app/ImGuiDataProvider.h"
#include "mpp/app/ImGuiPlatform.h"
#include "mpp/app/PackageManifest.h"
#include "mpp/app/RenderSystemConfig.h"
#include "mpp/app/InputManagerSDL.h"
#include "mpp/app/TimerSDL.h"
#include "mpp/app/WindowSDL.h"
#include "mpp/app/ZipArchive.h"
#include "mpp/resource-parsers/LegacyPipelineConversion.h"
#include "mpp/resource-parsers/LegacyPipelineResourceValidator.h"
#include "mpp/resource-parsers/LegacyPipelineSerializer.h"
#include "mpp/resource-parsers/PbrPipelineDocumentLoader.h"
#include "mpp/resource-parsers/PbrPipelineSerializer.h"
#include "mpp/resource-parsers/PbrPipelineRuntime.h"
#include "mpp/resource-parsers/GltfPbrMaterialLoader.h"
#include "mpp/resource-parsers/PbrPipelineResourceValidator.h"
#include "mpp/resource-parsers/ParticleResourceTests.h"
#include "mpp/resource-parsers/RenderGraphResourceTests.h"
#include "mpp/resource-parsers/SceneParser.h"
#include "mpp/resource-parsers/SceneSerializer.h"

using namespace mpp;
using namespace pipeline_editor;

namespace
{
	std::shared_ptr<PbrPipelineDocument> clonePipeline(std::shared_ptr<PbrPipelineDocument> const& value)
	{
		if (!value)
			return {};
		auto result = std::make_shared<PbrPipelineDocument>(*value);
		if (value->graph)
			result->graph = std::make_shared<RenderGraph>(*value->graph);
		return result;
	}
	class PipelineSnapshotCommand final : public mpp::app::EditorCommand
	{
		std::string mName;
		std::shared_ptr<PbrPipelineDocument>* mTarget;
		std::shared_ptr<PbrPipelineDocument> mBefore, mAfter;

	public:
		PipelineSnapshotCommand(std::string name,
		                        std::shared_ptr<PbrPipelineDocument>* target,
		                        std::shared_ptr<PbrPipelineDocument> before,
		                        std::shared_ptr<PbrPipelineDocument> after)
		    : mName(std::move(name)), mTarget(target), mBefore(std::move(before)), mAfter(std::move(after))
		{
		}
		std::string const& name() const override
		{
			return mName;
		}
		void execute() override
		{
			*mTarget = clonePipeline(mAfter);
		}
		void undo() override
		{
			*mTarget = clonePipeline(mBefore);
		}
		bool merge(mpp::app::EditorCommand const& other) override
		{
			auto value = dynamic_cast<PipelineSnapshotCommand const*>(&other);
			if (!value || value->mTarget != mTarget || value->mName != mName)
				return false;
			mAfter = clonePipeline(value->mAfter);
			return true;
		}
	};
	class SceneSnapshotCommand final : public mpp::app::EditorCommand
	{
		std::string mName;
		std::shared_ptr<SceneDocument>* mTarget;
		std::shared_ptr<SceneDocument> mBefore, mAfter;
		static std::shared_ptr<SceneDocument> clone(std::shared_ptr<SceneDocument> const& value)
		{
			return value ? std::make_shared<SceneDocument>(*value) : nullptr;
		}

	public:
		SceneSnapshotCommand(std::string name,
		                     std::shared_ptr<SceneDocument>* target,
		                     std::shared_ptr<SceneDocument> before,
		                     std::shared_ptr<SceneDocument> after)
		    : mName(std::move(name)), mTarget(target), mBefore(std::move(before)), mAfter(std::move(after))
		{
		}
		std::string const& name() const override
		{
			return mName;
		}
		void execute() override
		{
			*mTarget = clone(mAfter);
		}
		void undo() override
		{
			*mTarget = clone(mBefore);
		}
		bool merge(mpp::app::EditorCommand const& other) override
		{
			auto value = dynamic_cast<SceneSnapshotCommand const*>(&other);
			if (!value || value->mTarget != mTarget || value->mName != mName)
				return false;
			mAfter = clone(value->mAfter);
			return true;
		}
	};

	std::string uniqueName(std::string base, std::function<bool(std::string const&)> const& exists)
	{
		if (!exists(base))
			return base;
		for (unsigned suffix = 2;; ++suffix)
		{
			auto candidate = base + std::to_string(suffix);
			if (!exists(candidate))
				return candidate;
		}
	}

	std::filesystem::path editorExecutableDirectory()
	{
		std::vector<wchar_t> filename(32768);
		auto length = GetModuleFileNameW(nullptr, filename.data(), (DWORD)filename.size());
		if (length == 0 || length == filename.size())
			throw std::runtime_error("Could not determine the PipelineEditor executable directory.");
		return std::filesystem::path(std::wstring(filename.data(), length)).parent_path();
	}

	std::string trim(std::string value)
	{
		auto first = value.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
			return {};
		auto last = value.find_last_not_of(" \t\r\n");
		return value.substr(first, last - first + 1);
	}

	struct EditorSettings
	{
		std::filesystem::path iniPath;
		std::filesystem::path resourceLocation;
		std::filesystem::path renderDocExecutable;
		std::filesystem::path captureDirectory;
	};

	EditorSettings loadEditorSettings()
	{
		EditorSettings settings;
		settings.iniPath = editorExecutableDirectory() / "editor.ini";
		std::ifstream ini(settings.iniPath);
		if (!ini)
			throw std::runtime_error("Could not open editor configuration '" + settings.iniPath.string() + "'.");

		std::string section;
		std::string resourceValue;
		std::string renderDocValue;
		std::string captureValue;
		for (std::string line; std::getline(ini, line);)
		{
			line = trim(line);
			if (line.empty() || line[0] == ';' || line[0] == '#')
				continue;
			if (line.front() == '[' && line.back() == ']')
			{
				section = trim(line.substr(1, line.size() - 2));
				continue;
			}
			auto separator = line.find('=');
			if (separator == std::string::npos)
				throw std::runtime_error("Invalid editor.ini entry: " + line);
			auto key = trim(line.substr(0, separator));
			auto value = trim(line.substr(separator + 1));
			if (section == "Editor" && key == "resourcesLocation")
				resourceValue = value;
			if (section == "RenderDoc" && key == "executable")
				renderDocValue = value;
			if (section == "RenderDoc" && key == "captureDirectory")
				captureValue = value;
		}
		if (resourceValue.empty())
			throw std::runtime_error("editor.ini does not define [Editor] resourcesLocation.");

		auto resolve = [&](std::string const& value)
		{
			if (value.empty())
				return std::filesystem::path{};
			auto path = std::filesystem::path(value);
			if (path.is_relative())
				path = settings.iniPath.parent_path() / path;
			return std::filesystem::weakly_canonical(path);
		};
		settings.resourceLocation = resolve(resourceValue);
		settings.renderDocExecutable = resolve(renderDocValue);
		settings.captureDirectory = resolve(captureValue);
		if (!std::filesystem::is_directory(settings.resourceLocation))
		{
			throw std::runtime_error("Configured editor resource directory does not exist: " +
			                         settings.resourceLocation.string());
		}
		return settings;
	}

	void saveRenderDocSettings(EditorSettings const& settings)
	{
		std::ifstream input(settings.iniPath);
		if (!input)
			throw std::runtime_error("Could not read '" + settings.iniPath.string() + "'.");
		std::vector<std::string> lines;
		for (std::string line; std::getline(input, line);)
			lines.push_back(line);

		auto setValue =
		    [&](std::string const& requestedSection, std::string const& requestedKey, std::string const& value)
		{
			std::string section;
			bool sectionFound = false;
			size_t sectionEnd = lines.size();
			for (size_t index = 0; index < lines.size(); ++index)
			{
				auto line = trim(lines[index]);
				if (line.size() >= 2 && line.front() == '[' && line.back() == ']')
				{
					if (section == requestedSection && sectionEnd == lines.size())
						sectionEnd = index;
					section = trim(line.substr(1, line.size() - 2));
					sectionFound |= section == requestedSection;
					continue;
				}
				if (section != requestedSection)
					continue;
				auto separator = line.find('=');
				if (separator != std::string::npos && trim(line.substr(0, separator)) == requestedKey)
				{
					lines[index] = requestedKey + "=" + value;
					return;
				}
			}
			if (sectionFound)
			{
				lines.insert(lines.begin() + sectionEnd, requestedKey + "=" + value);
				return;
			}
			if (!lines.empty() && !lines.back().empty())
				lines.push_back({});
			lines.push_back("[" + requestedSection + "]");
			lines.push_back(requestedKey + "=" + value);
		};

		setValue("RenderDoc", "executable", settings.renderDocExecutable.string());
		setValue("RenderDoc", "captureDirectory", settings.captureDirectory.string());
		std::ostringstream output;
		for (auto const& line : lines)
			output << line << '\n';
		mpp::app::atomicWriteText(settings.iniPath, output.str());
	}

	class RenderDocCapture
	{
		HMODULE mModule{nullptr};
		RENDERDOC_API_1_1_1* mApi{nullptr};
		uint32_t mCaptureCountBefore{0};
		std::filesystem::path mRequestedCapture;

	public:
		void initialise(std::filesystem::path const& executable)
		{
			if (mApi)
				return;
			auto library = executable.parent_path() / "renderdoc.dll";
			if (!std::filesystem::is_regular_file(library))
			{
				throw std::runtime_error("RenderDoc library was not found beside qrenderdoc.exe: " + library.string());
			}
			mModule = LoadLibraryW(library.c_str());
			if (!mModule)
				throw std::runtime_error("Could not load RenderDoc library: " + library.string());
			auto getApi = reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(mModule, "RENDERDOC_GetAPI"));
			if (!getApi || getApi(eRENDERDOC_API_Version_1_1_1, reinterpret_cast<void**>(&mApi)) != 1 || !mApi)
			{
				throw std::runtime_error("Could not acquire the RenderDoc 1.1.1 API.");
			}

			// PipelineEditor owns capture triggering. Disable RenderDoc's built-in
			// keyboard hooks so loading renderdoc.dll cannot independently capture
			// frames (including its default F12 capture shortcut).
			mApi->SetCaptureKeys(nullptr, 0);
			mApi->SetFocusToggleKeys(nullptr, 0);
			mApi->MaskOverlayBits(0, 0);
		}

		void discardUnexpectedCapture() noexcept
		{
			if (!mApi || !mApi->IsFrameCapturing())
				return;
			try
			{
				auto capturesBefore = mApi->GetNumCaptures();
				mApi->EndFrameCapture(nullptr, nullptr);
				auto capturesAfter = mApi->GetNumCaptures();
				if (capturesAfter <= capturesBefore)
					return;
				uint32_t pathLength = 0;
				if (!mApi->GetCapture(capturesAfter - 1, nullptr, &pathLength, nullptr) || pathLength == 0)
					return;
				std::vector<char> path(pathLength + 1, 0);
				if (!mApi->GetCapture(capturesAfter - 1, path.data(), &pathLength, nullptr))
					return;
				std::error_code ignored;
				std::filesystem::remove(std::filesystem::path(path.data()), ignored);
			}
			catch (...)
			{
			}
		}

		void begin(std::filesystem::path const& captureDirectory)
		{
			std::error_code error;
			std::filesystem::create_directories(captureDirectory, error);
			if (error || !std::filesystem::is_directory(captureDirectory))
			{
				throw std::runtime_error("Could not create capture directory: " + captureDirectory.string());
			}

			auto now = std::chrono::system_clock::now();
			auto time = std::chrono::system_clock::to_time_t(now);
			std::tm local{};
			localtime_s(&local, &time);
			std::ostringstream name;
			name << "PipelineEditor_" << std::put_time(&local, "%Y-%m-%d_%H-%M-%S");
			mRequestedCapture = captureDirectory / (name.str() + ".rdc");
			for (unsigned suffix = 2; std::filesystem::exists(mRequestedCapture); ++suffix)
			{
				mRequestedCapture = captureDirectory / (name.str() + "_" + std::to_string(suffix) + ".rdc");
			}
			auto pathTemplate = mRequestedCapture;
			pathTemplate.replace_extension();
			if (mApi->IsFrameCapturing())
			{
				throw std::runtime_error("RenderDoc was already capturing before PipelineEditor requested a frame.");
			}
			mCaptureCountBefore = mApi->GetNumCaptures();
			mApi->SetLogFilePathTemplate(pathTemplate.string().c_str());
			mApi->StartFrameCapture(nullptr, nullptr);
			if (!mApi->IsFrameCapturing())
			{
				throw std::runtime_error("RenderDoc did not start a viewport capture.");
			}
		}

		std::filesystem::path end()
		{
			if (!mApi->EndFrameCapture(nullptr, nullptr))
				throw std::runtime_error("RenderDoc failed to complete the viewport capture.");
			auto captureCount = mApi->GetNumCaptures();
			if (captureCount <= mCaptureCountBefore)
				throw std::runtime_error("RenderDoc completed without reporting a capture file.");
			uint32_t pathLength = 0;
			if (!mApi->GetCapture(captureCount - 1, nullptr, &pathLength, nullptr) || pathLength == 0)
			{
				throw std::runtime_error("RenderDoc did not report the capture path.");
			}
			std::vector<char> path(pathLength + 1, 0);
			if (!mApi->GetCapture(captureCount - 1, path.data(), &pathLength, nullptr))
			{
				throw std::runtime_error("RenderDoc did not return the capture path.");
			}
			std::filesystem::path generated(path.data());
			if (generated != mRequestedCapture)
			{
				std::error_code error;
				std::filesystem::rename(generated, mRequestedCapture, error);
				if (error)
					throw std::runtime_error("Could not rename RenderDoc capture to '" + mRequestedCapture.string() +
					                         "': " + error.message());
			}
			return mRequestedCapture;
		}
	};

	void launchRenderDoc(std::filesystem::path const& executable, std::filesystem::path const& capture)
	{
		std::wstring command = L"\"" + executable.wstring() + L"\" \"" + capture.wstring() + L"\"";
		std::vector<wchar_t> writable(command.begin(), command.end());
		writable.push_back(L'\0');
		STARTUPINFOW startup{};
		startup.cb = sizeof(startup);
		PROCESS_INFORMATION process{};
		auto workingDirectory = executable.parent_path().wstring();
		if (!CreateProcessW(nullptr,
		                    writable.data(),
		                    nullptr,
		                    nullptr,
		                    FALSE,
		                    0,
		                    nullptr,
		                    workingDirectory.c_str(),
		                    &startup,
		                    &process))
		{
			throw std::runtime_error("Could not launch RenderDoc for capture: " + capture.string());
		}
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
	}

	mpp::data::StructuredData meshSpecification()
	{
		mpp::data::StructuredData mesh("MeshSpecification");
		mesh.addEntry("primitive", "triangles");
		mesh.addEntry("indexed", "true");
		mesh.addEntry("storage", "static");
		mpp::data::StructuredData buffer("Buffer");
		auto channel = [&](char const* data, bool normalised = false)
		{
			mpp::data::StructuredData value("Channel");
			value.addEntry("data", data);
			value.addEntry("type", "float32");
			if (normalised)
				value.addEntry("normalised", "true");
			buffer.addEntry("Channel", value);
		};
		channel("position3");
		channel("normal3");
		channel("texcoord2");
		channel("colour4", true);
		channel("tangent4");
		mesh.addEntry("Buffer", buffer);
		return mesh;
	}

	uint32_t passNameIndex(std::string const& name)
	{
		auto first = name.find_last_not_of("0123456789");
		if (first == name.size() - 1)
			return 0;
		try
		{
			return (uint32_t)std::stoul(name.substr(first + 1));
		}
		catch (...)
		{
			return 0;
		}
	}

	PbrPipelineResourceDocument makeLocalResource(PbrPipelineResourceKind kind, std::string const& name);

	/* GLTF_LOCAL_CONVERTER_REMOVED */
	char const* localResourceKindName(PbrPipelineResourceKind kind)
	{
		switch (kind)
		{
		case PbrPipelineResourceKind::PbrMaterial:
			return "PBR Material";
		case PbrPipelineResourceKind::Program:
			return "Program";
		case PbrPipelineResourceKind::Texture:
			return "Texture";
		case PbrPipelineResourceKind::PostEffectMaterial:
			return "Post Effect Material";
		case PbrPipelineResourceKind::ParticleEffect:
			return "Particle Effect";
		default:
			return "Sampler";
		}
	}

	PbrPipelineResourceDocument makeLocalResource(PbrPipelineResourceKind kind, std::string const& name)
	{
		PbrPipelineResourceDocument value;
		value.name = name;
		value.kind = kind;
		if (kind == PbrPipelineResourceKind::PbrMaterial)
		{
			value.definition = mpp::data::StructuredData("PbrMaterial");
			value.definition.addEntry("name", name);
			value.definition.addEntry("MeshSpecification", meshSpecification());
			mpp::data::StructuredData surface("Surface");
			surface.addEntry("baseColourFactor", "1 1 1 1");
			surface.addEntry("metallicFactor", "0");
			surface.addEntry("roughnessFactor", "1");
			surface.addEntry("emissiveFactor", "0 0 0");
			surface.addEntry("normalScale", "1");
			surface.addEntry("occlusionStrength", "1");
			surface.addEntry("alphaMode", "OPAQUE");
			surface.addEntry("alphaCutoff", "0.5");
			surface.addEntry("doubleSided", "false");
			value.definition.addEntry("Surface", surface);
		}
		else if (kind == PbrPipelineResourceKind::Program)
		{
			value.definition = mpp::data::StructuredData("Program");
			value.definition.addEntry("name", name);
			value.definition.addEntry("positionType", "3D");
			value.definition.addEntry("textures", "0");
			value.definition.addEntry("MeshSpecification", meshSpecification());
		}
		else if (kind == PbrPipelineResourceKind::Texture)
		{
			value.definition = mpp::data::StructuredData("Texture");
			value.definition.addEntry("name", name);
			value.definition.addEntry("target", "2D");
			value.definition.addEntry("filename", "shared/pbr/arrow.png");
			value.definition.addEntry("colourSpace", "LINEAR");
			value.definition.addEntry("minFilter", "LINEAR");
			value.definition.addEntry("magFilter", "LINEAR");
			value.definition.addEntry("wrap", "CLAMP_TO_EDGE");
		}
		else if (kind == PbrPipelineResourceKind::ParticleEffect)
		{
			value.definition = mpp::data::StructuredData("ParticleEffect");
			value.definition.addEntry("version", "1");
			value.definition.addEntry("name", name);
			value.definition.addEntry("maximumParticleCount", "1024");
			mpp::data::StructuredData emitters("Emitters"), emitter("Emitter"), spawn("Spawn");
			emitter.addEntry("name", "Emitter");
			emitter.addEntry("maximumParticleCount", "1024");
			spawn.addEntry("shape", "point");
			spawn.addEntry("rate", "10");
			emitter.addEntry("Spawn", spawn);
			emitters.addEntry("Emitter", emitter);
			value.definition.addEntry("Emitters", emitters);
		}
		else if (kind == PbrPipelineResourceKind::PostEffectMaterial)
		{
			value.definition = mpp::data::StructuredData("PostEffectMaterial");
			value.definition.addEntry("name", name);
			mpp::data::StructuredData program("Program");
			program.addEntry("Ref", "__mpp_p2d_bloom_extract__");
			value.definition.addEntry("Program", program);
			mpp::data::StructuredData samplerSlots("SamplerSlots");
			samplerSlots.addEntry("Slot", "TEX1");
			value.definition.addEntry("SamplerSlots", samplerSlots);
			value.definition.addEntry("Uniforms", mpp::data::StructuredData("Uniforms"));
		}
		else
		{
			value.definition = mpp::data::StructuredData("Sampler");
			value.definition.addEntry("name", name);
			value.definition.addEntry("minFilter", "LINEAR");
			value.definition.addEntry("magFilter", "LINEAR");
			value.definition.addEntry("wrap", "CLAMP_TO_EDGE");
			value.definition.addEntry("maxAnisotropy", "1");
		}
		return value;
	}

	void renameResource(PbrPipelineResourceDocument& resource, std::string const& name)
	{
		resource.name = name;
		resource.definition.setEntryValue("name", name);
	}

	void renameResourceReferences(PbrPipelineDocument& document, std::string const& oldName, std::string const& newName)
	{
		auto rewrite = [&](std::string& value)
		{
			if (value == oldName)
				value = newName;
		};
		auto rewriteData = [&](auto&& self, mpp::data::StructuredData& data) -> void
		{
			if (data.isValue())
			{
				if (data.getValue() == oldName)
					data.setValue(newName);
			}
			else
				for (auto& entry : data)
					self(self, entry.second);
		};
		for (auto& resource : document.localResources)
			rewriteData(rewriteData, resource.definition);
		for (auto& binding : document.previewBindings)
			rewrite(binding.materialResource);
		rewrite(document.environment.irradiance);
		rewrite(document.environment.prefilteredSpecular);
		rewrite(document.environment.brdfLut);
		rewrite(document.environment.background);
		for (auto& value : document.imports)
			rewrite(value.fallback);
		if (document.graph)
			for (uint32_t pass = 0; pass < document.graph->getPassCount(); ++pass)
				if (document.graph->getPassInfo({pass}).programResource == oldName)
					document.graph->setPassProgramResource({pass}, newName);
	}

	void removeResourceReferences(PbrPipelineDocument& document, std::string const& name)
	{
		auto clear = [&](std::string& value)
		{
			if (value == name)
				value.clear();
		};
		auto clearData = [&](auto&& self, mpp::data::StructuredData& data) -> void
		{
			if (data.isValue())
			{
				if (data.getValue() == name)
					data.setValue("");
			}
			else
				for (auto& entry : data)
					self(self, entry.second);
		};
		for (auto& resource : document.localResources)
			clearData(clearData, resource.definition);
		document.previewBindings.erase(std::remove_if(document.previewBindings.begin(),
		                                              document.previewBindings.end(),
		                                              [&](auto const& value)
		                                              { return value.materialResource == name; }),
		                               document.previewBindings.end());
		clear(document.environment.irradiance);
		clear(document.environment.prefilteredSpecular);
		clear(document.environment.brdfLut);
		clear(document.environment.background);
		for (auto& value : document.imports)
			clear(value.fallback);
		if (document.graph)
			for (uint32_t pass = 0; pass < document.graph->getPassCount(); ++pass)
				if (document.graph->getPassInfo({pass}).programResource == name)
					document.graph->clearPassProgramResource({pass});
	}

	struct SdlLifetime
	{
		~SdlLifetime()
		{
			SDL_Quit();
		}
	};
	struct ChangeFlag
	{
		bool value{false};
		uint64_t* serial{nullptr};
		ChangeFlag& operator=(bool updated)
		{
			if (updated && serial)
				++*serial;
			value = updated;
			return *this;
		}
		operator bool() const
		{
			return value;
		}
	};

	struct PreparedPreview
	{
		std::shared_ptr<PbrPipelineDocument> pipeline;
		std::shared_ptr<SceneDocument> scene;
		std::string pipelinePath;
		std::string scenePath;
		std::vector<std::filesystem::path> dependencies;
		std::map<std::string, mpp::app::DocumentFileRevision> dependencyRevisions;
		bool hotReload{false};
		std::string validationFailure;
	};

	void collectPayloadPaths(mpp::data::StructuredData const& data,
	                         std::filesystem::path const& owner,
	                         std::set<std::filesystem::path>& paths)
	{
		for (auto const& entry : data)
		{
			auto key = entry.first;
			std::transform(
			    key.begin(), key.end(), key.begin(), [](unsigned char value) { return (char)std::tolower(value); });
			if (entry.second.isValue() && (key == "filename" || key == "file" || key == "vertexshader" ||
			                               key == "fragmentshader" || key == "geometryshader"))
			{
				auto value = entry.second.getValue();
				if (!value.empty() && value.find('\n') == std::string::npos)
					paths.insert(mpp::app::normaliseDocumentPath(owner.parent_path() / value));
			}
			else if (!entry.second.isValue())
				collectPayloadPaths(entry.second, owner, paths);
		}
	}

	void rewritePackagePayloadPaths(mpp::data::StructuredData& data,
	                                std::filesystem::path const& owner,
	                                std::map<std::filesystem::path, std::string>& payloads,
	                                std::map<std::string, std::filesystem::path>& directoryPayloads,
	                                bool shaderBlock = false)
	{
		for (auto& entry : data)
		{
			auto key = entry.first;
			std::transform(
			    key.begin(), key.end(), key.begin(), [](unsigned char value) { return (char)std::tolower(value); });
			if (entry.second.isValue() && (key == "filename" || key == "file" || key == "vertexshader" ||
			                               key == "fragmentshader" || key == "geometryshader"))
			{
				auto value = entry.second.getValue();
				if (value.empty() || value.find('\n') != std::string::npos)
					continue;
				auto source = mpp::app::normaliseDocumentPath(owner.parent_path() / value);
				if (!std::filesystem::is_regular_file(source))
					throw std::runtime_error("Package export is missing payload '" + source.string() + "'.");
				if (key == "vertexshader" || key == "fragmentshader" || key == "geometryshader" ||
				    (shaderBlock && key == "file"))
				{
					auto root = source.parent_path();
					std::string packageRoot = "shaders/" + std::to_string(directoryPayloads.size() + 1);
					for (auto const& child : std::filesystem::recursive_directory_iterator(root))
						if (child.is_regular_file())
							directoryPayloads.emplace(
							    packageRoot + "/" + std::filesystem::relative(child.path(), root).generic_string(),
							    mpp::app::normaliseDocumentPath(child.path()));
					entry.second.setValue(packageRoot + "/" + source.filename().generic_string());
				}
				else
				{
					auto found = payloads.find(source);
					if (found == payloads.end())
					{
						auto name = source.filename().string();
						for (char& character : name)
							if (character == '\\' || character == '/')
								character = '_';
						found = payloads.emplace(source, "assets/" + std::to_string(payloads.size() + 1) + "_" + name)
						            .first;
					}
					entry.second.setValue(found->second);
				}
			}
			else if (!entry.second.isValue())
				rewritePackagePayloadPaths(entry.second,
				                           owner,
				                           payloads,
				                           directoryPayloads,
				                           shaderBlock || key == "vertexshader" || key == "fragmentshader" ||
				                               key == "geometryshader");
		}
	}

	std::string packageDiagnosticSummary(DiagnosticBag const& diagnostics)
	{
		std::string result;
		for (auto const& diagnostic : diagnostics.getDiagnostics())
			if (diagnostic.severity == DiagnosticSeverity::Error)
			{
				if (!result.empty())
					result += '\n';
				result += "[" + diagnostic.code + "] " + diagnostic.message;
			}
		return result;
	}

	void exportPipelinePackage(PbrPipelineDocument const& sourcePipeline,
	                           SceneDocument const& sourceScene,
	                           std::string const& pipelinePath,
	                           std::string const& scenePath,
	                           std::filesystem::path const& destination)
	{
		auto pipeline = sourcePipeline;
		if (sourcePipeline.graph)
			pipeline.graph = std::make_shared<RenderGraph>(*sourcePipeline.graph);
		auto scene = sourceScene;
		if (scene.environmentBinding != pipeline.environment.binding)
			throw std::runtime_error("Package export requires the preview-scene environment binding to match the "
			                         "pipeline environment binding.");
		auto diagnostics = pipeline.validate();
		diagnostics.append(resource_parsers::validatePbrPipelineResourceDefinitions(pipeline));
		diagnostics.append(scene.validate());
		for (auto const& effect : scene.particleEffects)
		{
			bool resolved = std::any_of(pipeline.localResources.begin(), pipeline.localResources.end(), [&](auto const& resource) { return resource.name == effect.effect && resource.kind == PbrPipelineResourceKind::ParticleEffect; }) ||
				std::any_of(pipeline.externalResources.begin(), pipeline.externalResources.end(), [&](auto const& resource) { return resource.libraryName + "::" + resource.resource.name == effect.effect && resource.resource.kind == PbrPipelineResourceKind::ParticleEffect; });
			if (!resolved) diagnostics.error("MPP-SCENE-035", "Scene particle effect resource '" + effect.effect + "' is unavailable or has the wrong type.", { scene.sourcePath }, effect.id);
		}
		if (diagnostics.hasErrors())
			throw std::runtime_error("Package export requires a valid pipeline and preview scene.\n" +
			                         packageDiagnosticSummary(diagnostics));
		std::map<std::string, std::string> localized;
		std::map<std::string, std::filesystem::path> localizedOwners;
		std::set<std::string> names;
		for (auto const& resource : pipeline.localResources)
			names.insert(resource.name);
		for (auto const& external : pipeline.externalResources)
		{
			auto resource = external.resource;
			auto base = external.libraryName + "." + resource.name, local = base;
			unsigned suffix = 2;
			while (names.contains(local))
				local = base + std::to_string(suffix++);
			names.insert(local);
			localized[external.libraryName + "::" + resource.name] = local;
			localizedOwners[local] = external.libraryPath;
			renameResource(resource, local);
			pipeline.localResources.push_back(std::move(resource));
		}
		auto rewriteReference = [&](auto&& self, mpp::data::StructuredData& value) -> void
		{
			if (value.isValue())
			{
				auto found = localized.find(value.getValue());
				if (found != localized.end())
					value.setValue(found->second);
			}
			else
				for (auto& entry : value)
					self(self, entry.second);
		};
		for (auto& resource : pipeline.localResources)
			rewriteReference(rewriteReference, resource.definition);
		auto replace = [&](std::string& value)
		{
			auto found = localized.find(value);
			if (found != localized.end())
				value = found->second;
		};
		for (auto& binding : pipeline.previewBindings)
			replace(binding.materialResource);
		for (auto& effect : scene.particleEffects)
			replace(effect.effect);
		replace(pipeline.environment.irradiance);
		replace(pipeline.environment.prefilteredSpecular);
		replace(pipeline.environment.brdfLut);
		replace(pipeline.environment.background);
		pipeline.externalResources.clear();
		pipeline.resourceLibraries.clear();
		auto temporary = mpp::app::createUniqueTemporaryDirectory("MPP");
		try
		{
			std::map<std::filesystem::path, std::string> payloads;
			std::map<std::string, std::filesystem::path> directoryPayloads;
			auto pipelineOwner = pipelinePath.empty() ? std::filesystem::path(sourcePipeline.sourcePath)
			                                          : std::filesystem::path(pipelinePath);
			for (auto& resource : pipeline.localResources)
				rewritePackagePayloadPaths(resource.definition,
				                           localizedOwners.contains(resource.name) ? localizedOwners[resource.name]
				                                                                   : pipelineOwner,
				                           payloads,
				                           directoryPayloads);
			if (!pipeline.environment.hdrEquirectangular.empty())
			{
				auto hdrSource = mpp::app::normaliseDocumentPath(pipelineOwner.parent_path() / pipeline.environment.hdrEquirectangular);
				if (!std::filesystem::is_regular_file(hdrSource)) throw std::runtime_error("Package export is missing HDR IBL source '" + hdrSource.string() + "'.");
				auto packageName = "hdr/" + hdrSource.filename().generic_string();
				auto nameUsed = [&]() { if (directoryPayloads.contains(packageName)) return true; for (auto const& [source, name] : payloads) if (name == packageName) return true; return false; };
				for (uint32_t suffix = 2; nameUsed(); ++suffix) packageName = "hdr/" + hdrSource.stem().generic_string() + "_" + std::to_string(suffix) + hdrSource.extension().generic_string();
				payloads.emplace(hdrSource, packageName);
				pipeline.environment.hdrEquirectangular = packageName;
			}
			auto sceneOwner =
			    scenePath.empty() ? std::filesystem::path(sourceScene.sourcePath) : std::filesystem::path(scenePath);
			for (auto& model : scene.models)
				if (model.source == SceneModelSource::MppModel && !model.file.empty())
				{
					auto path = mpp::app::normaliseDocumentPath(sceneOwner.parent_path() / model.file);
					if (!std::filesystem::is_regular_file(path))
						throw std::runtime_error("Package export is missing model '" + path.string() + "'.");
					auto modelRoot = path.parent_path();
					std::string packageRoot = "models/" + std::to_string(directoryPayloads.size() + 1);
					for (auto const& entry : std::filesystem::recursive_directory_iterator(modelRoot))
					{
						if (!entry.is_regular_file())
							continue;
						auto relative = std::filesystem::relative(entry.path(), modelRoot);
						directoryPayloads.emplace(packageRoot + "/" + relative.generic_string(),
						                          mpp::app::normaliseDocumentPath(entry.path()));
					}
					model.file = packageRoot + "/" + path.filename().generic_string();
				}
			pipeline.previewScene = "scene.yaml";
			pipeline.sourcePath = (temporary / "pipeline.yaml").string();
			scene.sourcePath = (temporary / "scene.yaml").string();
			resource_parsers::PbrPipelineSerializer::toFile(pipeline, pipeline.sourcePath);
			resource_parsers::SceneSerializer::toFile(scene, scene.sourcePath);
			mpp::app::writePackageManifest(temporary / "manifest.xml");
			std::map<std::string, std::filesystem::path> files{{"manifest.xml", temporary / "manifest.xml"},
			                                                   {"pipeline.yaml", temporary / "pipeline.yaml"},
			                                                   {"scene.yaml", temporary / "scene.yaml"}};
			for (auto const& [source, name] : payloads)
				files.emplace(name, source);
			for (auto const& [name, source] : directoryPayloads)
				files.emplace(name, source);
			mpp::app::ZipArchive::write(destination, files);
		}
		catch (...)
		{
			std::filesystem::remove_all(temporary);
			throw;
		}
		std::filesystem::remove_all(temporary);
	}

	// Mirrors exportPipelinePackage, but first converts the source PBR
	// pipeline (and its PbrMaterial resources) down to a LegacyPipeline
	// document + BasicMaterial resources (see
	// doc/implemented/LEGACY_PIPELINE_EXPORT_PLAN.md). The localization/asset-copy steps
	// below are the same generic StructuredData-walking logic used for the
	// Pbr export; only the document/resource types and the extra
	// baked-texture ownership override differ.
	void exportLegacyPipelinePackage(PbrPipelineDocument const& sourcePipeline,
	                                 SceneDocument const& sourceScene,
	                                 std::string const& pipelinePath,
	                                 std::string const& scenePath,
	                                 std::filesystem::path const& destination)
	{
		auto temporary = mpp::app::createUniqueTemporaryDirectory("MPP");
		try
		{
			DiagnosticBag conversionDiagnostics;
			auto pipeline =
			    resource_parsers::convertPbrPipelineToLegacy(sourcePipeline, temporary.string(), conversionDiagnostics);
			auto scene = sourceScene;
			auto diagnostics = pipeline.validate();
			diagnostics.append(resource_parsers::validateLegacyPipelineResourceDefinitions(pipeline));
			diagnostics.append(scene.validate());
			diagnostics.append(conversionDiagnostics);
			for (auto const& effect : scene.particleEffects)
			{
				bool resolved = std::any_of(pipeline.localResources.begin(), pipeline.localResources.end(), [&](auto const& resource) { return resource.name == effect.effect && resource.kind == LegacyPipelineResourceKind::ParticleEffect; }) ||
					std::any_of(pipeline.externalResources.begin(), pipeline.externalResources.end(), [&](auto const& resource) { return resource.libraryName + "::" + resource.resource.name == effect.effect && resource.resource.kind == LegacyPipelineResourceKind::ParticleEffect; });
				if (!resolved) diagnostics.error("MPP-SCENE-035", "Scene particle effect resource '" + effect.effect + "' is unavailable or has the wrong type.", { scene.sourcePath }, effect.id);
			}
			if (!sourceScene.environmentBinding.empty())
				diagnostics.warning("MPP-LEGACY-EXPORT-001",
				                    "Preview scene's environment binding '" + sourceScene.environmentBinding +
				                        "' has no legacy equivalent and will be ignored.",
				                    {},
				                    "environment");
			if (diagnostics.hasErrors())
				throw std::runtime_error("Legacy package export requires a valid pipeline and preview scene.\n" +
				                         packageDiagnosticSummary(diagnostics));

			// Materials that had no base-colour texture got a generated
			// flat-colour PNG baked into `temporary` by the conversion above
			// (see mpp::convertPbrMaterialToBasic); their <filename> is bare,
			// so their payload-rewrite "owner" must point at `temporary`
			// rather than the source pipeline's own directory.
			std::set<std::string> bakedTextureMaterials;
			for (auto const& diagnostic : conversionDiagnostics.getDiagnostics())
				if (diagnostic.code == "MPP-LEGACY-MATERIAL-002")
					bakedTextureMaterials.insert(diagnostic.objectId);

			std::map<std::string, std::string> localized;
			std::map<std::string, std::filesystem::path> localizedOwners;
			std::set<std::string> names;
			for (auto const& resource : pipeline.localResources)
				names.insert(resource.name);
			for (auto const& external : pipeline.externalResources)
			{
				auto resource = external.resource;
				auto base = external.libraryName + "." + resource.name, local = base;
				unsigned suffix = 2;
				while (names.contains(local))
					local = base + std::to_string(suffix++);
				names.insert(local);
				localized[external.libraryName + "::" + resource.name] = local;
				localizedOwners[local] = external.libraryPath;
				resource.name = local;
				resource.definition.setEntryValue("name", local);
				pipeline.localResources.push_back(std::move(resource));
			}
			auto rewriteReference = [&](auto&& self, mpp::data::StructuredData& value) -> void
			{
				if (value.isValue())
				{
					auto found = localized.find(value.getValue());
					if (found != localized.end())
						value.setValue(found->second);
				}
				else
					for (auto& entry : value)
						self(self, entry.second);
			};
			for (auto& resource : pipeline.localResources)
				rewriteReference(rewriteReference, resource.definition);
			auto replace = [&](std::string& value)
			{
				auto found = localized.find(value);
				if (found != localized.end())
					value = found->second;
			};
			for (auto& binding : pipeline.previewBindings)
				replace(binding.materialResource);
			for (auto& effect : scene.particleEffects)
				replace(effect.effect);
			pipeline.externalResources.clear();
			pipeline.resourceLibraries.clear();

			std::map<std::filesystem::path, std::string> payloads;
			std::map<std::string, std::filesystem::path> directoryPayloads;
			auto pipelineOwner = pipelinePath.empty() ? std::filesystem::path(sourcePipeline.sourcePath)
			                                          : std::filesystem::path(pipelinePath);
			auto bakedOwner = temporary / "pipeline.yaml";
			for (auto& resource : pipeline.localResources)
				rewritePackagePayloadPaths(resource.definition,
				                           bakedTextureMaterials.contains(resource.name) ? bakedOwner
				                           : localizedOwners.contains(resource.name)     ? localizedOwners[resource.name]
				                                                                          : pipelineOwner,
				                           payloads,
				                           directoryPayloads);
			auto sceneOwner =
			    scenePath.empty() ? std::filesystem::path(sourceScene.sourcePath) : std::filesystem::path(scenePath);
			for (auto& model : scene.models)
				if (model.source == SceneModelSource::MppModel && !model.file.empty())
				{
					auto path = mpp::app::normaliseDocumentPath(sceneOwner.parent_path() / model.file);
					if (!std::filesystem::is_regular_file(path))
						throw std::runtime_error("Package export is missing model '" + path.string() + "'.");
					auto modelRoot = path.parent_path();
					std::string packageRoot = "models/" + std::to_string(directoryPayloads.size() + 1);
					for (auto const& entry : std::filesystem::recursive_directory_iterator(modelRoot))
					{
						if (!entry.is_regular_file())
							continue;
						auto relative = std::filesystem::relative(entry.path(), modelRoot);
						directoryPayloads.emplace(packageRoot + "/" + relative.generic_string(),
						                          mpp::app::normaliseDocumentPath(entry.path()));
					}
					model.file = packageRoot + "/" + path.filename().generic_string();
				}
			pipeline.previewScene = "scene.yaml";
			pipeline.sourcePath = (temporary / "pipeline.yaml").string();
			scene.sourcePath = (temporary / "scene.yaml").string();
			resource_parsers::LegacyPipelineSerializer::toFile(pipeline, pipeline.sourcePath);
			resource_parsers::SceneSerializer::toFile(scene, scene.sourcePath);
			mpp::app::writePackageManifest(temporary / "manifest.xml");
			std::map<std::string, std::filesystem::path> files{{"manifest.xml", temporary / "manifest.xml"},
			                                                   {"pipeline.yaml", temporary / "pipeline.yaml"},
			                                                   {"scene.yaml", temporary / "scene.yaml"}};
			for (auto const& [source, name] : payloads)
				files.emplace(name, source);
			for (auto const& [name, source] : directoryPayloads)
				files.emplace(name, source);
			mpp::app::ZipArchive::write(destination, files);
		}
		catch (...)
		{
			std::filesystem::remove_all(temporary);
			throw;
		}
		std::filesystem::remove_all(temporary);
	}

	std::vector<std::filesystem::path> workspaceDependencies(PbrPipelineDocument const& pipeline,
	                                                         SceneDocument const* scene,
	                                                         std::string const& pipelinePath,
	                                                         std::string const& scenePath)
	{
		std::set<std::filesystem::path> paths;
		if (!pipelinePath.empty())
			paths.insert(mpp::app::normaliseDocumentPath(pipelinePath));
		if (!scenePath.empty())
			paths.insert(mpp::app::normaliseDocumentPath(scenePath));
		auto pipelineOwner = pipelinePath.empty() ? pipeline.sourcePath : pipelinePath;
		if (!pipeline.environment.hdrEquirectangular.empty() && !pipelineOwner.empty()) paths.insert(mpp::app::resolveDocumentReference(pipelineOwner, pipeline.environment.hdrEquirectangular));
		for (auto const& library : pipeline.resourceLibraries)
			if (!pipelineOwner.empty())
				paths.insert(mpp::app::resolveDocumentReference(pipelineOwner, library));
		for (auto const& resource : pipeline.localResources)
			collectPayloadPaths(resource.definition, pipelineOwner, paths);
		for (auto const& resource : pipeline.externalResources)
			collectPayloadPaths(resource.resource.definition, resource.libraryPath, paths);
		if (scene)
			for (auto const& model : scene->models)
				if (model.source == SceneModelSource::MppModel && !model.file.empty())
					paths.insert(mpp::app::resolveDocumentReference(scenePath.empty() ? scene->sourcePath : scenePath,
					                                                model.file));
		return {paths.begin(), paths.end()};
	}

	PreparedPreview preparePreview(std::shared_ptr<PbrPipelineDocument> pipeline,
	                               std::shared_ptr<SceneDocument> scene,
	                               std::string pipelinePath,
	                               std::string scenePath,
	                               bool hotReload,
	                               mpp::app::BackgroundCancellationToken const& cancellation,
	                               mpp::app::BackgroundJobQueue::ProgressCallback const& progress)
	{
		PreparedPreview result;
		result.pipeline = std::move(pipeline);
		result.scene = std::move(scene);
		result.pipelinePath = std::move(pipelinePath);
		result.scenePath = std::move(scenePath);
		result.hotReload = hotReload;
		progress(0.1f, "Validating pipeline and graph");
		cancellation.throwIfCancelled();
		auto diagnostics = result.pipeline->validate();
		diagnostics.append(resource_parsers::validatePbrPipelineResourceDefinitions(*result.pipeline));
		progress(0.4f, "Validating preview scene");
		cancellation.throwIfCancelled();
		if (result.scene)
		{
			diagnostics.append(result.scene->validate());
			if (result.scene->environmentBinding != result.pipeline->environment.binding)
				diagnostics.error("MPP-SCENE-RUNTIME-007",
				                  "Scene environment binding does not match the pipeline environment binding.");
		}
		if (diagnostics.hasErrors())
		{
			result.validationFailure = "Background validation found " +
			                           std::to_string(diagnostics.count(DiagnosticSeverity::Error)) +
			                           " error(s); retaining the last valid preview.";
			return result;
		}
		progress(0.65f, "Reading and decoding dependencies");
		result.dependencies =
		    workspaceDependencies(*result.pipeline, result.scene.get(), result.pipelinePath, result.scenePath);
		size_t index = 0;
		for (auto const& path : result.dependencies)
		{
			cancellation.throwIfCancelled();
			auto extension = path.extension().string();
			std::transform(extension.begin(),
			               extension.end(),
			               extension.begin(),
			               [](unsigned char value) { return (char)std::tolower(value); });
			if (std::filesystem::exists(path) && (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
			                                      extension == ".bmp" || extension == ".gif" || extension == ".tga"))
				mpp::app::loadImageFile(path.string());
			progress(0.65f + 0.3f * (float(++index) / std::max<size_t>(1, result.dependencies.size())),
			         "Reading and decoding dependencies");
		}
		for (auto const& path : result.dependencies)
		{
			cancellation.throwIfCancelled();
			result.dependencyRevisions[path.string()] = mpp::app::captureDocumentFileRevision(path);
		}
		progress(1.0f, "Ready for GPU installation");
		return result;
	}
} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	try
	{
		bool warningsAsErrors = false;
		if (__argc >= 2 && (std::string(__argv[1]) == "--help" || std::string(__argv[1]) == "-h"))
		{
			fprintf(stdout,
			        "PipelineEditor options:\n  --help, -h                                  Show this help.\n  "
			        "--validate [--warnings-as-errors] <pipeline.yaml> Validate a workspace.\n  --export-package "
			        "<pipeline.yaml> <package.mpppackage> Export a self-contained package.\n  --export-legacy-package "
			        "<pipeline.yaml> <package.mpppackage> Export a self-contained legacy package.\n  --smoke-test "
			        "[pipeline.yaml]                 Render 30 frames then exit.\n  --gpu-tests                     "
			        "            Run the render graph GPU suite then exit.\n  --width <pixels> --height <pixels>  "
			        "        Set editor window size.\n  --recovery-seconds <seconds>                Set recovery "
			        "interval.\n  [pipeline.yaml]                              Open a workspace.\n");
			return 0;
		}
		if (__argc >= 2 && std::string(__argv[1]) == "--export-package")
		{
			if (__argc != 4)
			{
				fprintf(stderr, "usage: PipelineEditor --export-package <pipeline.yaml> <package.mpppackage>\n");
				return 2;
			}
			try
			{
				auto pipeline = resource_parsers::PbrPipelineDocumentLoader::fromFile(__argv[2]);
				if (pipeline.previewScene.empty())
					throw std::runtime_error("Package export requires a preview scene.");
				auto scenePath = mpp::app::resolveDocumentReference(__argv[2], pipeline.previewScene);
				auto scene = resource_parsers::SceneParser::fromFile(scenePath.string());
				exportPipelinePackage(
				    pipeline, scene, __argv[2], scenePath.string(), mpp::app::normaliseDocumentPath(__argv[3]));
				fprintf(stderr, "Package exported: %s\n", __argv[3]);
				return 0;
			}
			catch (std::exception const& error)
			{
				fprintf(stderr, "MPP-PACKAGE-EXPORT-001: %s\n", error.what());
				return 1;
			}
		}
		if (__argc >= 2 && std::string(__argv[1]) == "--export-legacy-package")
		{
			if (__argc != 4)
			{
				fprintf(stderr, "usage: PipelineEditor --export-legacy-package <pipeline.yaml> <package.mpppackage>\n");
				return 2;
			}
			try
			{
				auto pipeline = resource_parsers::PbrPipelineDocumentLoader::fromFile(__argv[2]);
				if (pipeline.previewScene.empty())
					throw std::runtime_error("Package export requires a preview scene.");
				auto scenePath = mpp::app::resolveDocumentReference(__argv[2], pipeline.previewScene);
				auto scene = resource_parsers::SceneParser::fromFile(scenePath.string());
				exportLegacyPipelinePackage(
				    pipeline, scene, __argv[2], scenePath.string(), mpp::app::normaliseDocumentPath(__argv[3]));
				fprintf(stderr, "Legacy package exported: %s\n", __argv[3]);
				return 0;
			}
			catch (std::exception const& error)
			{
				fprintf(stderr, "MPP-LEGACY-PACKAGE-EXPORT-001: %s\n", error.what());
				return 1;
			}
		}
		if (__argc >= 2 && std::string(__argv[1]) == "--validate")
		{
			int pathIndex = 2;
			if (__argc > 2 && std::string(__argv[2]) == "--warnings-as-errors")
			{
				warningsAsErrors = true;
				pathIndex = 3;
			}
			if (__argc <= pathIndex)
			{
				fprintf(stderr, "usage: PipelineEditor --validate [--warnings-as-errors] <pipeline.yaml>\n");
				return 2;
			}
			try
			{
				// The context-free suites need no GL context, so CLI validation is
				// where they can actually run. Without this they are dead code and
				// stop protecting the parse/serialize contract they were written for.
				std::string suiteFailure;
				if (!mpp::runRenderGraphTopologyTests(&suiteFailure))
				{
					fprintf(stderr, "MPP-PIPELINE-CLI-002: render graph topology tests failed: %s\n", suiteFailure.c_str());
					return 1;
				}
				if (!resource_parsers::runRenderGraphResourceTests(&suiteFailure))
				{
					fprintf(stderr, "MPP-PIPELINE-CLI-003: render graph resource tests failed: %s\n", suiteFailure.c_str());
					return 1;
				}
				if (!resource_parsers::runParticleResourceTests(&suiteFailure))
				{
					fprintf(stderr, "MPP-PIPELINE-CLI-007: particle resource tests failed: %s\n", suiteFailure.c_str());
					return 1;
				}
				if (!mpp::runPbrMaterialSpecializationTests(&suiteFailure))
				{
					fprintf(stderr, "MPP-PIPELINE-CLI-004: PBR material specialization tests failed: %s\n", suiteFailure.c_str());
					return 1;
				}
				if (!mpp::runParticleSystemCpuTests(&suiteFailure))
				{
					fprintf(stderr, "MPP-PIPELINE-CLI-006: particle system CPU tests failed: %s\n", suiteFailure.c_str());
					return 1;
				}
				auto document = resource_parsers::PbrPipelineDocumentLoader::fromFile(__argv[pathIndex]);
				auto diagnostics = document.validate();
				diagnostics.append(resource_parsers::validatePbrPipelineResourceDefinitions(document));
				if (!document.previewScene.empty())
				{
					auto sceneFile = std::filesystem::path(__argv[pathIndex]).parent_path() / document.previewScene;
					if (std::filesystem::exists(sceneFile))
					{
						auto sceneDocument = resource_parsers::SceneParser::fromFile(sceneFile.string());
						diagnostics.append(sceneDocument.validate());
						if (sceneDocument.environmentBinding != document.environment.binding)
							diagnostics.error(
							    "MPP-SCENE-RUNTIME-007",
							    "Scene environment binding does not match the pipeline environment binding.",
							    {sceneFile.string()},
							    "environment");
					}
					else
						diagnostics.error("MPP-PIPELINE-CLI-001",
						                  "Preview scene does not exist: " + sceneFile.string(),
						                  {__argv[pathIndex]},
						                  "previewScene");
				}
				for (auto const& value : diagnostics.getDiagnostics())
					fprintf(stderr, "%s: %s\n", value.code.c_str(), value.message.c_str());
				return diagnostics.hasErrors(warningsAsErrors) ? 1 : 0;
			}
			catch (std::exception const& error)
			{
				fprintf(stderr, "MPP-PIPELINE-CLI-002: %s\n", error.what());
				return 1;
			}
		}
		auto editorSettings = loadEditorSettings();
		auto renderSystemOptions = mpp::app::loadRenderSystemOptions(editorSettings.iniPath);
		RenderDocCapture renderDocCapture;
		if (std::filesystem::is_regular_file(editorSettings.renderDocExecutable))
		{
			try
			{
				renderDocCapture.initialise(editorSettings.renderDocExecutable);
			}
			catch (...)
			{ /* Report configuration/load errors only when capture is requested. */
			}
		}
		if (!editorSettings.captureDirectory.empty())
		{
			std::error_code ignored;
			std::filesystem::create_directories(editorSettings.captureDirectory, ignored);
		}
		auto editorResourceLocation = editorSettings.resourceLocation;
		auto editorResourcePath = [&](std::filesystem::path const& relative)
		{ return (editorResourceLocation / relative).lexically_normal().string(); };
		auto minimalTemplatePath = editorResourcePath("shared/pbr/templates/Minimal.pipeline.yaml"),
		     shadowsTemplatePath = editorResourcePath("shared/pbr/templates/Shadows.pipeline.yaml"),
		     fullTemplatePath = editorResourcePath("shared/pbr/templates/Full.pipeline.yaml"),
		     emptyTemplatePath = editorResourcePath("shared/pbr/templates/Empty.pipeline.yaml");
		int windowWidth = 1440, windowHeight = 900;
		float recoverySeconds = 30.0f;
		bool smokeTest = false, gpuTests = false;
		std::string configurationWarning;
		try
		{
			std::ifstream config("PipelineEditor.cfg");
			std::string key;
			while (std::getline(config, key, '='))
			{
				std::string value;
				if (!std::getline(config, value))
					break;
				if (key == "width")
					windowWidth = std::max(640, std::stoi(value));
				else if (key == "height")
					windowHeight = std::max(480, std::stoi(value));
				else if (key == "recoverySeconds")
					recoverySeconds = std::max(5.0f, std::stof(value));
			}
		}
		catch (std::exception const& error)
		{
			configurationWarning =
			    "PipelineEditor.cfg contains invalid values; defaults were used where necessary.\n\n" +
			    std::string(error.what());
		}
		std::string startupPath;
		for (int argument = 1; argument < __argc; ++argument)
		{
			std::string value = __argv[argument];
			if (value == "--smoke-test")
				smokeTest = true;
			else if (value == "--gpu-tests")
				gpuTests = true;
			else if (value == "--width" && argument + 1 < __argc)
				windowWidth = std::max(640, std::stoi(__argv[++argument]));
			else if (value == "--height" && argument + 1 < __argc)
				windowHeight = std::max(480, std::stoi(__argv[++argument]));
			else if (value == "--recovery-seconds" && argument + 1 < __argc)
				recoverySeconds = std::max(5.0f, std::stof(__argv[++argument]));
			else if (!value.starts_with("--"))
				startupPath = value;
		}
		if (smokeTest)
		{
			runProcessFlowModelTests();
			runProcessFlowLayoutTests();
		}
		bool startupFromDefaultTemplate = startupPath.empty();
		if (startupFromDefaultTemplate)
			startupPath = fullTemplatePath;
		std::shared_ptr<PbrPipelineDocument> openDocument;
		std::shared_ptr<SceneDocument> openScene;
		std::string currentPath, scenePath;
		std::vector<std::string> recentPaths;
		{
			std::ifstream recent("PipelineEditor.recent.txt");
			std::string path;
			while (std::getline(recent, path))
				if (!path.empty() && std::find(recentPaths.begin(), recentPaths.end(), path) == recentPaths.end() &&
				    recentPaths.size() < 8)
					recentPaths.push_back(path);
		}
		std::string recentPersistenceError;
		auto writeRecent = [&]()
		{
			std::ofstream recent("PipelineEditor.recent.txt", std::ios::trunc);
			if (!recent)
			{
				recentPersistenceError = "Could not update PipelineEditor.recent.txt.";
				return false;
			}
			for (auto const& value : recentPaths)
				recent << value << '\n';
			if (!recent)
			{
				recentPersistenceError = "Could not update PipelineEditor.recent.txt.";
				return false;
			}
			recentPersistenceError.clear();
			return true;
		};
		auto rememberRecent = [&](std::string const& path)
		{
			recentPaths.erase(std::remove(recentPaths.begin(), recentPaths.end(), path), recentPaths.end());
			recentPaths.insert(recentPaths.begin(), path);
			if (recentPaths.size() > 8)
				recentPaths.resize(8);
			writeRecent();
		};
		bool recoveredDocument = false, recoveredScene = false;
		std::string startupError = configurationWarning;
		if (!startupPath.empty())
		{
			try
			{
				std::string pipelineLoadPath = startupPath;
				if (!startupFromDefaultTemplate && mpp::app::documentHasNewerRecovery(startupPath))
				{
					if (MessageBoxA(nullptr,
					                "A newer pipeline recovery document exists. Recover it?",
					                "PipelineEditor Recovery",
					                MB_YESNO | MB_ICONQUESTION) == IDYES)
					{
						pipelineLoadPath = mpp::app::documentRecoveryPath(startupPath).string();
						recoveredDocument = true;
					}
					else
						mpp::app::removeDocumentRecovery(startupPath);
				}
				std::shared_ptr<PbrPipelineDocument> candidateDocument;
				try
				{
					candidateDocument = std::make_shared<PbrPipelineDocument>(
					    resource_parsers::PbrPipelineDocumentLoader::fromFile(pipelineLoadPath));
				}
				catch (std::exception const& recoveryError)
				{
					if (!recoveredDocument)
						throw;
					auto message = "The pipeline recovery file could not be loaded and will be removed. The explicit "
					               "save will be opened instead.\n\n" +
					               std::string(recoveryError.what());
					MessageBoxA(nullptr, message.c_str(), "Recovery Failed", MB_OK | MB_ICONWARNING);
					mpp::app::removeDocumentRecovery(startupPath);
					recoveredDocument = false;
					candidateDocument = std::make_shared<PbrPipelineDocument>(
					    resource_parsers::PbrPipelineDocumentLoader::fromFile(startupPath));
				}
				std::shared_ptr<SceneDocument> candidateScene;
				std::string candidateScenePath;
				if (!candidateDocument->previewScene.empty())
				{
					candidateScenePath =
					    (std::filesystem::path(startupPath).parent_path() / candidateDocument->previewScene).string();
					std::string sceneLoadPath = candidateScenePath;
					if (!startupFromDefaultTemplate && mpp::app::documentHasNewerRecovery(candidateScenePath))
					{
						if (MessageBoxA(nullptr,
						                "A newer preview-scene recovery document exists. Recover it?",
						                "PipelineEditor Recovery",
						                MB_YESNO | MB_ICONQUESTION) == IDYES)
						{
							sceneLoadPath = mpp::app::documentRecoveryPath(candidateScenePath).string();
							recoveredScene = true;
						}
						else
							mpp::app::removeDocumentRecovery(candidateScenePath);
					}
					try
					{
						candidateScene =
						    std::make_shared<SceneDocument>(resource_parsers::SceneParser::fromFile(sceneLoadPath));
					}
					catch (std::exception const& recoveryError)
					{
						if (!recoveredScene)
							throw;
						auto message = "The scene recovery file could not be loaded and will be removed. The explicit "
						               "save will be opened instead.\n\n" +
						               std::string(recoveryError.what());
						MessageBoxA(nullptr, message.c_str(), "Recovery Failed", MB_OK | MB_ICONWARNING);
						mpp::app::removeDocumentRecovery(candidateScenePath);
						recoveredScene = false;
						candidateScene = std::make_shared<SceneDocument>(
						    resource_parsers::SceneParser::fromFile(candidateScenePath));
					}
				}
				openDocument = std::move(candidateDocument);
				openScene = std::move(candidateScene);
				scenePath = startupFromDefaultTemplate ? std::string() : std::move(candidateScenePath);
				currentPath =
				    (startupFromDefaultTemplate || openDocument->importedFromRenderGraph) ? std::string() : startupPath;
				if (!startupFromDefaultTemplate)
					rememberRecent(startupPath);
			}
			catch (std::exception const& error)
			{
				startupError += (startupError.empty() ? std::string() : std::string("\n\n"));
				startupError += "Could not open startup workspace '" + startupPath + "'.\n\n" + error.what() +
				                "\n\nThe editor will start without replacing or deleting the source document.";
				openDocument.reset();
				openScene.reset();
				currentPath.clear();
				scenePath.clear();
			}
		}
		if (!SDL_Init(SDL_INIT_VIDEO))
			throw std::runtime_error(SDL_GetError());
		SdlLifetime sdlLifetime;
		WindowSDL window("PBR Pipeline Editor");
		window.create(windowWidth, windowHeight, false, true);
		Logger logger;
		if (!logger.initialise("PipelineEditor.log", Logger::Level::Debug))
			throw std::runtime_error("Could not create editor log.");
		RenderSystem renderSystem(window.getWidth(), window.getHeight(), &logger, renderSystemOptions);
		ResourceManager resources(&renderSystem, &logger);
		resources.setImageLoadFunction(mpp::app::loadImageFile);
		renderSystem.createCoreResources(&resources);
		RenderGraphPassFactoryRegistry authoringRegistry;
		registerBuiltInRenderGraphPasses(authoringRegistry);
		ImGuiBackendData backend{};
		imGuiSetup(&renderSystem, &resources, &backend, true, editorResourcePath("pipeline-editor/fa-solid-900.ttf"));
		if (gpuTests)
		{
			// The GPU suite needs a live context, so this is the only place it can
			// run. Without a caller it is compiled and never executed.
			std::string suiteFailure;
			if (!mpp::runRenderGraphGpuTests(&renderSystem, &suiteFailure))
			{
				fprintf(stderr, "MPP-PIPELINE-CLI-005: render graph GPU tests failed: %s\n", suiteFailure.c_str());
				return 1;
			}
			fprintf(stderr, "Render graph GPU tests passed.\n");
			return 0;
		}
		InputManagerSDL input;
		TimerSDL timer;
		timer.reset();
		auto font = resources.getResource("__ImGui_Font__", true);
		auto provider = std::make_shared<ImGuiDataProvider>(std::vector<ResourcePtr>{font});
		BufferRenderer renderer(provider);
		resource_parsers::PbrPipelineRuntime pipelineRuntime(&renderSystem, &resources);
		SceneRuntime sceneRuntime(&renderSystem, &resources);
		auto scene = std::make_shared<Scene>(&renderSystem);
		scene->load();
		scene->setClearColour(Colour(0.094f, 0.106f, 0.125f));
		uint32_t viewportWidth = 960, viewportHeight = 720;
		scene->setViewport(0, 0, viewportWidth, viewportHeight);
		RenderTextureOptions diagnosticOptions;
		diagnosticOptions.colourType = TextureInternalType::UnsignedInteger;
		diagnosticOptions.colourNormalised = true;
		diagnosticOptions.colourBitSize = 8;
		diagnosticOptions.colourChannels = 4;
		auto diagnosticTarget = std::dynamic_pointer_cast<RenderTexture>(renderSystem.createRenderTexture(
		    "PipelineEditor.ImageDiagnostic", viewportWidth, viewportHeight, diagnosticOptions));
		auto diagnosticTexture = provider->registerTexture(diagnosticTarget);
		RenderTargetPtr diagnosticResolveTarget;
		bool diagnosticResolveDepth = false;
		auto camera = std::make_shared<Camera>(
		    glm::vec3(0, 3, 8), 0.0f, 0.0f, 0.0f, 60.0f, float(windowWidth) / float(windowHeight));
		glm::vec3 orbitTarget(0);
		float orbitYaw = 0, orbitPitch = 0, orbitDistance = 8;
		auto setOrbitView = [&](glm::vec3 position, glm::vec3 target)
		{
			orbitTarget = target;
			auto offset = position - target;
			orbitDistance = std::max(0.05f, glm::length(offset));
			orbitPitch = std::asin(std::clamp(offset.y / orbitDistance, -1.0f, 1.0f));
			orbitYaw = std::atan2(offset.x, offset.z);
			camera->setLookAt(position, target);
		};
		auto updateOrbitCamera = [&]()
		{
			auto cosine = std::cos(orbitPitch);
			glm::vec3 offset(std::sin(orbitYaw) * cosine, std::sin(orbitPitch), std::cos(orbitYaw) * cosine);
			camera->setLookAt(orbitTarget + offset * orbitDistance, orbitTarget);
		};
		if (openScene)
		{
			setOrbitView(openScene->camera.position, openScene->camera.target);
			camera->setFov(openScene->camera.fov);
			camera->setClipDistances(openScene->camera.nearPlane, openScene->camera.farPlane);
		}
		renderSystem.getOrCreateRenderPipeline("EditorUI");
		std::string activePipeline = "EditorUI", activeGraphResource, previewFailure, textureDiagnosticFailure;
		RenderTargetPtr activePreviewTarget, inspectedTarget;
		ImTextureID activePreviewTexture = 0;
		std::shared_ptr<PbrPipelineDocument> activePreviewDocument;
		bool inspectSelectedImage = false, debugEnvironmentCube = smokeTest;
		int inspectedVersion = 0, inspectedMip = 0, textureDiagnosticMode = 0;
		float diagnosticExposure = 1.0f, diagnosticDepthNear = 0.1f, diagnosticDepthFar = 100.0f, diagnosticDepthRangeScale = 99.9f;
		uint64_t previewEditSerial = 0;
		bool previewStale = false;
		ChangeFlag documentChangedSincePreview{false, &previewEditSerial};
		uint32_t runtimeGeneration = 0;
		// Only re-home the interactive orbit camera when the scene's *authored*
		// camera actually changed (a different scene opened, or its Camera fields
		// edited) -- not on every preview rebuild. installPreview runs on every
		// pipeline edit too (bloom toggles, adding passes, ...), and those pass
		// the same scene through unchanged; resetting the view on each of those
		// discarded whatever orbit/pan/zoom the user had set up interactively.
		std::optional<SceneCameraDocument> lastAppliedSceneCamera;
		auto installPreview = [&](std::shared_ptr<PbrPipelineDocument> const& previewDocument,
		                          std::shared_ptr<SceneDocument> const& previewScene)
		{
			DiagnosticBag previewDiagnostics;
			if (previewDocument)
			{
				previewDiagnostics = previewDocument->validate(renderSystem.getCaps());
				previewDiagnostics.append(previewDocument->validateOutputAntiAliasing(
				    renderSystem.getOptions().antiAliasing, &renderSystem.getCaps()));
			}
			if (!previewDocument || !previewDocument->graph || previewDiagnostics.hasErrors() ||
			    (previewScene && (previewScene->validate().hasErrors() ||
			                      previewScene->environmentBinding != previewDocument->environment.binding)))
			{
				previewStale = !activeGraphResource.empty();
				previewFailure = "Working pipeline or scene is invalid; retaining the last valid preview.";
				for (auto const& diagnostic : previewDiagnostics.getDiagnostics())
					if (diagnostic.severity == DiagnosticSeverity::Error)
					{
						previewFailure += " [" + diagnostic.code + "] " + diagnostic.message;
						break;
					}
				return false;
			}
			auto suffix = std::to_string(++runtimeGeneration), candidatePipeline = "EditorPreview." + suffix,
			     candidateGraph = "PipelineEditor.Graph." + suffix;
			bool graphDeclared = false, pipelineDeclared = false, workspacePrepared = false, activated = false;
			try
			{
				// PipelineEditor exposes graph images after the complete frame. Production
				// transient aliasing may reuse an intermediate image's storage after its
				// final consumer, making the Inspector show a later image instead. Retain
				// distinct preview storage so SceneEmissive/BloomExtract and other authored
				// intermediates remain inspectable after graph execution.
				for (uint32_t image = 0; image < previewDocument->graph->getImageCount(); ++image)
				{
					auto handle = GraphImageHandle{image, 0};
					auto desc = previewDocument->graph->getImageInfo(handle).desc;
					if (!desc.external && desc.transient) { desc.transient = false; previewDocument->graph->setImageDesc(handle, desc); }
				}
				auto graphStream = std::make_shared<RenderGraphStream>(&resources);
				graphStream->setGraph(previewDocument->graph);
				auto graphResource = resources.declareResource(candidateGraph, graphStream).first;
				graphDeclared = true;
				graphResource->load();
				graphResource->create();
				if (!pipelineRuntime.rebuild(clonePipeline(previewDocument), viewportWidth, viewportHeight))
				{
					std::string message = "Pipeline workspace rebuild failed.";
					for (auto const& diagnostic : pipelineRuntime.getDiagnostics().getDiagnostics())
						if (diagnostic.severity == DiagnosticSeverity::Error)
						{
							message += " " + diagnostic.message;
							break;
						}
					throw std::runtime_error(message);
				}
				workspacePrepared = true;
				RenderPipelineOptions previewOptions;
				previewOptions.mode = RenderPipelineMode::XmlGraphPbrForward;
				previewOptions.graphTemplate = graphResource;
				// Local PostEffectMaterial/etc. resources are registered by
				// PbrPipelineRuntime under its qualified root (see rebuild()), not
				// globally -- without this, FullscreenEffectPass's unqualified
				// fallback lookup can't find them (see demo-suite/PackageScene.cpp's
				// identical assignment for the same reason).
				previewOptions.resourceRoot = pipelineRuntime.getRootResource();
				previewOptions.graphImports = pipelineRuntime.getImports();
				previewOptions.outputs = previewDocument->outputs;
				previewOptions.environment = pipelineRuntime.getEnvironment();
				previewOptions.bloom.enabled = previewDocument->bloom.enabled;
				previewOptions.bloom.blurPasses = previewDocument->bloom.blurPasses;
				// The authored BloomExtract input is the isolated PBR emissive MRT,
				// not scene luminance. Preserve all emissive values, including white
				// (1.0), rather than subtracting BloomOptions' scene threshold of 1.0.
				previewOptions.bloom.useMrtEmissiveMask = previewDocument->bloom.enabled;
				previewOptions.bloom.threshold = 0.0f;
				previewOptions.ambientOcclusion = previewDocument->ambientOcclusion;
				previewOptions.debugEnvironmentCube = debugEnvironmentCube;
				if (previewScene && previewScene->getShadowLightIndex())
				{
					// SceneRuntime commits this shared domain only with a valid candidate.
					previewOptions.shadowDomain = "PipelineEditor.PreviewShadow";
				}
				auto candidatePreviewTarget = pipelineRuntime.getPresentationTarget();
				auto candidatePipelineObject =
				    renderSystem.getOrCreateRenderPipeline(candidatePipeline, previewOptions);
				candidatePipelineObject->setFlowTelemetryEnabled(true);
				pipelineDeclared = true;
				std::map<std::string, RenderTargetPtr> outputDestinations;
				for (auto const& output : previewDocument->outputs)
					for (uint32_t image = 0; image < previewDocument->graph->getImageCount(); ++image)
					{
						auto info = previewDocument->graph->getImageInfo({image, 0});
						if (info.name != output.image || !info.desc.external)
							continue;
						auto destination = previewOptions.graphImports.find(info.importName);
						if (destination != previewOptions.graphImports.end())
							outputDestinations.emplace(output.name, destination->second);
						break;
					}
				if (outputDestinations.size() == previewDocument->outputs.size())
					candidatePipelineObject->prepareOutputs(*previewDocument->graph, outputDestinations);
				if (previewScene)
				{
					std::map<std::string, ResourcePtr> particleEffectBindings;
					for (auto const& authored : previewScene->particleEffects)
						if (auto resource = pipelineRuntime.getResolvedResource(authored.effect))
							particleEffectBindings.emplace(authored.effect, std::move(resource));
					if (!sceneRuntime.rebuild(*previewScene,
					                          pipelineRuntime.getMaterialBindings(),
					                          pipelineRuntime.getInstanceOverrides(),
					                          previewDocument->environment.binding,
					                          previewOptions.shadowDomain,
					                          particleEffectBindings))
					{
						std::string message = "Preview scene rebuild failed.";
						for (auto const& diagnostic : sceneRuntime.getDiagnostics().getDiagnostics())
							if (diagnostic.severity == DiagnosticSeverity::Error)
							{
								message += " " + diagnostic.message;
								break;
							}
						throw std::runtime_error(message);
					}
					scene = sceneRuntime.getScene();
					scene->setClearColour(Colour(0.094f, 0.106f, 0.125f));
					scene->setViewport(0, 0, viewportWidth, viewportHeight);
					bool const sceneCameraChanged = !lastAppliedSceneCamera ||
					    lastAppliedSceneCamera->position != previewScene->camera.position ||
					    lastAppliedSceneCamera->target != previewScene->camera.target ||
					    lastAppliedSceneCamera->fov != previewScene->camera.fov ||
					    lastAppliedSceneCamera->nearPlane != previewScene->camera.nearPlane ||
					    lastAppliedSceneCamera->farPlane != previewScene->camera.farPlane;
					if (sceneCameraChanged)
					{
						setOrbitView(previewScene->camera.position, previewScene->camera.target);
						camera->markCut();
						camera->setFov(previewScene->camera.fov);
						camera->setClipDistances(previewScene->camera.nearPlane, previewScene->camera.farPlane);
					}
					lastAppliedSceneCamera = previewScene->camera;
				}
				auto candidatePreviewDocument = previewDocument;
				auto obsoletePipeline = activePipeline, obsoleteGraph = activeGraphResource;
				auto obsoleteTexture = activePreviewTexture;
				auto obsoletePreviewTarget = activePreviewTarget;
				if (obsoleteTexture)
					provider->unregisterTexture(obsoleteTexture);
				ImTextureID candidatePreviewTexture = 0;
				try
				{
					if (auto texture = std::dynamic_pointer_cast<RenderTexture>(candidatePreviewTarget))
						candidatePreviewTexture = provider->registerTexture(texture);
				}
				catch (...)
				{
					if (auto texture = std::dynamic_pointer_cast<RenderTexture>(obsoletePreviewTarget))
						activePreviewTexture = provider->registerTexture(texture);
					throw;
				}
				activePipeline = candidatePipeline;
				activated = true;
				activeGraphResource = candidateGraph;
				activePreviewTarget = candidatePreviewTarget;
				activePreviewDocument = std::move(candidatePreviewDocument);
				activePreviewTexture = candidatePreviewTexture;
				inspectedTarget.reset();
				diagnosticResolveTarget.reset();
				textureDiagnosticFailure.clear();
				previewStale = false;
				documentChangedSincePreview = false;
				previewFailure.clear();
				if (!obsoleteGraph.empty())
				{
					renderSystem.removeRenderPipeline(obsoletePipeline);
					try
					{
						resources.deleteResource(obsoleteGraph);
					}
					catch (std::exception const& cleanupError)
					{
						logger.warn("Obsolete preview graph cleanup was deferred: " + std::string(cleanupError.what()));
					}
				}
				pipelineRuntime.accept();
				return true;
			}
			catch (std::exception const& error)
			{
				auto failure = std::string(error.what());
				if (activated)
				{
					previewStale = false;
					documentChangedSincePreview = false;
					previewFailure = "Preview installed, but obsolete-generation cleanup was deferred: " + failure;
					logger.warn(previewFailure);
					return true;
				}
				if (pipelineDeclared)
					try
					{
						renderSystem.removeRenderPipeline(candidatePipeline);
					}
					catch (...)
					{
					}
				if (workspacePrepared)
					try
					{
						pipelineRuntime.rollback();
					}
					catch (std::exception const& cleanupError)
					{
						failure += " Cleanup also failed: " + std::string(cleanupError.what());
					}
				if (graphDeclared)
				{
					try
					{
						resources.deleteResource(candidateGraph);
					}
					catch (...)
					{
					}
				}
				previewStale = !activeGraphResource.empty();
				previewFailure = std::move(failure);
				fprintf(stderr, "Preview rebuild failed: %s\n", previewFailure.c_str());
				fflush(stderr);
				logger.error("Preview rebuild failed: " + previewFailure);
				return false;
			}
		};
		auto rebuildPreview = [&]()
		{
			return installPreview(clonePipeline(openDocument),
			                      openScene ? std::make_shared<SceneDocument>(*openScene) : nullptr);
		};
		rebuildPreview();
		int selectedPass = -1, selectedImage = -1, selectedImport = -1, selectedBinding = -1, selectedOverride = -1,
		    selectedModel = -1, selectedLocalResource = -1, selectedExternalResource = -1;
		ProcessFlowModelBuilder processFlowBuilder;
		ProcessFlowLayout processFlowLayout;
		ProcessFlowView processFlowView;
		ProcessFlowModel processFlowModel;
		RenderPipelineFlowSnapshotPtr sampledFlowSnapshot;
		ProcessFlowSampleGate processFlowSampleGate;
		auto flowSampleAcquired = std::chrono::steady_clock::time_point{};
		uint64_t processFlowEditSerial = UINT64_MAX;
		uint64_t sampledFlowSceneGeneration = 0, processFlowSceneGeneration = UINT64_MAX;
		std::string processFlowPipeline;
		mpp::app::CommandStack pipelineCommands(256), sceneCommands(256);
		bool lastEditScene = false;
		bool running = true, pipelineDirty = recoveredDocument || (startupFromDefaultTemplate && openDocument),
		     sceneDirty = recoveredScene || (startupFromDefaultTemplate && openScene), resetLayout = true,
		     showPreferences = false;
		float fps = 0, fpsTime = 0, recoveryTimer = 0;
		int frames = 0, smokeStableFrames = 0;
		std::deque<double> gpuFrameTimeSamples;
		double gpuFrameTimeSum = 0;
		std::string gpuFrameTimePipeline;
		std::string operationErrorTitle, operationErrorMessage;
		bool operationMessageIsSuccess = false, openOperationError = !startupError.empty();
		std::string gltfImportPath;
		std::vector<std::string> gltfImportMaterials;
		std::vector<bool> gltfImportSelected;
		bool openGltfImportDialog = false;
		if (openOperationError)
		{
			operationErrorTitle = startupPath.empty() ? "Startup Warning" : "Open Failed";
			operationErrorMessage = startupError;
		}
		else if (!recentPersistenceError.empty())
		{
			openOperationError = true;
			operationErrorTitle = "Recent Files Warning";
			operationErrorMessage = recentPersistenceError;
		}
		bool captureRequested = false;
		bool captureAndOpen = false;
		auto reportOperationError =
		    [&](std::string title, std::string action, std::string const& path, std::exception const& error)
		{
			operationMessageIsSuccess = false;
			operationErrorTitle = std::move(title);
			operationErrorMessage = std::move(action) +
			                        "\n\nPath: " + (path.empty() ? std::string("(not selected)") : path) + "\n\n" +
			                        error.what() + "\n\nThe active workspace and unsaved edits were preserved.";
			openOperationError = true;
		};
		auto ensureRenderDocSettings = [&]()
		{
			bool changed = false;
			auto renderDocLibrary = editorSettings.renderDocExecutable.parent_path() / "renderdoc.dll";
			if (!std::filesystem::is_regular_file(editorSettings.renderDocExecutable) ||
			    !std::filesystem::is_regular_file(renderDocLibrary))
			{
				auto selected = mpp::app::openExecutableFileDialog(window.getWindow(), "Locate qrenderdoc.exe");
				if (!selected)
					return false;
				editorSettings.renderDocExecutable = mpp::app::normaliseDocumentPath(*selected);
				changed = true;
			}
			if (!std::filesystem::is_directory(editorSettings.captureDirectory))
			{
				auto selected = mpp::app::selectFolderDialog(window.getWindow(), "Select RenderDoc Capture Directory");
				if (!selected)
					return false;
				editorSettings.captureDirectory = mpp::app::normaliseDocumentPath(*selected);
				changed = true;
			}
			if (changed)
				saveRenderDocSettings(editorSettings);
			return true;
		};
		mpp::app::BackgroundJobQueue backgroundJobs;
		mpp::app::BackgroundFileWatcher fileWatcher;
		uint64_t observedEditSerial = 0, scheduledEditSerial = 0, activeBackgroundGeneration = 0;
		auto lastPreviewEdit = std::chrono::steady_clock::now();
		bool gpuInstallationPending = false;
		std::map<std::string, mpp::app::DocumentFileRevision> trackedFiles;
		std::vector<std::string> externalConflicts;
		auto workspaceFilePaths = [&]()
		{
			std::vector<std::filesystem::path> paths;
			if (openDocument)
				paths = workspaceDependencies(*openDocument, openScene.get(), currentPath, scenePath);
			return paths;
		};
		auto refreshTrackedFiles = [&]()
		{
			trackedFiles.clear();
			auto paths = workspaceFilePaths();
			for (auto const& path : paths)
				trackedFiles[path.string()] = mpp::app::captureDocumentFileRevision(path);
			fileWatcher.setFiles(paths);
			fileWatcher.acknowledgeAll();
			externalConflicts.clear();
		};
		auto updateTrackedDependencies = [&]()
		{
			auto paths = workspaceFilePaths();
			fileWatcher.setFiles(paths);
			std::set<std::string> retained;
			for (auto const& path : paths)
			{
				auto name = path.string();
				retained.insert(name);
				if (!trackedFiles.contains(name))
					trackedFiles[name] = mpp::app::captureDocumentFileRevision(path);
			}
			for (auto iterator = trackedFiles.begin(); iterator != trackedFiles.end();)
				if (!retained.contains(iterator->first))
					iterator = trackedFiles.erase(iterator);
				else
					++iterator;
		};
		auto acknowledgeTrackedFile = [&](std::string const& path)
		{
			if (path.empty())
				return;
			auto normalised = mpp::app::normaliseDocumentPath(path).string();
			trackedFiles[normalised] = mpp::app::captureDocumentFileRevision(normalised);
			fileWatcher.acknowledge(normalised);
			externalConflicts.erase(std::remove(externalConflicts.begin(), externalConflicts.end(), normalised),
			                        externalConflicts.end());
		};
		auto cleanupWorkspaceRecovery = [&]()
		{
			mpp::app::removeDocumentRecovery(currentPath);
			mpp::app::removeDocumentRecovery(scenePath);
		};
		auto confirmDiscardWorkspace = [&]()
		{
			if (pipelineDirty &&
			    MessageBoxA(
			        nullptr, "Discard unsaved pipeline changes?", "PipelineEditor", MB_YESNO | MB_ICONWARNING) != IDYES)
				return false;
			if (sceneDirty && MessageBoxA(nullptr,
			                              "Discard unsaved preview-scene changes?",
			                              "PipelineEditor",
			                              MB_YESNO | MB_ICONWARNING) != IDYES)
				return false;
			return true;
		};
		auto loadWorkspace = [&](std::string const& requestedPath, bool offerRecovery, bool addToRecent = true)
		{
			try
			{
				auto path = mpp::app::normaliseDocumentPath(requestedPath).string();
				bool pipelineRecovered = false, sceneRecovered = false;
				std::string pipelineLoadPath = path;
				if (offerRecovery && mpp::app::documentHasNewerRecovery(path))
				{
					if (MessageBoxA(nullptr,
					                "A newer pipeline recovery document exists. Recover it?",
					                "PipelineEditor Recovery",
					                MB_YESNO | MB_ICONQUESTION) == IDYES)
					{
						pipelineLoadPath = mpp::app::documentRecoveryPath(path).string();
						pipelineRecovered = true;
					}
					else
						mpp::app::removeDocumentRecovery(path);
				}
				std::shared_ptr<PbrPipelineDocument> candidateDocument;
				try
				{
					candidateDocument = std::make_shared<PbrPipelineDocument>(
					    resource_parsers::PbrPipelineDocumentLoader::fromFile(pipelineLoadPath));
				}
				catch (std::exception const& recoveryError)
				{
					if (!pipelineRecovered)
						throw;
					mpp::app::removeDocumentRecovery(path);
					pipelineRecovered = false;
					operationErrorTitle = "Pipeline Recovery Failed";
					operationErrorMessage =
					    "The recovery file was invalid and was removed. The explicit save was loaded instead.\n\n" +
					    std::string(recoveryError.what());
					openOperationError = true;
					candidateDocument = std::make_shared<PbrPipelineDocument>(
					    resource_parsers::PbrPipelineDocumentLoader::fromFile(path));
				}
				std::shared_ptr<SceneDocument> candidateScene;
				std::string candidateScenePath;
				if (!candidateDocument->previewScene.empty())
				{
					candidateScenePath = mpp::app::normaliseDocumentPath(std::filesystem::path(path).parent_path() /
					                                                     candidateDocument->previewScene)
					                         .string();
					std::string sceneLoadPath = candidateScenePath;
					if (offerRecovery && mpp::app::documentHasNewerRecovery(candidateScenePath))
					{
						if (MessageBoxA(nullptr,
						                "A newer preview-scene recovery document exists. Recover it?",
						                "PipelineEditor Recovery",
						                MB_YESNO | MB_ICONQUESTION) == IDYES)
						{
							sceneLoadPath = mpp::app::documentRecoveryPath(candidateScenePath).string();
							sceneRecovered = true;
						}
						else
							mpp::app::removeDocumentRecovery(candidateScenePath);
					}
					try
					{
						candidateScene =
						    std::make_shared<SceneDocument>(resource_parsers::SceneParser::fromFile(sceneLoadPath));
					}
					catch (std::exception const& recoveryError)
					{
						if (!sceneRecovered)
							throw;
						mpp::app::removeDocumentRecovery(candidateScenePath);
						sceneRecovered = false;
						operationErrorTitle = "Scene Recovery Failed";
						operationErrorMessage =
						    "The recovery file was invalid and was removed. The explicit save was loaded instead.\n\n" +
						    std::string(recoveryError.what());
						openOperationError = true;
						candidateScene = std::make_shared<SceneDocument>(
						    resource_parsers::SceneParser::fromFile(candidateScenePath));
					}
				}
				cleanupWorkspaceRecovery();
				openDocument = std::move(candidateDocument);
				openScene = std::move(candidateScene);
				scenePath = std::move(candidateScenePath);
				currentPath = openDocument->importedFromRenderGraph ? std::string() : path;
				if (addToRecent)
					rememberRecent(path);
				selectedPass = selectedImage = selectedImport = selectedBinding = selectedOverride = selectedModel =
				    selectedLocalResource = selectedExternalResource = -1;
				pipelineCommands.clear();
				sceneCommands.clear();
				pipelineDirty = pipelineRecovered || currentPath.empty();
				sceneDirty = sceneRecovered;
				documentChangedSincePreview = true;
				refreshTrackedFiles();
				if (!recentPersistenceError.empty())
				{
					operationErrorTitle = "Recent Files Warning";
					operationErrorMessage = recentPersistenceError;
					openOperationError = true;
				}
				return true;
			}
			catch (std::exception const& error)
			{
				reportOperationError("Open Failed",
				                     "Could not open the complete pipeline and preview-scene workspace.",
				                     requestedPath,
				                     error);
				return false;
			}
		};
		auto confirmExternalOverwrite = [&](std::string const& path)
		{
			if (path.empty())
				return true;
			auto normalised = mpp::app::normaliseDocumentPath(path).string();
			auto found = trackedFiles.find(normalised);
			if (found == trackedFiles.end() || !mpp::app::documentFileChanged(normalised, found->second))
				return true;
			auto message = "The file changed outside PipelineEditor:\n\n" + normalised +
			               "\n\nOverwrite the external version with the editor version?";
			return MessageBoxA(nullptr, message.c_str(), "External Change Conflict", MB_YESNO | MB_ICONWARNING) ==
			       IDYES;
		};
		auto confirmInvalidPipelineSave = [&]()
		{
			auto diagnostics = openDocument->validate(renderSystem.getCaps());
			diagnostics.append(openDocument->validateOutputAntiAliasing(renderSystem.getOptions().antiAliasing,
			                                                            &renderSystem.getCaps()));
			diagnostics.append(resource_parsers::validatePbrPipelineResourceDefinitions(*openDocument));
			if (!diagnostics.hasErrors())
				return true;
			auto message = "The pipeline has " + std::to_string(diagnostics.count(DiagnosticSeverity::Error)) +
			               " validation error(s). Save the invalid working document anyway?";
			return MessageBoxA(nullptr, message.c_str(), "Save Invalid Pipeline", MB_YESNO | MB_ICONWARNING) == IDYES;
		};
		auto confirmInvalidSceneSave = [&]()
		{
			auto diagnostics = openScene->validate();
			if (!diagnostics.hasErrors())
				return true;
			auto message = "The preview scene has " + std::to_string(diagnostics.count(DiagnosticSeverity::Error)) +
			               " validation error(s). Save the invalid working document anyway?";
			return MessageBoxA(nullptr, message.c_str(), "Save Invalid Scene", MB_YESNO | MB_ICONWARNING) == IDYES;
		};
		auto saveScene = [&](bool forceSaveAs)
		{
			if (!openScene)
				return true;
			std::string target = scenePath;
			if (forceSaveAs || target.empty())
			{
				auto selected =
				    mpp::app::saveXmlFileDialog(window.getWindow(), "Save Preview Scene", "preview.scene.yaml");
				if (!selected)
					return false;
				target = mpp::app::normaliseDocumentPath(*selected).string();
			}
			if (!confirmInvalidSceneSave() || !confirmExternalOverwrite(target))
				return false;
			auto oldPath = scenePath;
			try
			{
				resource_parsers::SceneSerializer::toFile(*openScene, target);
				scenePath = target;
				openScene->sourcePath = target;
				sceneCommands.markSavePoint();
				sceneDirty = false;
				mpp::app::removeDocumentRecovery(oldPath);
				mpp::app::removeDocumentRecovery(target);
				if (!oldPath.empty() && oldPath != target)
					trackedFiles.erase(mpp::app::normaliseDocumentPath(oldPath).string());
				acknowledgeTrackedFile(target);
				updateTrackedDependencies();
				if (openDocument)
				{
					auto reference = currentPath.empty() ? std::filesystem::path(target)
					                                     : mpp::app::makeDocumentRelativeReference(currentPath, target);
					if (openDocument->previewScene != reference.generic_string())
					{
						auto before = clonePipeline(openDocument);
						openDocument->previewScene = reference.generic_string();
						auto after = clonePipeline(openDocument);
						pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
						    "Update Preview Scene Path", &openDocument, before, after));
						pipelineDirty = true;
						documentChangedSincePreview = true;
					}
				}
				return true;
			}
			catch (std::exception const& error)
			{
				reportOperationError("Save Scene Failed", "Could not save the preview scene.", target, error);
				return false;
			}
		};
		auto savePipeline = [&](bool forceSaveAs)
		{
			if (!openDocument)
				return true;
			std::string target = currentPath;
			if (forceSaveAs || target.empty())
			{
				auto selected = mpp::app::saveXmlFileDialog(window.getWindow(), "Save PBR Pipeline", "pipeline.yaml");
				if (!selected)
					return false;
				target = mpp::app::normaliseDocumentPath(*selected).string();
			}
			if (!confirmInvalidPipelineSave() || !confirmExternalOverwrite(target))
				return false;
			auto oldPath = currentPath, oldPreview = openDocument->previewScene;
			auto oldLibraries = openDocument->resourceLibraries;
			if (!scenePath.empty())
				openDocument->previewScene =
				    mpp::app::makeDocumentRelativeReference(target, scenePath).generic_string();
			if (target != oldPath)
			{
				auto sourceDocument =
				    oldPath.empty() ? std::filesystem::path(openDocument->sourcePath) : std::filesystem::path(oldPath);
				if (!sourceDocument.empty())
					for (auto& library : openDocument->resourceLibraries)
						library = mpp::app::makeDocumentRelativeReference(
						              target, mpp::app::resolveDocumentReference(sourceDocument, library))
						              .generic_string();
			}
			try
			{
				resource_parsers::PbrPipelineSerializer::toFile(*openDocument, target);
				currentPath = target;
				openDocument->sourcePath = target;
				rememberRecent(target);
				pipelineCommands.markSavePoint();
				pipelineDirty = false;
				mpp::app::removeDocumentRecovery(oldPath);
				mpp::app::removeDocumentRecovery(target);
				if (!oldPath.empty() && oldPath != target)
					trackedFiles.erase(mpp::app::normaliseDocumentPath(oldPath).string());
				acknowledgeTrackedFile(target);
				updateTrackedDependencies();
				if (!recentPersistenceError.empty())
				{
					operationErrorTitle = "Recent Files Warning";
					operationErrorMessage = recentPersistenceError;
					openOperationError = true;
				}
				return true;
			}
			catch (std::exception const& error)
			{
				openDocument->previewScene = oldPreview;
				openDocument->resourceLibraries = std::move(oldLibraries);
				reportOperationError("Save Pipeline Failed", "Could not save the pipeline.", target, error);
				return false;
			}
		};
		auto queueWorkingPreview = [&](std::string label)
		{
			if (!openDocument)
				return;
			auto pipeline = clonePipeline(openDocument);
			auto previewScene = openScene ? std::make_shared<SceneDocument>(*openScene) : nullptr;
			auto pipelineFile = currentPath, sceneFile = scenePath;
			observedEditSerial = previewEditSerial;
			scheduledEditSerial = previewEditSerial;
			lastPreviewEdit = std::chrono::steady_clock::now();
			logger.info("Queued background workspace validation generation " +
			            std::to_string(backgroundJobs.currentGeneration() + 1) + ".");
			activeBackgroundGeneration = backgroundJobs.submit(
			    std::move(label),
			    [pipeline = std::move(pipeline),
			     previewScene = std::move(previewScene),
			     pipelineFile = std::move(pipelineFile),
			     sceneFile = std::move(sceneFile)](mpp::app::BackgroundCancellationToken const& cancellation,
			                                       mpp::app::BackgroundJobQueue::ProgressCallback const& progress)
			    {
				    return std::any(
				        preparePreview(pipeline, previewScene, pipelineFile, sceneFile, false, cancellation, progress));
			    });
		};
		auto forceWorkingPreviewRebuild = [&]()
		{
			// installPreview keeps the active generation renderable until its
			// replacement succeeds. A forced action additionally invalidates derived
			// HDR IBL outputs so changed EXR sources cannot remain cached.
			renderSystem.getIblEnvironmentCache().clear();
			queueWorkingPreview("Forced preview resource rebuild");
		};
		auto regenerateAntiAliasingOnly = [&]()
		{
			if (!openDocument || !activePreviewDocument || activeGraphResource.empty())
				return false;
			auto candidateDocument = clonePipeline(activePreviewDocument);
			candidateDocument->outputs = openDocument->outputs;
			auto diagnostics = candidateDocument->validateOutputAntiAliasing(renderSystem.getOptions().antiAliasing,
			                                                                 &renderSystem.getCaps());
			if (diagnostics.hasErrors())
			{
				previewStale = true;
				previewFailure = "Anti-aliasing change is invalid; retaining the last valid preview.";
				for (auto const& diagnostic : diagnostics.getDiagnostics())
					if (diagnostic.severity == DiagnosticSeverity::Error)
					{
						previewFailure += " [" + diagnostic.code + "] " + diagnostic.message;
						break;
					}
				return false;
			}
			auto obsoletePipeline = activePipeline,
			     candidatePipeline = "EditorPreview.AA." + std::to_string(++runtimeGeneration);
			bool declared = false;
			try
			{
				auto options = renderSystem.getRenderPipeline(obsoletePipeline)->getOptions();
				options.outputs = candidateDocument->outputs;
				auto candidate = renderSystem.getOrCreateRenderPipeline(candidatePipeline, options);
				candidate->setFlowTelemetryEnabled(true);
				declared = true;
				std::map<std::string, RenderTargetPtr> destinations;
				for (auto const& output : candidateDocument->outputs)
					for (uint32_t image = 0; image < candidateDocument->graph->getImageCount(); ++image)
					{
						auto info = candidateDocument->graph->getImageInfo({image, 0});
						if (info.name != output.image || !info.desc.external)
							continue;
						auto destination = options.graphImports.find(info.importName);
						if (destination != options.graphImports.end())
							destinations.emplace(output.name, destination->second);
						break;
					}
				if (destinations.size() != candidateDocument->outputs.size())
					throw std::runtime_error("Could not resolve every named anti-aliasing output destination.");
				candidate->prepareOutputs(*candidateDocument->graph, destinations);
				activePipeline = candidatePipeline;
				activePreviewDocument = std::move(candidateDocument);
				previewStale = false;
				previewFailure.clear();
				documentChangedSincePreview = false;
				try
				{
					renderSystem.removeRenderPipeline(obsoletePipeline);
				}
				catch (std::exception const& cleanupError)
				{
					logger.warn("Obsolete anti-aliasing pipeline cleanup was deferred: " +
					            std::string(cleanupError.what()));
				}
				logger.info("Regenerated anti-aliasing output processing without rebuilding the scene or camera.");
				return true;
			}
			catch (std::exception const& error)
			{
				if (declared)
					try
					{
						renderSystem.removeRenderPipeline(candidatePipeline);
					}
					catch (...)
					{
					}
				previewStale = true;
				previewFailure =
				    "Anti-aliasing regeneration failed; scene, camera, and prior pipeline were retained: " +
				    std::string(error.what());
				logger.error(previewFailure);
				return false;
			}
		};
		if (smokeTest && openDocument && !openDocument->outputs.empty() && !activeGraphResource.empty())
		{
			auto originalOutputs = openDocument->outputs;
			auto sceneBefore = scene;
			auto cameraPosition = camera->getPosition(), cameraDirection = camera->getDirection();
			auto effective =
			    resolveAntiAliasing(renderSystem.getOptions().antiAliasing, openDocument->outputs.front().antiAliasing);
			for (auto& output : openDocument->outputs)
				output.antiAliasing.fxaa = !effective.fxaa;
			if (!regenerateAntiAliasingOnly() || scene != sceneBefore || camera->getPosition() != cameraPosition ||
			    camera->getDirection() != cameraDirection)
				throw std::runtime_error("Anti-aliasing-only regeneration did not preserve scene/camera state.");
			openDocument->outputs = std::move(originalOutputs);
			if (!regenerateAntiAliasingOnly())
				throw std::runtime_error("Could not restore smoke-test anti-aliasing settings.");
		}
		std::vector<std::string> hotReloadFiles;
		auto queueHotReload = [&](std::vector<std::string> changedFiles)
		{
			if (currentPath.empty() || pipelineDirty || sceneDirty)
				return;
			auto pipelineFile = currentPath;
			hotReloadFiles = std::move(changedFiles);
			logger.info("Queued hot reload for " + std::to_string(hotReloadFiles.size()) +
			            " changed dependency file(s).");
			activeBackgroundGeneration = backgroundJobs.submit(
			    "Hot reload",
			    [pipelineFile](mpp::app::BackgroundCancellationToken const& cancellation,
			                   mpp::app::BackgroundJobQueue::ProgressCallback const& progress)
			    {
				    progress(0.05f, "Reading pipeline XML");
				    auto pipeline = std::make_shared<PbrPipelineDocument>(
				        resource_parsers::PbrPipelineDocumentLoader::fromFile(pipelineFile));
				    cancellation.throwIfCancelled();
				    std::shared_ptr<SceneDocument> scene;
				    std::string sceneFile;
				    if (!pipeline->previewScene.empty())
				    {
					    sceneFile = mpp::app::normaliseDocumentPath(std::filesystem::path(pipelineFile).parent_path() /
					                                                pipeline->previewScene)
					                    .string();
					    progress(0.25f, "Reading preview-scene XML");
					    scene = std::make_shared<SceneDocument>(resource_parsers::SceneParser::fromFile(sceneFile));
				    }
				    cancellation.throwIfCancelled();
				    return std::any(preparePreview(
				        std::move(pipeline), std::move(scene), pipelineFile, sceneFile, true, cancellation, progress));
			    });
		};
		refreshTrackedFiles();
		while (running)
		{
			float dt = timer.getDeltaTime();
			fpsTime += dt;
			recoveryTimer += dt;
			++frames;
			if (fpsTime >= 0.5f)
			{
				fps = frames / fpsTime;
				frames = 0;
				fpsTime = 0;
			}
			auto now = std::chrono::steady_clock::now();
			if (previewEditSerial != observedEditSerial)
			{
				observedEditSerial = previewEditSerial;
				lastPreviewEdit = now;
				backgroundJobs.cancel();
				activeBackgroundGeneration = backgroundJobs.currentGeneration();
				gpuInstallationPending = false;
			}
			if (documentChangedSincePreview && openDocument && scheduledEditSerial != previewEditSerial &&
			    std::chrono::duration<float>(now - lastPreviewEdit).count() >= 0.35f)
				queueWorkingPreview("Validating edited workspace");
			mpp::app::BackgroundJobResult backgroundResult;
			while (backgroundJobs.poll(backgroundResult))
			{
				if (backgroundResult.generation != activeBackgroundGeneration ||
				    backgroundResult.generation != backgroundJobs.currentGeneration() || backgroundResult.cancelled)
					continue;
				if (!backgroundResult.error.empty())
				{
					previewStale = !activeGraphResource.empty();
					previewFailure = backgroundResult.label + " failed: " + backgroundResult.error;
					logger.error(previewFailure);
					if (backgroundResult.label == "Hot reload")
						for (auto const& path : hotReloadFiles)
							if (std::find(externalConflicts.begin(), externalConflicts.end(), path) ==
							    externalConflicts.end())
								externalConflicts.push_back(path);
					continue;
				}
				try
				{
					auto prepared = std::any_cast<PreparedPreview>(std::move(backgroundResult.value));
					if (!prepared.validationFailure.empty())
					{
						previewStale = !activeGraphResource.empty();
						previewFailure = prepared.validationFailure;
						if (prepared.hotReload)
							for (auto const& path : hotReloadFiles)
								if (std::find(externalConflicts.begin(), externalConflicts.end(), path) ==
								    externalConflicts.end())
									externalConflicts.push_back(path);
						continue;
					}
					if (prepared.hotReload && (pipelineDirty || sceneDirty))
					{
						previewStale = !activeGraphResource.empty();
						previewFailure = "Hot-reload result was rejected because the document became dirty.";
						continue;
					}
					std::vector<std::string> changedDuringBuild;
					for (auto const& [path, revision] : prepared.dependencyRevisions)
						if (mpp::app::documentFileChanged(path, revision))
							changedDuringBuild.push_back(path);
					if (!changedDuringBuild.empty())
					{
						logger.info(
						    "Rejected stale prepared generation because dependencies changed during background work.");
						if (prepared.hotReload)
							queueHotReload(std::move(changedDuringBuild));
						else
						{
							documentChangedSincePreview = true;
							lastPreviewEdit = std::chrono::steady_clock::now();
						}
						continue;
					}
					gpuInstallationPending = true;
					bool installed = installPreview(prepared.pipeline, prepared.scene);
					gpuInstallationPending = false;
					if (!installed)
					{
						if (prepared.hotReload)
							for (auto const& path : hotReloadFiles)
								if (std::find(externalConflicts.begin(), externalConflicts.end(), path) ==
								    externalConflicts.end())
									externalConflicts.push_back(path);
						continue;
					}
					if (prepared.hotReload)
					{
						logger.info("Installed hot-reloaded workspace generation '" + prepared.pipeline->name +
						            "' on the render thread.");
						openDocument = std::move(prepared.pipeline);
						openScene = std::move(prepared.scene);
						currentPath = std::move(prepared.pipelinePath);
						scenePath = std::move(prepared.scenePath);
						pipelineCommands.clear();
						sceneCommands.clear();
						pipelineDirty = sceneDirty = false;
						selectedPass = selectedImage = selectedImport = selectedBinding = selectedOverride =
						    selectedModel = selectedLocalResource = selectedExternalResource = -1;
						refreshTrackedFiles();
						previewFailure.clear();
					}
					else
					{
						std::vector<std::filesystem::path> dependencies = prepared.dependencies;
						fileWatcher.setFiles(dependencies);
						std::set<std::string> retained;
						for (auto const& path : dependencies)
						{
							auto name = path.string();
							retained.insert(name);
							if (!trackedFiles.contains(name))
								trackedFiles[name] = mpp::app::captureDocumentFileRevision(path);
						}
						for (auto iterator = trackedFiles.begin(); iterator != trackedFiles.end();)
							if (!retained.contains(iterator->first))
								iterator = trackedFiles.erase(iterator);
							else
								++iterator;
					}
				}
				catch (std::exception const& error)
				{
					gpuInstallationPending = false;
					previewStale = !activeGraphResource.empty();
					previewFailure = "Could not install prepared background result: " + std::string(error.what());
				}
			}
			if (recoveryTimer >= recoverySeconds)
			{
				if (pipelineDirty && openDocument && !currentPath.empty())
					try
					{
						resource_parsers::PbrPipelineSerializer::toFile(
						    *openDocument, mpp::app::documentRecoveryPath(currentPath).string());
					}
					catch (std::exception const& error)
					{
						reportOperationError("Pipeline Recovery Failed",
						                     "Could not update the crash-recovery copy.",
						                     mpp::app::documentRecoveryPath(currentPath).string(),
						                     error);
					}
				if (sceneDirty && openScene && !scenePath.empty())
					try
					{
						resource_parsers::SceneSerializer::toFile(*openScene,
						                                          mpp::app::documentRecoveryPath(scenePath).string());
					}
					catch (std::exception const& error)
					{
						reportOperationError("Scene Recovery Failed",
						                     "Could not update the crash-recovery copy.",
						                     mpp::app::documentRecoveryPath(scenePath).string(),
						                     error);
					}
				recoveryTimer = 0;
			}
			auto watchedChanges = fileWatcher.poll();
			if (!watchedChanges.empty())
			{
				std::vector<std::string> changed;
				for (auto const& value : watchedChanges)
					changed.push_back(value.path.string());
				if (pipelineDirty || sceneDirty)
				{
					for (auto const& path : changed)
						if (std::find(externalConflicts.begin(), externalConflicts.end(), path) ==
						    externalConflicts.end())
							externalConflicts.push_back(path);
				}
				else
					queueHotReload(std::move(changed));
			}
			bool closeRequested = !window.processEvents(&input);
			imGuiHandleInput(&input, &backend);
			input.update();
			if (input.keyPressed(Key_Escape))
				closeRequested = true;
			auto captureWork = backgroundJobs.progress();
			bool captureEnabled =
			    !captureWork.queued && !captureWork.running && !gpuInstallationPending && !activeGraphResource.empty();
			if (closeRequested && confirmDiscardWorkspace())
			{
				cleanupWorkspaceRecovery();
				running = false;
			}
			if (window.getWidth() > 0 && window.getHeight() > 0 &&
			    ((size_t)window.getWidth() != renderSystem.getWindowWidth() ||
			     (size_t)window.getHeight() != renderSystem.getWindowHeight()))
			{
				renderSystem.setDisplay(window.getWidth(), window.getHeight());
				windowWidth = window.getWidth();
				windowHeight = window.getHeight();
			}
			imGuiNewFrame(window.getWindow(), &backend);
			ImGui::NewFrame();
			if (captureEnabled && ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_F12, ImGuiInputFlags_RouteGlobal))
			{
				captureRequested = true;
				captureAndOpen = true;
			}
			bool requestNew = false, requestOpen = false, requestSave = false, requestSaveAs = false,
			     requestSaveScene = false, requestSaveSceneAs = false, requestSaveAll = false,
			     requestExportPackage = false, requestExportLegacyPackage = false, requestUndo = false, requestRedo = false, requestDuplicate = false,
			     requestDelete = false, requestAddPopup = false, requestAutoOrder = false,
			     requestReloadConflicts = false, requestKeepConflicts = false, requestOverwriteConflicts = false,
			     requestValidateFocus = false, requestGltfImport = false;
			int addKind = -1;
			std::string addFactory;
			std::string requestedRecent;
			std::string newTemplatePath = fullTemplatePath;
			auto applyAntiAliasingOverride =
			    [&](std::string const& label, auto member, auto value, std::optional<size_t> outputIndex = std::nullopt)
			{
				if (!openDocument || openDocument->outputs.empty())
					return;
				auto work = backgroundJobs.progress();
				bool isolatedChange = !documentChangedSincePreview && !work.queued && !work.running &&
				                      !gpuInstallationPending && !activeGraphResource.empty();
				auto visibleLabel = label.substr(0, label.find("##"));
				auto before = clonePipeline(openDocument);
				if (outputIndex)
					openDocument->outputs[*outputIndex].antiAliasing.*member = value;
				else
					for (auto& output : openDocument->outputs)
						output.antiAliasing.*member = value;
				auto after = clonePipeline(openDocument);
				pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
				    "Change " + visibleLabel + " anti-aliasing", &openDocument, before, after));
				pipelineDirty = currentPath.empty() || pipelineCommands.dirty();
				lastEditScene = false;
				documentChangedSincePreview = true;
				if (isolatedChange)
					regenerateAntiAliasingOnly();
				else
					queueWorkingPreview("Regenerating after " + visibleLabel + " change");
			};
			if (ImGui::BeginMainMenuBar())
			{
				if (ImGui::BeginMenu("File"))
				{
					if (ImGui::BeginMenu("New"))
					{
						if (ImGui::MenuItem("Minimal PBR Pipeline", "Ctrl+N"))
						{
							requestNew = true;
							newTemplatePath = minimalTemplatePath;
						}
						if (ImGui::MenuItem("PBR Shadows Pipeline"))
						{
							requestNew = true;
							newTemplatePath = shadowsTemplatePath;
						}
						if (ImGui::MenuItem("Full PBR Pipeline"))
						{
							requestNew = true;
							newTemplatePath = fullTemplatePath;
						}
						if (ImGui::MenuItem("Empty Pipeline"))
						{
							requestNew = true;
							newTemplatePath = emptyTemplatePath;
						}
						ImGui::EndMenu();
					}
					if (ImGui::MenuItem("Open...", "Ctrl+O"))
						requestOpen = true;
					if (ImGui::MenuItem("Import glTF...", nullptr, false, openDocument != nullptr))
						requestGltfImport = true;
					if (!recentPaths.empty() && ImGui::BeginMenu("Open Recent"))
					{
						for (auto const& path : recentPaths)
							if (ImGui::MenuItem(path.c_str()))
								requestedRecent = path;
						ImGui::EndMenu();
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Save", "Ctrl+S", false, openDocument != nullptr))
						requestSave = true;
					if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, openDocument != nullptr))
						requestSaveAs = true;
					if (ImGui::MenuItem("Save Scene", nullptr, false, openScene != nullptr))
						requestSaveScene = true;
					if (ImGui::MenuItem("Save Scene As...", nullptr, false, openScene != nullptr))
						requestSaveSceneAs = true;
					if (ImGui::MenuItem("Save All", "Ctrl+Alt+S", false, openDocument != nullptr))
						requestSaveAll = true;
					{
						// Both formats require a valid source PBR pipeline: the Legacy
						// format is derived from it by conversion, so it can never be
						// more permissive than the Pbr export it builds on.
						bool exportEnabled = openDocument && openScene &&
						    openScene->environmentBinding == openDocument->environment.binding &&
						    !openDocument->validate(renderSystem.getCaps()).hasErrors() &&
						    !openDocument
						         ->validateOutputAntiAliasing(renderSystem.getOptions().antiAliasing,
						                                      &renderSystem.getCaps())
						         .hasErrors() &&
						    !resource_parsers::validatePbrPipelineResourceDefinitions(*openDocument).hasErrors() &&
						    !openScene->validate().hasErrors();
						if (ImGui::BeginMenu("Export Package", exportEnabled))
						{
							if (ImGui::MenuItem("Pbr Package...")) requestExportPackage = true;
							if (ImGui::MenuItem("Legacy Package...")) requestExportLegacyPackage = true;
							ImGui::EndMenu();
						}
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Exit") && confirmDiscardWorkspace())
					{
						cleanupWorkspaceRecovery();
						running = false;
					}
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Edit"))
				{
					auto& commands = lastEditScene ? sceneCommands : pipelineCommands;
					if (ImGui::MenuItem("Undo", "Ctrl+Z", false, commands.canUndo()))
						requestUndo = true;
					if (ImGui::MenuItem("Redo", "Ctrl+Y", false, commands.canRedo()))
						requestRedo = true;
					ImGui::Separator();
					if (ImGui::MenuItem("Preferences..."))
						showPreferences = true;
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Add"))
				{
					if (ImGui::MenuItem("Pass...", nullptr, false, openDocument && openDocument->graph))
						requestAddPopup = true;
					if (ImGui::MenuItem("Image", nullptr, false, openDocument && openDocument->graph))
						addKind = 1;
					if (ImGui::MenuItem("Typed Import", nullptr, false, openDocument != nullptr))
						addKind = 2;
					ImGui::Separator();
					if (ImGui::MenuItem("PBR Material", nullptr, false, openDocument != nullptr))
						addKind = 3;
					if (ImGui::MenuItem("Program", nullptr, false, openDocument != nullptr))
						addKind = 4;
					if (ImGui::MenuItem("Texture", nullptr, false, openDocument != nullptr))
						addKind = 5;
					if (ImGui::MenuItem("Sampler", nullptr, false, openDocument != nullptr))
						addKind = 6;
					if (ImGui::MenuItem("Post Effect Material", nullptr, false, openDocument != nullptr))
						addKind = 11;
					if (ImGui::MenuItem("Particle Effect", nullptr, false, openDocument != nullptr))
						addKind = 12;
					if (ImGui::MenuItem("Preview Binding", nullptr, false, openDocument != nullptr))
						addKind = 7;
					if (ImGui::MenuItem("Instance Override", nullptr, false, openDocument != nullptr))
						addKind = 8;
					ImGui::Separator();
					if (ImGui::MenuItem("Sphere Model", nullptr, false, openScene != nullptr))
						addKind = 9;
					if (ImGui::MenuItem("Directional Light", nullptr, false, openScene != nullptr))
						addKind = 10;
					if (ImGui::MenuItem("Particle Effect Instance", nullptr, false, openScene != nullptr))
						addKind = 13;
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Pipeline"))
				{
					if (ImGui::MenuItem("Validate"))
						requestValidateFocus = true;
					if (ImGui::MenuItem("Force Rebuild"))
						forceWorkingPreviewRebuild();
					if (ImGui::MenuItem(
					        "Auto-order Pass Dependencies", nullptr, false, openDocument && openDocument->graph))
						requestAutoOrder = true;
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("View"))
				{
					if (ImGui::MenuItem("Environment Cube", nullptr, &debugEnvironmentCube, openDocument != nullptr))
						renderSystem.getRenderPipeline(activePipeline)->setDebugEnvironmentCube(debugEnvironmentCube);
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Draw the active environment cubemap around the preview scene");
					ImGui::EndMenu();
				}
				if (ImGui::BeginMenu("Window"))
				{
					if (ImGui::MenuItem("Reset Layout"))
						resetLayout = true;
					ImGui::EndMenu();
				}
				ImGui::EndMainMenuBar();
			}
			auto toolbarHeight = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;
			if (ImGui::BeginViewportSideBar("##PipelineEditorToolbar",
			                                ImGui::GetMainViewport(),
			                                ImGuiDir_Up,
			                                toolbarHeight,
			                                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
			                                    ImGuiWindowFlags_NoSavedSettings))
			{
				auto toolbarButton = [&](char const* label, char const* tooltip, bool enabled = true)
				{
					if (!enabled)
						ImGui::BeginDisabled();
					bool pressed = ImGui::Button(label, ImVec2(ImGui::GetFrameHeight(), 0));
					if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
						ImGui::SetTooltip("%s", tooltip);
					if (!enabled)
						ImGui::EndDisabled();
					return pressed;
				};
				auto& activeCommands = lastEditScene ? sceneCommands : pipelineCommands;
				if (toolbarButton(ICON_FA_FILE "##ToolbarNew", "New workspace"))
					requestNew = true;
				ImGui::SameLine();
				if (toolbarButton(ICON_FA_FOLDER_OPEN "##ToolbarOpen", "Open pipeline"))
					requestOpen = true;
				ImGui::SameLine();
				if (toolbarButton(ICON_FA_SAVE "##ToolbarSaveAll", "Save all", openDocument != nullptr))
					requestSaveAll = true;
				ImGui::SameLine();
				if (toolbarButton(ICON_FA_UNDO "##ToolbarUndo", "Undo", activeCommands.canUndo()))
					requestUndo = true;
				ImGui::SameLine();
				if (toolbarButton(ICON_FA_REDO "##ToolbarRedo", "Redo", activeCommands.canRedo()))
					requestRedo = true;
				ImGui::SameLine();
				if (toolbarButton(ICON_FA_PLUS "##ToolbarAdd", "Add item", openDocument != nullptr))
					requestAddPopup = true;
				ImGui::SameLine();
				if (toolbarButton(ICON_FA_CLONE "##ToolbarDuplicate",
				                  "Duplicate selection",
				                  openDocument != nullptr || openScene != nullptr))
					requestDuplicate = true;
				ImGui::SameLine();
				if (toolbarButton(ICON_FA_TRASH "##ToolbarDelete",
				                  "Delete selection",
				                  openDocument != nullptr || openScene != nullptr))
					requestDelete = true;
				ImGui::SameLine();
				if (toolbarButton(
				        ICON_FA_CHECK_CIRCLE "##ToolbarValidate", "Show diagnostics", openDocument != nullptr))
					requestValidateFocus = true;
				ImGui::SameLine();
				if (toolbarButton(
				        ICON_FA_SYNC_ALT "##ToolbarRebuild", "Force rebuild preview resources", openDocument != nullptr))
					forceWorkingPreviewRebuild();
				ImGui::SameLine();
				if (toolbarButton(ICON_FA_CAMERA "##ToolbarCapture",
				                  "Capture viewport and open in RenderDoc (Ctrl+F12)",
				                  captureEnabled))
				{
					captureRequested = true;
					captureAndOpen = true;
				}
				ImGui::SameLine();
				if (!captureEnabled)
					ImGui::BeginDisabled();
				if (ImGui::ArrowButton("##ToolbarCaptureMenu", ImGuiDir_Down))
					ImGui::OpenPopup("RenderDoc Capture");
				if (!captureEnabled)
					ImGui::EndDisabled();
				if (ImGui::BeginPopup("RenderDoc Capture"))
				{
					if (ImGui::MenuItem("Capture Viewport"))
					{
						captureRequested = true;
						captureAndOpen = false;
					}
					if (ImGui::MenuItem("Capture Viewport and Open", "Ctrl+F12"))
					{
						captureRequested = true;
						captureAndOpen = true;
					}
					ImGui::EndPopup();
				}
				if (openDocument && !openDocument->outputs.empty())
				{
					ImGui::SameLine();
					ImGui::TextDisabled("|");
					ImGui::SameLine();
					ImGui::TextDisabled("Global AA");
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip(
						    "Initial values come from editor.ini; selections set every named presentation output.");
					auto const& iniDefaults = renderSystem.getOptions().antiAliasing;
					auto globalSampleCombo = [&](char const* label,
					                             std::optional<AntiAliasingSamples> AntiAliasingOverrides::* member,
					                             AntiAliasingSamples fallback)
					{
						auto value = (openDocument->outputs.front().antiAliasing.*member).value_or(fallback);
						bool mixed = std::any_of(openDocument->outputs.begin() + 1,
						                         openDocument->outputs.end(),
						                         [&](auto const& output)
						                         { return (output.antiAliasing.*member).value_or(fallback) != value; });
						int selected = mixed ? -1 : (int)value;
						char const* names[] = {"Off", "2x", "4x", "8x"};
						ImGui::SameLine();
						ImGui::SetNextItemWidth(76);
						if (ImGui::BeginCombo(label, mixed ? "Mixed" : names[selected]))
						{
							for (int option = 0; option < 4; ++option)
								if (ImGui::Selectable(names[option], !mixed && selected == option))
								{
									applyAntiAliasingOverride(
									    label, member, std::optional<AntiAliasingSamples>((AntiAliasingSamples)option));
								}
							ImGui::EndCombo();
						}
					};
					auto globalBooleanCombo =
					    [&](char const* label, std::optional<bool> AntiAliasingOverrides::* member, bool fallback)
					{
						auto value = (openDocument->outputs.front().antiAliasing.*member).value_or(fallback);
						bool mixed = std::any_of(openDocument->outputs.begin() + 1,
						                         openDocument->outputs.end(),
						                         [&](auto const& output)
						                         { return (output.antiAliasing.*member).value_or(fallback) != value; });
						int selected = mixed ? -1 : (value ? 1 : 0);
						char const* names[] = {"Off", "On"};
						ImGui::SameLine();
						ImGui::SetNextItemWidth(76);
						if (ImGui::BeginCombo(label, mixed ? "Mixed" : names[selected]))
						{
							for (int option = 0; option < 2; ++option)
								if (ImGui::Selectable(names[option], !mixed && selected == option))
								{
									applyAntiAliasingOverride(label, member, std::optional<bool>(option == 1));
								}
							ImGui::EndCombo();
						}
					};
					globalSampleCombo("MSAA##Toolbar", &AntiAliasingOverrides::msaa, iniDefaults.msaa);
					globalSampleCombo("SSAA##Toolbar", &AntiAliasingOverrides::ssaa, iniDefaults.ssaa);
					globalBooleanCombo("TAA##Toolbar", &AntiAliasingOverrides::taa, iniDefaults.taa);
					globalBooleanCombo("FXAA##Toolbar", &AntiAliasingOverrides::fxaa, iniDefaults.fxaa);
					ImGui::SameLine();
					auto bloomBefore = clonePipeline(openDocument);
					bool toolbarBloomEnabled = openDocument->bloom.enabled;
					if (ImGui::Checkbox("Bloom##Toolbar", &toolbarBloomEnabled))
					{
						openDocument->setBloomEnabled(toolbarBloomEnabled);
						auto after = clonePipeline(openDocument);
						pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
						                             "Toggle Bloom", &openDocument, bloomBefore, after), true);
						pipelineDirty = true;
						documentChangedSincePreview = true;
						lastEditScene = false;
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Enable or disable bloom in the preview pipeline");
					ImGui::SameLine();
					auto aoBefore = clonePipeline(openDocument);
					int toolbarAoMethod = (int)openDocument->ambientOcclusion.method;
					ImGui::SetNextItemWidth(90.0f);
					if (ImGui::Combo("AO##Toolbar", &toolbarAoMethod, "None\0SSAO\0GTAO\0"))
					{
						openDocument->setAmbientOcclusionMethod((AmbientOcclusionMethod)toolbarAoMethod);
						auto after = clonePipeline(openDocument);
						pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
						                             "Change Ambient Occlusion", &openDocument, aoBefore, after), true);
						pipelineDirty = true;
						documentChangedSincePreview = true;
						lastEditScene = false;
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Select the fixed ambient-occlusion method for the preview pipeline");
				}
				auto workProgress = backgroundJobs.progress();
				if (workProgress.queued || workProgress.running || gpuInstallationPending)
				{
					ImGui::SameLine();
					ImGui::TextDisabled("%s: %s",
					                    workProgress.label.c_str(),
					                    gpuInstallationPending ? "Installing GPU generation"
					                                           : workProgress.stage.c_str());
					ImGui::SameLine();
					ImGui::ProgressBar(gpuInstallationPending ? 1.0f : workProgress.fraction, ImVec2(150, 0));
				}
			}
			ImGui::End();
			ImGuiID dockspace = ImGui::GetID("PipelineEditor.Dockspace");
			ImGui::DockSpaceOverViewport(dockspace, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
			if (resetLayout || !ImGui::DockBuilderGetNode(dockspace) ||
			    !ImGui::DockBuilderGetNode(dockspace)->IsSplitNode())
			{
				resetLayout = false;
				ImGui::DockBuilderRemoveNode(dockspace);
				ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
				ImGui::DockBuilderSetNodeSize(dockspace, ImGui::GetMainViewport()->WorkSize);
				ImGuiID left, right;
				ImGui::DockBuilderSplitNode(dockspace, ImGuiDir_Left, 0.28f, &left, &right);
				ImGuiID leftLower, leftUpper;
				ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.48f, &leftLower, &leftUpper);
				ImGui::DockBuilderDockWindow("Pipeline Hierarchy", leftUpper);
				ImGui::DockBuilderDockWindow("Process Flow", leftUpper);
				ImGui::DockBuilderDockWindow("Inspector", leftLower);
				ImGui::DockBuilderDockWindow("Diagnostics", leftLower);
				ImGui::DockBuilderDockWindow("Allocations", leftLower);
				ImGui::DockBuilderDockWindow("Statistics", leftLower);
				ImGui::DockBuilderDockWindow("Viewport", right);
				ImGui::DockBuilderFinish(dockspace);
			}
			if (!externalConflicts.empty())
			{
				ImGui::Begin("External File Conflict");
				ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.15f, 1.0f), "Files changed outside PipelineEditor:");
				for (auto const& path : externalConflicts)
					ImGui::BulletText("%s", path.c_str());
				ImGui::TextWrapped("Reload discards editor changes. Keep Editor Version acknowledges disk changes and "
				                   "marks affected documents dirty. External libraries remain read-only.");
				if (ImGui::Button("Reload Workspace"))
					requestReloadConflicts = true;
				ImGui::SameLine();
				if (ImGui::Button("Overwrite Pipeline/Scene"))
					requestOverwriteConflicts = true;
				ImGui::SameLine();
				if (ImGui::Button("Keep Editor Version"))
					requestKeepConflicts = true;
				ImGui::End();
			}
			if (openOperationError)
			{
				ImGui::OpenPopup("Operation Result");
				openOperationError = false;
			}
			if (ImGui::BeginPopupModal("Operation Result", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::TextColored(operationMessageIsSuccess ? ImVec4(0.3f, 1, 0.4f, 1) : ImVec4(1, 0.35f, 0.35f, 1),
				                   "%s",
				                   operationErrorTitle.c_str());
				ImGui::Separator();
				ImGui::TextWrapped("%s", operationErrorMessage.c_str());
				if (ImGui::Button("Copy Details"))
					ImGui::SetClipboardText(operationErrorMessage.c_str());
				ImGui::SameLine();
				if (ImGui::Button("Close"))
					ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}
			if (requestAddPopup)
				ImGui::OpenPopup("Add Item");
			if (ImGui::BeginPopup("Add Item"))
			{
				if (openDocument && openDocument->graph && ImGui::BeginMenu("Pass"))
				{
					for (auto const& factory : authoringRegistry.getRegisteredMetadataNames())
					{
						auto metadata = authoringRegistry.findMetadata(factory);
						auto label = metadata && !metadata->displayName.empty()
						                 ? metadata->displayName + " (" + factory + ")"
						                 : factory;
						if (ImGui::MenuItem(label.c_str()))
						{
							addKind = 0;
							addFactory = factory;
						}
					}
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("Image", nullptr, false, openDocument && openDocument->graph))
					addKind = 1;
				if (ImGui::MenuItem("Typed Import", nullptr, false, openDocument != nullptr))
					addKind = 2;
				if (ImGui::BeginMenu("Local Resource", openDocument != nullptr))
				{
					if (ImGui::MenuItem("PBR Material"))
						addKind = 3;
					if (ImGui::MenuItem("Program"))
						addKind = 4;
					if (ImGui::MenuItem("Texture"))
						addKind = 5;
					if (ImGui::MenuItem("Sampler"))
						addKind = 6;
					if (ImGui::MenuItem("Post Effect Material"))
						addKind = 11;
					if (ImGui::MenuItem("Particle Effect"))
						addKind = 12;
					ImGui::EndMenu();
				}
				if (ImGui::MenuItem("Preview Binding", nullptr, false, openDocument != nullptr))
					addKind = 7;
				if (ImGui::MenuItem("Instance Override", nullptr, false, openDocument != nullptr))
					addKind = 8;
				if (ImGui::MenuItem("Sphere Model", nullptr, false, openScene != nullptr))
					addKind = 9;
				if (ImGui::MenuItem("Directional Light", nullptr, false, openScene != nullptr))
					addKind = 10;
				if (ImGui::MenuItem("Particle Effect Instance", nullptr, false, openScene != nullptr))
					addKind = 13;
				ImGui::EndPopup();
			}
			if (addKind >= 0)
			{
				if ((addKind <= 8 || addKind == 11 || addKind == 12) && openDocument)
				{
					auto before = clonePipeline(openDocument);
					if (addKind == 0 && openDocument->graph)
					{
						auto metadata = authoringRegistry.findMetadata(addFactory);
						auto base = metadata && !metadata->displayName.empty()
						                ? metadata->displayName
						                : addFactory.substr(addFactory.find_last_of('.') + 1);
						auto name =
						    uniqueName(base,
						               [&](auto const& value)
						               {
							               for (uint32_t pass = 0; pass < openDocument->graph->getPassCount(); ++pass)
								               if (openDocument->graph->getPassInfo({pass}).name == value)
									               return true;
							               return false;
						               });
						auto handle =
						    openDocument->graph->addPass(name, metadata ? metadata->type : GraphPassType::Scene);
						openDocument->graph->setPassCallbackFactory(handle, addFactory);
						selectedPass = (int)handle.id;
					}
					else if (addKind == 1 && openDocument->graph)
					{
						auto name = uniqueName(
						    "Image",
						    [&](auto const& value)
						    {
							    for (uint32_t image = 0; image < openDocument->graph->getImageCount(); ++image)
								    if (openDocument->graph->getImageInfo({image, 0}).name == value)
									    return true;
							    return false;
						    });
						GraphImageDesc desc;
						desc.format = GraphImageFormat::Rgba16f;
						desc.usage = GraphImageUsage::Sampled | GraphImageUsage::ColourAttachment;
						selectedImage = (int)openDocument->graph->createImage(name, desc).id;
						selectedPass = -1;
					}
					else if (addKind == 2)
					{
						auto id = uniqueName("import",
						                     [&](auto const& value)
						                     {
							                     return std::any_of(openDocument->imports.begin(),
							                                        openDocument->imports.end(),
							                                        [&](auto const& item) { return item.id == value; });
						                     });
						openDocument->imports.push_back(
						    {id, id, GraphImageFormat::Rgba8, GraphImageUsage::Sampled, true, ""});
						selectedImport = (int)openDocument->imports.size() - 1;
					}
					else if (addKind >= 3 && addKind <= 6)
					{
						auto kind = (PbrPipelineResourceKind)(addKind - 3);
						char const* bases[] = {"Material", "Program", "Texture", "Sampler"};
						auto name =
						    uniqueName(bases[addKind - 3],
						               [&](auto const& value)
						               {
							               return std::any_of(openDocument->localResources.begin(),
							                                  openDocument->localResources.end(),
							                                  [&](auto const& item) { return item.name == value; });
						               });
						openDocument->localResources.push_back(makeLocalResource(kind, name));
						selectedLocalResource = (int)openDocument->localResources.size() - 1;
					}
					else if (addKind == 7)
					{
						auto name =
						    uniqueName("MaterialBinding",
						               [&](auto const& value)
						               {
							               return std::any_of(openDocument->previewBindings.begin(),
							                                  openDocument->previewBindings.end(),
							                                  [&](auto const& item) { return item.binding == value; });
						               });
						std::string resource;
						for (auto const& value : openDocument->localResources)
							if (value.kind == PbrPipelineResourceKind::PbrMaterial)
							{
								resource = value.name;
								break;
							}
						openDocument->previewBindings.push_back({name, resource});
						selectedBinding = (int)openDocument->previewBindings.size() - 1;
					}
					else if (addKind == 8)
					{
						openDocument->previewOverrides.push_back({"Model", "MaterialBinding", {}});
						selectedOverride = (int)openDocument->previewOverrides.size() - 1;
					}
					else if (addKind == 11)
					{
						auto name =
						    uniqueName("PostEffect",
						               [&](auto const& value)
						               {
							               return std::any_of(openDocument->localResources.begin(),
							                                  openDocument->localResources.end(),
							                                  [&](auto const& item) { return item.name == value; });
						               });
						openDocument->localResources.push_back(
						    makeLocalResource(PbrPipelineResourceKind::PostEffectMaterial, name));
						selectedLocalResource = (int)openDocument->localResources.size() - 1;
					}
					else if (addKind == 12)
					{
						auto name = uniqueName("ParticleEffect", [&](auto const& value) { return std::any_of(openDocument->localResources.begin(), openDocument->localResources.end(), [&](auto const& item) { return item.name == value; }); });
						openDocument->localResources.push_back(makeLocalResource(PbrPipelineResourceKind::ParticleEffect, name));
						selectedLocalResource = (int)openDocument->localResources.size() - 1;
					}
					auto after = clonePipeline(openDocument);
					pipelineCommands.execute(
					    std::make_unique<PipelineSnapshotCommand>("Add Pipeline Item", &openDocument, before, after));
					pipelineDirty = true;
					lastEditScene = false;
					documentChangedSincePreview = true;
				}
				else if (openScene && (addKind == 9 || addKind == 10 || addKind == 13))
				{
					auto before = std::make_shared<SceneDocument>(*openScene);
					if (addKind == 9)
					{
						SceneModelDocument value;
						value.id = uniqueName("Sphere",
						                      [&](auto const& id)
						                      {
							                      return std::any_of(openScene->models.begin(),
							                                         openScene->models.end(),
							                                         [&](auto const& item) { return item.id == id; });
						                      });
						value.source = SceneModelSource::Sphere;
						value.layers = openScene->layers.empty() ? std::vector<std::string>{"Default"}
						                                         : std::vector<std::string>{openScene->layers.front()};
						if (openScene->layers.empty())
							openScene->layers.push_back("Default");
						if (openDocument && !openDocument->previewBindings.empty())
							value.materialBinding = openDocument->previewBindings.front().binding;
						openScene->models.push_back(value);
						selectedModel = (int)openScene->models.size() - 1;
					}
					else if (addKind == 10)
					{
						SceneLightDocument value;
						value.id = uniqueName("Light",
						                      [&](auto const& id)
						                      {
							                      return std::any_of(openScene->lights.begin(),
							                                         openScene->lights.end(),
							                                         [&](auto const& item) { return item.id == id; });
						                      });
						openScene->lights.push_back(value);
						selectedModel = -100 - (int)openScene->lights.size() + 1;
					}
					else
					{
						SceneParticleEffectDocument value;
						value.id = uniqueName("ParticleEffect", [&](auto const& id) { return std::any_of(openScene->particleEffects.begin(), openScene->particleEffects.end(), [&](auto const& item) { return item.id == id; }); });
						if (openDocument) for (auto const& resource : openDocument->localResources) if (resource.kind == PbrPipelineResourceKind::ParticleEffect) { value.effect = resource.name; break; }
						openScene->particleEffects.push_back(value);
						selectedModel = -10000 - (int)openScene->particleEffects.size() + 1;
					}
					auto after = std::make_shared<SceneDocument>(*openScene);
					sceneCommands.execute(
					    std::make_unique<SceneSnapshotCommand>("Add Scene Item", &openScene, before, after));
					sceneDirty = true;
					lastEditScene = true;
					documentChangedSincePreview = true;
				}
			}
			if (requestAutoOrder && openDocument && openDocument->graph)
			{
				auto result = openDocument->graph->buildDependencyOrder();
				if (result.valid)
				{
					std::vector<GraphPassHandle> order = result.passOrder;
					for (uint32_t pass = 0; pass < openDocument->graph->getPassCount(); ++pass)
						if (std::none_of(order.begin(), order.end(), [&](auto handle) { return handle.id == pass; }))
							order.push_back({pass});
					auto before = clonePipeline(openDocument);
					openDocument->graph->reorderPasses(order);
					auto after = clonePipeline(openDocument);
					documentChangedSincePreview = true;
					pipelineCommands.execute(
					    std::make_unique<PipelineSnapshotCommand>("Auto-order Passes", &openDocument, before, after));
					selectedPass = -1;
					lastEditScene = false;
					pipelineDirty = currentPath.empty() || pipelineCommands.dirty();
				}
				else
					previewFailure = result.diagnostics.empty() ? "Pass dependencies cannot be ordered."
					                                            : result.diagnostics.front();
			}
			if (requestUndo)
			{
				auto& commands = lastEditScene ? sceneCommands : pipelineCommands;
				if (commands.undo())
				{
					selectedPass = selectedImage = selectedImport = selectedBinding = selectedOverride = selectedModel =
					    selectedLocalResource = selectedExternalResource = -1;
					documentChangedSincePreview = true;
					if (lastEditScene)
						sceneDirty = scenePath.empty() || commands.dirty();
					else
						pipelineDirty = currentPath.empty() || commands.dirty();
				}
			}
			if (requestRedo)
			{
				auto& commands = lastEditScene ? sceneCommands : pipelineCommands;
				if (commands.redo())
				{
					selectedPass = selectedImage = selectedImport = selectedBinding = selectedOverride = selectedModel =
					    selectedLocalResource = selectedExternalResource = -1;
					documentChangedSincePreview = true;
					if (lastEditScene)
						sceneDirty = scenePath.empty() || commands.dirty();
					else
						pipelineDirty = currentPath.empty() || commands.dirty();
				}
			}
			if (requestDuplicate && openDocument)
			{
				auto before = clonePipeline(openDocument);
				bool changed = false;
				if (selectedPass >= 0 && openDocument->graph &&
				    (size_t)selectedPass < openDocument->graph->getPassCount())
				{
					auto source = openDocument->graph->getPassInfo({(uint32_t)selectedPass});
					auto name =
					    uniqueName(source.name + ".Copy",
					               [&](auto const& value)
					               {
						               for (uint32_t pass = 0; pass < openDocument->graph->getPassCount(); ++pass)
							               if (openDocument->graph->getPassInfo({pass}).name == value)
								               return true;
						               return false;
					               });
					selectedPass = (int)openDocument->graph->duplicatePass({(uint32_t)selectedPass}, name).id;
					changed = true;
				}
				else if (selectedImage >= 0 && openDocument->graph &&
				         (size_t)selectedImage < openDocument->graph->getImageCount())
				{
					auto source = openDocument->graph->getImageInfo({(uint32_t)selectedImage, 0});
					auto name =
					    uniqueName(source.name + ".Copy",
					               [&](auto const& value)
					               {
						               for (uint32_t image = 0; image < openDocument->graph->getImageCount(); ++image)
							               if (openDocument->graph->getImageInfo({image, 0}).name == value)
								               return true;
						               return false;
					               });
					source.desc.external = false;
					source.desc.transient = true;
					selectedImage = (int)openDocument->graph->createImage(name, source.desc).id;
					changed = true;
				}
				else if (selectedImport >= 0 && (size_t)selectedImport < openDocument->imports.size())
				{
					auto value = openDocument->imports[(size_t)selectedImport];
					value.id = uniqueName(value.id + ".Copy",
					                      [&](auto const& id)
					                      {
						                      return std::any_of(openDocument->imports.begin(),
						                                         openDocument->imports.end(),
						                                         [&](auto const& item) { return item.id == id; });
					                      });
					openDocument->imports.insert(openDocument->imports.begin() + selectedImport + 1, value);
					++selectedImport;
					changed = true;
				}
				else if (selectedLocalResource >= 0 &&
				         (size_t)selectedLocalResource < openDocument->localResources.size())
				{
					auto value = openDocument->localResources[(size_t)selectedLocalResource];
					auto name = uniqueName(value.name + ".Copy",
					                       [&](auto const& id)
					                       {
						                       return std::any_of(openDocument->localResources.begin(),
						                                          openDocument->localResources.end(),
						                                          [&](auto const& item) { return item.name == id; });
					                       });
					renameResource(value, name);
					openDocument->localResources.insert(
					    openDocument->localResources.begin() + selectedLocalResource + 1, value);
					++selectedLocalResource;
					changed = true;
				}
				else if (selectedBinding >= 0 && (size_t)selectedBinding < openDocument->previewBindings.size())
				{
					auto value = openDocument->previewBindings[(size_t)selectedBinding];
					value.binding =
					    uniqueName(value.binding + ".Copy",
					               [&](auto const& id)
					               {
						               return std::any_of(openDocument->previewBindings.begin(),
						                                  openDocument->previewBindings.end(),
						                                  [&](auto const& item) { return item.binding == id; });
					               });
					openDocument->previewBindings.insert(openDocument->previewBindings.begin() + selectedBinding + 1,
					                                     value);
					++selectedBinding;
					changed = true;
				}
				else if (selectedOverride >= 0 && (size_t)selectedOverride < openDocument->previewOverrides.size())
				{
					openDocument->previewOverrides.insert(openDocument->previewOverrides.begin() + selectedOverride + 1,
					                                      openDocument->previewOverrides[(size_t)selectedOverride]);
					++selectedOverride;
					changed = true;
				}
				if (changed)
				{
					auto after = clonePipeline(openDocument);
					pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
					    "Duplicate Pipeline Item", &openDocument, before, after));
					pipelineDirty = true;
					lastEditScene = false;
					documentChangedSincePreview = true;
				}
			}
			if (requestDelete && openDocument)
			{
				auto before = clonePipeline(openDocument);
				bool changed = false;
				if (selectedPass >= 0 && openDocument->graph &&
				    (size_t)selectedPass < openDocument->graph->getPassCount())
				{
					openDocument->graph->removePass({(uint32_t)selectedPass});
					selectedPass = -1;
					changed = true;
				}
				else if (selectedImage >= 0 && openDocument->graph &&
				         (size_t)selectedImage < openDocument->graph->getImageCount())
				{
					openDocument->graph->removeImage({(uint32_t)selectedImage, 0});
					selectedImage = -1;
					changed = true;
				}
				else if (selectedImport >= 0 && (size_t)selectedImport < openDocument->imports.size())
				{
					auto id = openDocument->imports[(size_t)selectedImport].id;
					if (openDocument->graph)
						for (uint32_t image = 0; image < openDocument->graph->getImageCount(); ++image)
							if (openDocument->graph->getImageInfo({image, 0}).importName == id)
								openDocument->graph->clearImageImportName({image, 0});
					openDocument->imports.erase(openDocument->imports.begin() + selectedImport);
					selectedImport = -1;
					changed = true;
				}
				else if (selectedLocalResource >= 0 &&
				         (size_t)selectedLocalResource < openDocument->localResources.size())
				{
					auto name = openDocument->localResources[(size_t)selectedLocalResource].name;
					openDocument->localResources.erase(openDocument->localResources.begin() + selectedLocalResource);
					removeResourceReferences(*openDocument, name);
					selectedLocalResource = -1;
					changed = true;
				}
				else if (selectedBinding >= 0 && (size_t)selectedBinding < openDocument->previewBindings.size())
				{
					openDocument->previewBindings.erase(openDocument->previewBindings.begin() + selectedBinding);
					selectedBinding = -1;
					changed = true;
				}
				else if (selectedOverride >= 0 && (size_t)selectedOverride < openDocument->previewOverrides.size())
				{
					openDocument->previewOverrides.erase(openDocument->previewOverrides.begin() + selectedOverride);
					selectedOverride = -1;
					changed = true;
				}
				if (changed)
				{
					auto after = clonePipeline(openDocument);
					pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
					    "Delete Pipeline Item", &openDocument, before, after));
					pipelineDirty = true;
					lastEditScene = false;
					documentChangedSincePreview = true;
				}
			}
			if (requestDuplicate && openScene && selectedModel >= 0 && (size_t)selectedModel < openScene->models.size())
			{
				auto before = std::make_shared<SceneDocument>(*openScene);
				auto value = openScene->models[(size_t)selectedModel];
				auto base = value.id + ".Copy";
				value.id = base;
				unsigned suffix = 2;
				auto exists = [&](std::string const& id)
				{
					return std::any_of(openScene->models.begin(),
					                   openScene->models.end(),
					                   [&](auto const& current) { return current.id == id; });
				};
				while (exists(value.id))
					value.id = base + std::to_string(suffix++);
				openScene->models.insert(openScene->models.begin() + selectedModel + 1, value);
				++selectedModel;
				auto after = std::make_shared<SceneDocument>(*openScene);
				documentChangedSincePreview = true;
				sceneCommands.execute(
				    std::make_unique<SceneSnapshotCommand>("Duplicate Scene Model", &openScene, before, after));
				lastEditScene = true;
				sceneDirty = scenePath.empty() || sceneCommands.dirty();
			}
			if (requestDuplicate && openScene && selectedModel <= -100 && selectedModel > -10000)
			{
				auto index = (size_t)(-100 - selectedModel);
				if (index < openScene->lights.size())
				{
					auto before = std::make_shared<SceneDocument>(*openScene);
					auto value = openScene->lights[index];
					auto base = value.id + ".Copy";
					value.id = base;
					unsigned suffix = 2;
					while (std::any_of(openScene->lights.begin(),
					                   openScene->lights.end(),
					                   [&](auto const& current) { return current.id == value.id; }))
						value.id = base + std::to_string(suffix++);
					openScene->lights.insert(openScene->lights.begin() + index + 1, value);
					selectedModel = -100 - (int)(index + 1);
					auto after = std::make_shared<SceneDocument>(*openScene);
					documentChangedSincePreview = true;
					sceneCommands.execute(
					    std::make_unique<SceneSnapshotCommand>("Duplicate Scene Light", &openScene, before, after));
					lastEditScene = true;
					sceneDirty = scenePath.empty() || sceneCommands.dirty();
				}
			}
			if (requestDuplicate && openScene && selectedModel <= -10000)
			{
				auto index = (size_t)(-10000 - selectedModel);
				if (index < openScene->particleEffects.size())
				{
					auto before = std::make_shared<SceneDocument>(*openScene); auto value = openScene->particleEffects[index]; auto base = value.id + ".Copy"; value.id = base; unsigned suffix = 2;
					while (std::any_of(openScene->particleEffects.begin(), openScene->particleEffects.end(), [&](auto const& current) { return current.id == value.id; })) value.id = base + std::to_string(suffix++);
					openScene->particleEffects.insert(openScene->particleEffects.begin() + index + 1, value); selectedModel = -10000 - (int)(index + 1);
					auto after = std::make_shared<SceneDocument>(*openScene); documentChangedSincePreview = true; sceneCommands.execute(std::make_unique<SceneSnapshotCommand>("Duplicate Particle Effect", &openScene, before, after)); lastEditScene = true; sceneDirty = scenePath.empty() || sceneCommands.dirty();
				}
			}
			if (requestDelete && openScene && selectedModel >= 0 && (size_t)selectedModel < openScene->models.size())
			{
				auto before = std::make_shared<SceneDocument>(*openScene);
				openScene->models.erase(openScene->models.begin() + selectedModel);
				selectedModel = -1;
				auto after = std::make_shared<SceneDocument>(*openScene);
				documentChangedSincePreview = true;
				sceneCommands.execute(
				    std::make_unique<SceneSnapshotCommand>("Delete Scene Model", &openScene, before, after));
				lastEditScene = true;
				sceneDirty = scenePath.empty() || sceneCommands.dirty();
			}
			if (requestDelete && openScene && selectedModel <= -100 && selectedModel > -10000)
			{
				auto index = (size_t)(-100 - selectedModel);
				if (index < openScene->lights.size())
				{
					auto before = std::make_shared<SceneDocument>(*openScene);
					openScene->lights.erase(openScene->lights.begin() + index);
					selectedModel = -1;
					auto after = std::make_shared<SceneDocument>(*openScene);
					documentChangedSincePreview = true;
					sceneCommands.execute(
					    std::make_unique<SceneSnapshotCommand>("Delete Scene Light", &openScene, before, after));
					lastEditScene = true;
					sceneDirty = scenePath.empty() || sceneCommands.dirty();
				}
			}
			if (requestDelete && openScene && selectedModel <= -10000)
			{
				auto index = (size_t)(-10000 - selectedModel);
				if (index < openScene->particleEffects.size())
				{
					auto before = std::make_shared<SceneDocument>(*openScene); openScene->particleEffects.erase(openScene->particleEffects.begin() + index); selectedModel = -1;
					auto after = std::make_shared<SceneDocument>(*openScene); documentChangedSincePreview = true; sceneCommands.execute(std::make_unique<SceneSnapshotCommand>("Delete Particle Effect", &openScene, before, after)); lastEditScene = true; sceneDirty = scenePath.empty() || sceneCommands.dirty();
				}
			}
			if (requestNew && confirmDiscardWorkspace())
			{
				if (loadWorkspace(newTemplatePath, false, false))
				{
					currentPath.clear();
					scenePath.clear();
					pipelineDirty = true;
					sceneDirty = openScene != nullptr;
					refreshTrackedFiles();
					queueWorkingPreview("New workspace preview");
				}
			}
			if (requestGltfImport)
			{
				if (auto selected = mpp::app::openGltfFileDialog(window.getWindow(), "Inspect glTF import"))
					try
					{
						gltfImportPath = *selected;
						gltfImportMaterials = resource_parsers::GltfPbrMaterialLoader::listMaterialNames(gltfImportPath);
						gltfImportSelected.assign(gltfImportMaterials.size(), false);
						openGltfImportDialog = true;
					}
					catch (std::exception const& error) { operationErrorTitle = "glTF Import Failed"; operationErrorMessage = error.what(); operationMessageIsSuccess = false; openOperationError = true; }
			}
			if (openGltfImportDialog) { ImGui::OpenPopup("Import glTF Items"); openGltfImportDialog = false; }
			if (ImGui::BeginPopupModal("Import glTF Items", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::TextUnformatted(gltfImportPath.c_str());
				ImGui::SeparatorText("Materials");
				for (size_t item = 0; item < gltfImportMaterials.size(); ++item) { bool selected = gltfImportSelected[item]; if (ImGui::Checkbox(gltfImportMaterials[item].c_str(), &selected)) gltfImportSelected[item] = selected; }
				ImGui::SeparatorText("Other glTF items");
				ImGui::TextDisabled("Meshes, nodes, scenes, cameras, lights, skins, and animations — Not supported yet");
				bool any = std::any_of(gltfImportSelected.begin(), gltfImportSelected.end(), [](bool selected) { return selected; });
				if (ImGui::Button("Import", ImVec2(120, 0)) && any && openDocument)
				{
					try
					{
						auto before = clonePipeline(openDocument); std::string assigned;
						for (size_t item = 0; item < gltfImportMaterials.size(); ++item) if (gltfImportSelected[item])
						{
							auto loaded = resource_parsers::GltfPbrMaterialLoader::loadMaterialByName(gltfImportPath, gltfImportMaterials[item]);
							PbrPipelineResourceDocument material{loaded.materialName, PbrPipelineResourceKind::PbrMaterial, std::move(loaded.definition)};
							auto base = material.name; unsigned suffix = 2; auto exists = [&](std::string const& name) { return std::any_of(openDocument->localResources.begin(), openDocument->localResources.end(), [&](auto const& value) { return value.name == name; }); };
							while (exists(material.name)) material.name = base + "." + std::to_string(suffix++); material.definition.setEntryValue("name", material.name);
							std::string binding = "Imported." + material.name; openDocument->localResources.push_back(std::move(material)); openDocument->previewBindings.push_back({binding, openDocument->localResources.back().name}); if (assigned.empty()) assigned = binding;
						}
						if (openScene && selectedModel >= 0 && (size_t)selectedModel < openScene->models.size() && !assigned.empty()) openScene->models[(size_t)selectedModel].materialBinding = assigned;
						auto after = clonePipeline(openDocument); pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>("Import glTF Materials", &openDocument, before, after)); pipelineDirty = true; documentChangedSincePreview = true; ImGui::CloseCurrentPopup();
					}
					catch (std::exception const& error) { operationErrorTitle = "glTF Import Failed"; operationErrorMessage = error.what(); openOperationError = true; }
				}
				ImGui::SameLine(); if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}
			if (requestOpen && confirmDiscardWorkspace())
				if (auto path = mpp::app::openXmlFileDialog(window.getWindow(), "Open PBR Pipeline"))
					if (loadWorkspace(*path, true))
						queueWorkingPreview("Opened workspace preview");
			if (!requestedRecent.empty() && confirmDiscardWorkspace())
			{
				if (std::filesystem::exists(requestedRecent))
				{
					if (loadWorkspace(requestedRecent, true))
						queueWorkingPreview("Recent workspace preview");
				}
				else
				{
					recentPaths.erase(std::remove(recentPaths.begin(), recentPaths.end(), requestedRecent),
					                  recentPaths.end());
					writeRecent();
					operationErrorTitle = "Recent File Removed";
					operationErrorMessage =
					    "The recent pipeline no longer exists and was removed from the list.\n\n" + requestedRecent;
					if (!recentPersistenceError.empty())
						operationErrorMessage += "\n\n" + recentPersistenceError;
					openOperationError = true;
				}
			}
			if (requestSaveAll)
			{
				if (!openScene || saveScene(false))
					savePipeline(false);
			}
			else
			{
				if (requestSaveScene || requestSaveSceneAs)
					saveScene(requestSaveSceneAs);
				if (requestSave || requestSaveAs)
					savePipeline(requestSaveAs);
			}
			if (requestExportPackage && openDocument && openScene)
				if (auto selected =
				        mpp::app::savePackageFileDialog(window.getWindow(), "Export Package", "workspace.mpppackage"))
				{
					try
					{
						auto target = mpp::app::normaliseDocumentPath(*selected);
						if (target.extension() != ".mpppackage")
							target += ".mpppackage";
						exportPipelinePackage(*openDocument, *openScene, currentPath, scenePath, target);
						operationMessageIsSuccess = true;
						operationErrorTitle = "Package Exported";
						operationErrorMessage = "Created self-contained package:\n" + target.string();
						openOperationError = true;
					}
					catch (std::exception const& error)
					{
						reportOperationError(
						    "Package Export Failed", "Could not create a complete package.", *selected, error);
					}
				}
			if (requestExportLegacyPackage && openDocument && openScene)
				if (auto selected = mpp::app::savePackageFileDialog(
				        window.getWindow(), "Export Legacy Package", "workspace.mpppackage"))
				{
					try
					{
						auto target = mpp::app::normaliseDocumentPath(*selected);
						if (target.extension() != ".mpppackage")
							target += ".mpppackage";
						exportLegacyPipelinePackage(*openDocument, *openScene, currentPath, scenePath, target);
						operationMessageIsSuccess = true;
						operationErrorTitle = "Legacy Package Exported";
						operationErrorMessage = "Created self-contained legacy package:\n" + target.string();
						openOperationError = true;
					}
					catch (std::exception const& error)
					{
						reportOperationError(
						    "Legacy Package Export Failed", "Could not create a complete legacy package.", *selected, error);
					}
				}
			if (requestOverwriteConflicts)
			{
				bool writableConflict = false;
				auto pipelineFile =
				         currentPath.empty() ? std::string() : mpp::app::normaliseDocumentPath(currentPath).string(),
				     sceneFile =
				         scenePath.empty() ? std::string() : mpp::app::normaliseDocumentPath(scenePath).string();
				if (std::find(externalConflicts.begin(), externalConflicts.end(), sceneFile) != externalConflicts.end())
				{
					writableConflict = true;
					saveScene(false);
				}
				if (std::find(externalConflicts.begin(), externalConflicts.end(), pipelineFile) !=
				    externalConflicts.end())
				{
					writableConflict = true;
					savePipeline(false);
				}
				if (!writableConflict)
				{
					operationErrorTitle = "Read-only Library Conflict";
					operationErrorMessage =
					    "Only external resource libraries changed. Reload the workspace, keep the editor version, or "
					    "use Make Local Copy; PipelineEditor never overwrites external libraries.";
					openOperationError = true;
				}
			}
			if (requestReloadConflicts && openDocument && confirmDiscardWorkspace())
			{
				bool wasUntitled = currentPath.empty();
				auto reloadPath = wasUntitled ? openDocument->sourcePath : currentPath;
				if (!reloadPath.empty() && loadWorkspace(reloadPath, false, !wasUntitled))
				{
					if (wasUntitled)
					{
						currentPath.clear();
						pipelineDirty = true;
						refreshTrackedFiles();
					}
					queueWorkingPreview("Reloaded workspace preview");
				}
			}
			if (requestKeepConflicts)
			{
				bool pipelineAffected = false, sceneAffected = false;
				auto conflicts = externalConflicts;
				for (auto const& path : conflicts)
				{
					if (!currentPath.empty() && mpp::app::normaliseDocumentPath(currentPath).string() == path)
						pipelineAffected = true;
					else if (!scenePath.empty() && mpp::app::normaliseDocumentPath(scenePath).string() == path)
						sceneAffected = true;
					else
						pipelineAffected = true;
					acknowledgeTrackedFile(path);
				}
				if (pipelineAffected)
					pipelineDirty = true;
				if (sceneAffected)
					sceneDirty = true;
				externalConflicts.clear();
				documentChangedSincePreview = true;
			}
			if (showPreferences)
			{
				ImGui::Begin("Preferences", &showPreferences);
				ImGui::InputInt("Startup width", &windowWidth);
				ImGui::InputInt("Startup height", &windowHeight);
				ImGui::InputFloat("Recovery interval (seconds)", &recoverySeconds);
				windowWidth = std::max(640, windowWidth);
				windowHeight = std::max(480, windowHeight);
				recoverySeconds = std::max(5.0f, recoverySeconds);

				ImGui::SeparatorText("RenderDoc");
				char renderDocPath[2048]{};
				strncpy_s(
				    renderDocPath, editorSettings.renderDocExecutable.string().c_str(), sizeof(renderDocPath) - 1);
				if (ImGui::InputText("qrenderdoc.exe", renderDocPath, sizeof(renderDocPath)))
					editorSettings.renderDocExecutable = renderDocPath;
				ImGui::SameLine();
				if (ImGui::Button("Browse##RenderDocExecutable"))
				{
					if (auto selected = mpp::app::openExecutableFileDialog(window.getWindow(), "Locate qrenderdoc.exe"))
						editorSettings.renderDocExecutable = *selected;
				}
				char capturePath[2048]{};
				strncpy_s(capturePath, editorSettings.captureDirectory.string().c_str(), sizeof(capturePath) - 1);
				if (ImGui::InputText("Capture directory", capturePath, sizeof(capturePath)))
					editorSettings.captureDirectory = capturePath;
				ImGui::SameLine();
				if (ImGui::Button("Browse##RenderDocCaptures"))
				{
					if (auto selected =
					        mpp::app::selectFolderDialog(window.getWindow(), "Select RenderDoc Capture Directory"))
						editorSettings.captureDirectory = *selected;
				}

				if (ImGui::Button("Save Preferences"))
				{
					try
					{
						std::ofstream config("PipelineEditor.cfg", std::ios::trunc);
						config << "width=" << windowWidth << '\n'
						       << "height=" << windowHeight << '\n'
						       << "recoverySeconds=" << recoverySeconds << '\n';
						if (!config)
							throw std::runtime_error("Could not write PipelineEditor.cfg.");
						saveRenderDocSettings(editorSettings);
					}
					catch (std::exception const& error)
					{
						operationErrorTitle = "Preferences Save Failed";
						operationErrorMessage = error.what();
						openOperationError = true;
					}
				}
				ImGui::TextDisabled("Window dimensions apply on next launch.");
				ImGui::End();
			}
			auto passEffectivelyEnabled = [&](GraphPassInfo const& info)
			{
				if (!info.enabled)
					return false;
				auto const& factory = info.callbackFactory;
				if (factory == "MPP.BloomExtract" || factory == "MPP.BloomComposite")
					return openDocument && openDocument->bloom.enabled;
				if (factory == "MPP.BloomBlurHorizontal" || factory == "MPP.BloomBlurVertical")
					return openDocument && openDocument->bloom.enabled &&
					       passNameIndex(info.name) < openDocument->bloom.blurPasses;
				return true;
			};
			std::vector<uint32_t> selectedMaterialPasses, selectedIndependentPasses;
			std::string selectedMaterialReference;
			if (openDocument)
			{
				if (selectedLocalResource >= 0 && (size_t)selectedLocalResource < openDocument->localResources.size() &&
				    openDocument->localResources[(size_t)selectedLocalResource].kind ==
				        PbrPipelineResourceKind::PbrMaterial)
					selectedMaterialReference = openDocument->localResources[(size_t)selectedLocalResource].name;
				else if (selectedExternalResource >= 0 &&
				         (size_t)selectedExternalResource < openDocument->externalResources.size() &&
				         openDocument->externalResources[(size_t)selectedExternalResource].resource.kind ==
				             PbrPipelineResourceKind::PbrMaterial)
				{
					auto const& external = openDocument->externalResources[(size_t)selectedExternalResource];
					selectedMaterialReference = external.libraryName + "::" + external.resource.name;
				}
				if (!selectedMaterialReference.empty() && openDocument->graph)
				{
					std::set<std::string> materialBindings;
					for (auto const& binding : openDocument->previewBindings)
						if (binding.materialResource == selectedMaterialReference)
							materialBindings.insert(binding.binding);
					bool usedByScene =
					    openScene && std::any_of(openScene->models.begin(),
					                             openScene->models.end(),
					                             [&](auto const& model)
					                             { return materialBindings.contains(model.materialBinding); });
					for (uint32_t pass = 0; pass < openDocument->graph->getPassCount(); ++pass)
					{
						auto info = openDocument->graph->getPassInfo({pass});
						if (!passEffectivelyEnabled(info))
							continue;
						auto metadata = authoringRegistry.findMetadata(info.callbackFactory);
						if (!metadata)
							continue;
						if (metadata->materialSlots.empty())
							selectedIndependentPasses.push_back(pass);
						else if (usedByScene)
							selectedMaterialPasses.push_back(pass);
					}
				}
			}
			auto showMaterialPassUsage = [&]()
			{
				ImGui::SeparatorText("Enabled passes");
				ImGui::TextDisabled("Uses selected material");
				if (selectedMaterialPasses.empty())
					ImGui::BulletText("None");
				else
					for (auto pass : selectedMaterialPasses)
						ImGui::BulletText("%s", openDocument->graph->getPassInfo({pass}).name.c_str());
				ImGui::TextDisabled("Material-independent (always runs)");
				if (selectedIndependentPasses.empty())
					ImGui::BulletText("None");
				else
					for (auto pass : selectedIndependentPasses)
						ImGui::BulletText("%s", openDocument->graph->getPassInfo({pass}).name.c_str());
			};
			ImGui::Begin("Pipeline Hierarchy");
			ImGui::TextUnformatted(openDocument ? openDocument->name.c_str() : "Pipeline");
			if (openDocument && openDocument->graph && ImGui::TreeNodeEx("Passes", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (uint32_t pass = 0; pass < openDocument->graph->getPassCount(); ++pass)
				{
					auto info = openDocument->graph->getPassInfo({pass});
					bool effectivelyEnabled = passEffectivelyEnabled(info);
					auto passLabel = info.name + (effectivelyEnabled ? std::string() : std::string("  [bypassed]"));
					bool materialPass = std::find(selectedMaterialPasses.begin(), selectedMaterialPasses.end(), pass) !=
					                    selectedMaterialPasses.end(),
					     independentPass =
					         std::find(selectedIndependentPasses.begin(), selectedIndependentPasses.end(), pass) !=
					         selectedIndependentPasses.end();
					ImGui::PushID((int)pass);
					if (materialPass || independentPass)
					{
						auto colour =
						    materialPass ? ImVec4(0.62f, 0.42f, 0.08f, 0.9f) : ImVec4(0.12f, 0.4f, 0.55f, 0.9f);
						ImGui::PushStyleColor(ImGuiCol_Header, colour);
						ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
						                      materialPass ? ImVec4(0.78f, 0.55f, 0.12f, 1.0f)
						                                   : ImVec4(0.16f, 0.55f, 0.72f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_HeaderActive,
						                      materialPass ? ImVec4(0.9f, 0.66f, 0.18f, 1.0f)
						                                   : ImVec4(0.2f, 0.65f, 0.85f, 1.0f));
					}
					if (!effectivelyEnabled)
						ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
					if (ImGui::Selectable(passLabel.c_str(),
					                      selectedPass == (int)pass || materialPass || independentPass))
					{
						selectedPass = (int)pass;
						selectedImage = selectedImport = selectedBinding = selectedOverride = selectedModel =
						    selectedLocalResource = selectedExternalResource = -1;
					}
					if (!effectivelyEnabled)
						ImGui::PopStyleColor();
					if (materialPass || independentPass)
						ImGui::PopStyleColor(3);
					if (ImGui::BeginDragDropSource())
					{
						ImGui::SetDragDropPayload("MPP_PASS", &pass, sizeof(pass));
						ImGui::Text("Move %s", info.name.c_str());
						ImGui::EndDragDropSource();
					}
					if (ImGui::BeginDragDropTarget())
					{
						if (auto payload = ImGui::AcceptDragDropPayload("MPP_PASS"))
						{
							auto source = *static_cast<uint32_t const*>(payload->Data);
							if (source != pass)
							{
								auto before = clonePipeline(openDocument);
								openDocument->graph->movePass({source}, pass);
								auto after = clonePipeline(openDocument);
								pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
								    "Move Pass", &openDocument, before, after));
								selectedPass = (int)pass;
								pipelineDirty = true;
								documentChangedSincePreview = true;
								lastEditScene = false;
							}
						}
						ImGui::EndDragDropTarget();
					}
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
			if (openDocument && openDocument->graph && ImGui::TreeNodeEx("Images", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (uint32_t image = 0; image < openDocument->graph->getImageCount(); ++image)
				{
					auto info = openDocument->graph->getImageInfo({image, 0});
					auto imageLabel = info.name + (hasGraphImageUsage(info.desc.usage, GraphImageUsage::Presentation)
					                                   ? std::string("  [presentation]")
					                                   : hasGraphImageUsage(info.desc.usage, GraphImageUsage::Exported) ? std::string("  [exported]") : std::string());
					if (ImGui::Selectable(imageLabel.c_str(), selectedImage == (int)image))
					{
						selectedImage = (int)image;
						inspectedVersion = (int)openDocument->graph->getImageVersionCount(image) - 1;
						selectedPass = selectedImport = selectedBinding = selectedOverride = selectedModel =
						    selectedLocalResource = selectedExternalResource = -1;
					}
				}
				ImGui::TreePop();
			}
			if (openDocument && !openDocument->imports.empty() &&
			    ImGui::TreeNodeEx("Typed Imports", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (size_t index = 0; index < openDocument->imports.size(); ++index)
					if (ImGui::Selectable(openDocument->imports[index].id.c_str(), selectedImport == (int)index))
					{
						selectedImport = (int)index;
						selectedPass = selectedImage = selectedBinding = selectedOverride = selectedModel =
						    selectedLocalResource = selectedExternalResource = -1;
					}
				ImGui::TreePop();
			}
			if (openDocument && !openDocument->localResources.empty() &&
			    ImGui::TreeNodeEx("Local Resources", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (size_t index = 0; index < openDocument->localResources.size(); ++index)
				{
					auto const& resource = openDocument->localResources[index];
					auto label = resource.name + "  [" + localResourceKindName(resource.kind) + "]";
					ImGui::PushID((int)index);
					if (ImGui::Selectable(label.c_str(), selectedLocalResource == (int)index))
					{
						selectedLocalResource = (int)index;
						selectedPass = selectedImage = selectedImport = selectedBinding = selectedOverride =
						    selectedModel = selectedExternalResource = -1;
					}
					if (ImGui::BeginDragDropSource())
					{
						ImGui::SetDragDropPayload("MPP_RESOURCE", &index, sizeof(index));
						ImGui::Text("Move %s", openDocument->localResources[index].name.c_str());
						ImGui::EndDragDropSource();
					}
					if (ImGui::BeginDragDropTarget())
					{
						if (auto payload = ImGui::AcceptDragDropPayload("MPP_RESOURCE"))
						{
							auto source = *static_cast<size_t const*>(payload->Data);
							if (source != index && source < openDocument->localResources.size())
							{
								auto before = clonePipeline(openDocument);
								auto value = openDocument->localResources[source];
								openDocument->localResources.erase(openDocument->localResources.begin() + source);
								openDocument->localResources.insert(openDocument->localResources.begin() + index,
								                                    value);
								auto after = clonePipeline(openDocument);
								pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
								    "Move Local Resource", &openDocument, before, after));
								selectedLocalResource = (int)index;
								pipelineDirty = true;
								documentChangedSincePreview = true;
								lastEditScene = false;
							}
						}
						ImGui::EndDragDropTarget();
					}
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
			if (openDocument && !openDocument->externalResources.empty() &&
			    ImGui::TreeNodeEx("External Libraries", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (size_t index = 0; index < openDocument->externalResources.size(); ++index)
				{
					auto const& value = openDocument->externalResources[index];
					auto label = value.libraryName + "::" + value.resource.name;
					if (ImGui::Selectable(label.c_str(), selectedExternalResource == (int)index))
					{
						selectedExternalResource = (int)index;
						selectedPass = selectedImage = selectedImport = selectedBinding = selectedOverride =
						    selectedModel = selectedLocalResource = -1;
					}
				}
				ImGui::TreePop();
			}
			if (openDocument && ImGui::Selectable("Pipeline Environment", selectedBinding == -2))
			{
				selectedBinding = -2;
				selectedPass = selectedImage = selectedImport = selectedOverride = selectedModel =
				    selectedLocalResource = selectedExternalResource = -1;
			}
			if (openDocument && ImGui::Selectable("Bloom Settings", selectedBinding == -3))
			{
				selectedBinding = -3;
				selectedPass = selectedImage = selectedImport = selectedOverride = selectedModel =
				    selectedLocalResource = selectedExternalResource = -1;
			}
			if (openDocument && ImGui::Selectable("Ambient Occlusion", selectedBinding == -4))
			{
				selectedBinding = -4;
				selectedPass = selectedImage = selectedImport = selectedOverride = selectedModel =
				    selectedLocalResource = selectedExternalResource = -1;
			}
			if (openDocument && !openDocument->previewBindings.empty() &&
			    ImGui::TreeNodeEx("Preview Bindings", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (size_t index = 0; index < openDocument->previewBindings.size(); ++index)
					if (ImGui::Selectable(openDocument->previewBindings[index].binding.c_str(),
					                      selectedBinding == (int)index))
					{
						selectedBinding = (int)index;
						selectedPass = selectedImage = selectedImport = selectedOverride = selectedModel =
						    selectedLocalResource = selectedExternalResource = -1;
					}
				ImGui::TreePop();
			}
			if (openDocument && !openDocument->previewOverrides.empty() &&
			    ImGui::TreeNodeEx("Instance Overrides", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (size_t index = 0; index < openDocument->previewOverrides.size(); ++index)
				{
					auto label = openDocument->previewOverrides[index].modelId + " / " +
					             openDocument->previewOverrides[index].binding;
					if (ImGui::Selectable(label.c_str(), selectedOverride == (int)index))
					{
						selectedOverride = (int)index;
						selectedPass = selectedImage = selectedImport = selectedBinding = selectedModel =
						    selectedLocalResource = selectedExternalResource = -1;
					}
				}
				ImGui::TreePop();
			}
			if (openScene && ImGui::TreeNodeEx("Preview Scene", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto selectScene = [&](int selection)
				{
					selectedModel = selection;
					selectedPass = selectedImage = selectedImport = selectedBinding = selectedOverride =
					    selectedLocalResource = selectedExternalResource = -1;
				};
				if (ImGui::Selectable("Camera", selectedModel == -2))
					selectScene(-2);
				if (ImGui::Selectable("Environment", selectedModel == -3))
					selectScene(-3);
				if (ImGui::Selectable("Render Layers", selectedModel == -4))
					selectScene(-4);
				if (ImGui::TreeNodeEx("Models", ImGuiTreeNodeFlags_DefaultOpen))
				{
					for (size_t model = 0; model < openScene->models.size(); ++model)
					{
						ImGui::PushID((int)model);
						if (ImGui::Selectable(openScene->models[model].id.c_str(), selectedModel == (int)model))
							selectScene((int)model);
						if (ImGui::BeginDragDropSource())
						{
							ImGui::SetDragDropPayload("MPP_MODEL", &model, sizeof(model));
							ImGui::Text("Move %s", openScene->models[model].id.c_str());
							ImGui::EndDragDropSource();
						}
						if (ImGui::BeginDragDropTarget())
						{
							if (auto payload = ImGui::AcceptDragDropPayload("MPP_MODEL"))
							{
								auto source = *static_cast<size_t const*>(payload->Data);
								if (source != model && source < openScene->models.size())
								{
									auto before = std::make_shared<SceneDocument>(*openScene);
									auto value = openScene->models[source];
									openScene->models.erase(openScene->models.begin() + source);
									openScene->models.insert(openScene->models.begin() + model, value);
									auto after = std::make_shared<SceneDocument>(*openScene);
									sceneCommands.execute(std::make_unique<SceneSnapshotCommand>(
									    "Move Scene Model", &openScene, before, after));
									selectScene((int)model);
									sceneDirty = true;
									documentChangedSincePreview = true;
									lastEditScene = true;
								}
							}
							ImGui::EndDragDropTarget();
						}
						ImGui::PopID();
					}
					ImGui::TreePop();
				}
				if (ImGui::TreeNodeEx("Lights", ImGuiTreeNodeFlags_DefaultOpen))
				{
					for (size_t light = 0; light < openScene->lights.size(); ++light)
						if (ImGui::Selectable(openScene->lights[light].id.c_str(), selectedModel == -100 - (int)light))
							selectScene(-100 - (int)light);
					ImGui::TreePop();
				}
				if (ImGui::TreeNodeEx("Particle Effects", ImGuiTreeNodeFlags_DefaultOpen))
				{
					for (size_t effect = 0; effect < openScene->particleEffects.size(); ++effect)
						if (ImGui::Selectable(openScene->particleEffects[effect].id.c_str(), selectedModel == -10000 - (int)effect))
							selectScene(-10000 - (int)effect);
					ImGui::TreePop();
				}
				ImGui::TreePop();
			}
			ImGui::End();

			bool flowFiltersChanged = processFlowView.consumeFiltersChanged();
			bool flowRefreshRequested = processFlowView.consumeRefreshRequested();
			bool flowPipelineChanged = processFlowPipeline != activePipeline;
			uint64_t sceneGeneration = sceneRuntime.getGeneration();
			bool flowSceneChanged = processFlowSceneGeneration != sceneGeneration;
			if (flowPipelineChanged || flowSceneChanged)
			{
				processFlowPipeline = activePipeline;
				processFlowSceneGeneration = sceneGeneration;
				sampledFlowSnapshot.reset();
				sampledFlowSceneGeneration = 0;
				flowSampleAcquired = {};
			}
			double flowNow = std::chrono::duration<double>(now.time_since_epoch()).count();
			bool flowSampleDue = processFlowSampleGate.poll(
			    flowNow, flowRefreshRequested || flowPipelineChanged || flowSceneChanged);
			bool flowSnapshotChanged = false;
			if (flowSampleDue && !activeGraphResource.empty())
			{
				auto latest = renderSystem.getRenderPipeline(activePipeline)->getLastFlowSnapshot();
				if (latest && (!sampledFlowSnapshot || latest->pipelineGeneration != sampledFlowSnapshot->pipelineGeneration ||
				               latest->frameSerial > sampledFlowSnapshot->frameSerial))
				{
					sampledFlowSnapshot = std::move(latest);
					sampledFlowSceneGeneration = sceneGeneration;
					flowSampleAcquired = now;
					flowSnapshotChanged = true;
				}
			}
			bool flowDocumentChanged = processFlowEditSerial != previewEditSerial;
			if (flowFiltersChanged || flowRefreshRequested || flowPipelineChanged || flowSceneChanged ||
			    flowSnapshotChanged || flowDocumentChanged || processFlowModel.revision == 0)
			{
				processFlowEditSerial = previewEditSerial;
				ProcessFlowBuildInput input;
				bool workingGraphInvalid = false;
				if (documentChangedSincePreview && openDocument && openDocument->graph)
					workingGraphInvalid = !openDocument->graph->compile().valid;
				auto flowDocument = workingGraphInvalid ? openDocument : activePreviewDocument;
				input.graph = (workingGraphInvalid || !activeGraphResource.empty()) && flowDocument && flowDocument->graph
				                  ? flowDocument->graph.get() : nullptr;
				input.snapshot = !workingGraphInvalid && sampledFlowSceneGeneration == sceneGeneration
				                     ? sampledFlowSnapshot : RenderPipelineFlowSnapshotPtr{};
				if (!activeGraphResource.empty())
					input.outputPlans = renderSystem.getRenderPipeline(activePipeline)->getOutputPlans();
				input.sceneGeneration = sceneGeneration;
				input.stale = !workingGraphInvalid && (previewStale || documentChangedSincePreview);
				input.staleReason = "Showing process flow from the last valid generation while working changes are pending.";
				input.filters = processFlowView.filters();
				if (flowDocument)
					for (size_t index = 0; index < flowDocument->imports.size(); ++index)
					{
						input.imports[flowDocument->imports[index].id] = (int)index;
						if (!flowDocument->imports[index].semantic.empty())
							input.imports[flowDocument->imports[index].semantic] = (int)index;
					}
				if (input.snapshot)
					for (auto const& batch : input.snapshot->batches)
						if (batch.sceneObject && !input.sceneObjects.contains(batch.sceneObject))
						{
							auto id = sceneRuntime.getModelId(batch.sceneObject);
							if (id.empty()) continue;
							int index = -1;
							if (openScene)
								for (size_t model = 0; model < openScene->models.size(); ++model)
									if (openScene->models[model].id == id) { index = (int)model; break; }
							input.sceneObjects.emplace(batch.sceneObject, ProcessFlowSceneObjectRef{index, id});
						}
				if (input.graph)
				{
					input.passBypassReasons.resize(input.graph->getPassCount());
					for (uint32_t pass = 0; pass < input.graph->getPassCount(); ++pass)
					{
						auto info = input.graph->getPassInfo({pass});
						if (!info.enabled) input.passBypassReasons[pass] = "Disabled by authored pass setting";
						else if (flowDocument &&
						         ((info.callbackFactory == "MPP.BloomExtract" || info.callbackFactory == "MPP.BloomComposite") &&
						          !flowDocument->bloom.enabled))
							input.passBypassReasons[pass] = "Bypassed because bloom is disabled";
						else if (flowDocument &&
						         (info.callbackFactory == "MPP.BloomBlurHorizontal" || info.callbackFactory == "MPP.BloomBlurVertical") &&
						         (!flowDocument->bloom.enabled || passNameIndex(info.name) >= flowDocument->bloom.blurPasses))
							input.passBypassReasons[pass] = flowDocument->bloom.enabled
							                                      ? "Bypassed beyond the effective bloom blur-pass count"
							                                      : "Bypassed because bloom is disabled";
					}
				}
				try
				{
					auto candidate = processFlowBuilder.build(input);
					processFlowLayout.apply(candidate);
					processFlowModel = std::move(candidate);
				}
				catch (std::exception const& error)
				{
					uint64_t expectedGeneration = input.snapshot ? input.snapshot->pipelineGeneration : 0;
					if (!processFlowModel.nodes.empty() && processFlowModel.pipelineGeneration == expectedGeneration)
						processFlowModel.warningBanner = "Latest process-flow sample was rejected; retaining the previous valid model: " +
						                                 std::string(error.what());
					else
					{
						ProcessFlowModel failed; failed.revision = processFlowModel.revision + 1;
						failed.diagnostics.push_back(error.what()); processFlowModel = std::move(failed);
					}
				}
			}
			double flowSampleAge = flowSampleAcquired == std::chrono::steady_clock::time_point{}
			                           ? 0.0
			                           : std::chrono::duration<double>(now - flowSampleAcquired).count();
			ProcessFlowHighlight flowHighlight{selectedPass, selectedImage, selectedImport, selectedModel,
			                                   selectedMaterialReference};
			auto flowSelection = processFlowView.draw(processFlowModel, flowSampleAge, flowHighlight);
			if (flowSelection.kind == ProcessFlowSelection::Kind::Pass && openDocument && openDocument->graph &&
			    flowSelection.index >= 0 && (size_t)flowSelection.index < openDocument->graph->getPassCount())
			{
				selectedPass = flowSelection.index;
				selectedImage = selectedImport = selectedBinding = selectedOverride = selectedModel =
				    selectedLocalResource = selectedExternalResource = -1;
				ImGui::SetWindowFocus("Inspector");
			}
			else if (flowSelection.kind == ProcessFlowSelection::Kind::Import && openDocument &&
			         flowSelection.index >= 0 && (size_t)flowSelection.index < openDocument->imports.size())
			{
				selectedImport = flowSelection.index;
				selectedPass = selectedImage = selectedBinding = selectedOverride = selectedModel =
				    selectedLocalResource = selectedExternalResource = -1;
				ImGui::SetWindowFocus("Inspector");
			}
			else if (flowSelection.kind == ProcessFlowSelection::Kind::Image && openDocument && openDocument->graph &&
			         flowSelection.index >= 0 && (size_t)flowSelection.index < openDocument->graph->getImageCount())
			{
				selectedImage = flowSelection.index;
				if (openDocument && openDocument->graph && selectedImage >= 0)
					inspectedVersion = (int)openDocument->graph->getImageVersionCount((uint32_t)selectedImage) - 1;
				selectedPass = selectedImport = selectedBinding = selectedOverride = selectedModel =
				    selectedLocalResource = selectedExternalResource = -1;
				ImGui::SetWindowFocus("Inspector");
			}
			else if (flowSelection.kind == ProcessFlowSelection::Kind::Material && openDocument)
			{
				bool found = false;
				for (size_t index = 0; index < openDocument->localResources.size(); ++index)
					if (openDocument->localResources[index].name == flowSelection.materialName ||
					    flowSelection.materialName.ends_with("/" + openDocument->localResources[index].name))
					{ selectedLocalResource = (int)index; selectedExternalResource = -1; found = true; break; }
				if (!found)
					for (size_t index = 0; index < openDocument->externalResources.size(); ++index)
					{
						auto const& resource = openDocument->externalResources[index];
						if (resource.resource.name == flowSelection.materialName ||
						    resource.libraryName + "::" + resource.resource.name == flowSelection.materialName ||
						    flowSelection.materialName.ends_with("/" + resource.resource.name))
						{ selectedExternalResource = (int)index; selectedLocalResource = -1; found = true; break; }
					}
				if (found)
				{
					selectedPass = selectedImage = selectedImport = selectedBinding = selectedOverride = selectedModel = -1;
					ImGui::SetWindowFocus("Inspector");
				}
			}
			else if (flowSelection.kind == ProcessFlowSelection::Kind::SceneObject && openScene &&
			         flowSelection.sceneObjectIndex >= 0 &&
			         (size_t)flowSelection.sceneObjectIndex < openScene->models.size())
			{
				selectedModel = flowSelection.sceneObjectIndex;
				selectedPass = selectedImage = selectedImport = selectedBinding = selectedOverride =
				    selectedLocalResource = selectedExternalResource = -1;
				ImGui::SetWindowFocus("Inspector");
			}

			ImGui::Begin("Inspector");
			if (openDocument && openDocument->graph && selectedPass >= 0)
			{
				auto info = openDocument->graph->getPassInfo({(uint32_t)selectedPass});
				char passName[256]{};
				strncpy_s(passName, info.name.c_str(), 255);
				if (ImGui::InputText("Pass name", passName, sizeof(passName)))
				{
					auto before = clonePipeline(openDocument);
					try
					{
						openDocument->graph->setPassName({(uint32_t)selectedPass}, passName);
						auto after = clonePipeline(openDocument);
						pipelineCommands.execute(
						    std::make_unique<PipelineSnapshotCommand>("Rename Pass", &openDocument, before, after),
						    true);
						pipelineDirty = true;
						documentChangedSincePreview = true;
					}
					catch (std::exception const& error)
					{
						previewFailure = error.what();
					}
				}
				if (ImGui::BeginCombo("Factory", info.callbackFactory.c_str()))
				{
					for (auto const& factory : authoringRegistry.getRegisteredMetadataNames())
						if (ImGui::Selectable(factory.c_str(), factory == info.callbackFactory))
						{
							auto before = clonePipeline(openDocument);
							openDocument->graph->setPassCallbackFactory({(uint32_t)selectedPass}, factory);
							auto after = clonePipeline(openDocument);
							pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
							    "Change Pass Factory", &openDocument, before, after));
							pipelineDirty = true;
							documentChangedSincePreview = true;
						}
					ImGui::EndCombo();
				}
				if (info.type == GraphPassType::Scene && ImGui::CollapsingHeader("Scene Colour Attachments", ImGuiTreeNodeFlags_DefaultOpen))
				{
					bool emissiveAttachment = false;
					for (size_t output = 0; output < info.colourOutputs.size(); ++output)
					{
						auto image = openDocument->graph->getImageInfo(info.colourOutputs[output].image);
						ImGui::BulletText("Colour %zu: %s (mip %u)", output, image.name.c_str(), info.colourOutputs[output].mipLevel);
						if (output == 1 && image.name == "SceneEmissive") emissiveAttachment = true;
					}
					if (openDocument->bloom.enabled)
					{
						if (emissiveAttachment) ImGui::TextDisabled("Bloom MRT: SceneEmissive is attached at colour output 1.");
						else ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.2f, 1.0f), "Bloom needs SceneEmissive at colour output 1 to receive PBR emissive output.");
					}
				}
				auto metadata = authoringRegistry.findMetadata(info.callbackFactory);
				if (metadata)
				{
					ImGui::TextDisabled("%s / %s", metadata->category.c_str(), metadata->displayName.c_str());
					if (!metadata->inputs.empty() &&
					    ImGui::CollapsingHeader("Input Slots", ImGuiTreeNodeFlags_DefaultOpen))
					{
						for (auto const& slot : metadata->inputs)
						{
							std::string formats;
							for (auto format : slot.acceptedFormats)
							{
								if (!formats.empty())
									formats += ", ";
								formats += graphImageFormatName(format);
							}
							ImGui::BulletText("%s%s sampler=%s",
							                  slot.name.c_str(),
							                  slot.required ? " (required)" : " (optional)",
							                  slot.sampler.c_str());
							if (!formats.empty())
								ImGui::TextDisabled("  Formats: %s", formats.c_str());
							if (!slot.fallbackId.empty())
								ImGui::TextDisabled("  Fallback: %s", slot.fallbackId.c_str());
						}
					}
					if (!metadata->outputs.empty() && ImGui::CollapsingHeader("Output Slots"))
					{
						for (auto const& slot : metadata->outputs)
						{
							std::string formats;
							for (auto format : slot.acceptedFormats)
							{
								if (!formats.empty())
									formats += ", ";
								formats += graphImageFormatName(format);
							}
							ImGui::BulletText("%s%s%s",
							                  slot.name.c_str(),
							                  slot.depth ? " (depth)" : "",
							                  slot.required ? " (required)" : " (optional)");
							if (!formats.empty())
								ImGui::TextDisabled("  Formats: %s", formats.c_str());
						}
					}
					if (!metadata->parameters.empty() && ImGui::CollapsingHeader("Parameter Contract"))
					{
						for (auto const& parameter : metadata->parameters)
						{
							bool present = info.parameters.getUniformData().contains(parameter.name);
							ImGui::BulletText("%s%s type=%d [%zu x %zu]%s",
							                  parameter.name.c_str(),
							                  parameter.required ? " (required)" : "",
							                  (int)parameter.type,
							                  parameter.count,
							                  parameter.elements,
							                  parameter.hasRange ? (" range " + std::to_string(parameter.minimum) +
							                                        ".." + std::to_string(parameter.maximum))
							                                           .c_str()
							                                     : "");
							if (!present)
							{
								ImGui::SameLine();
								ImGui::PushID(parameter.name.c_str());
								if (ImGui::SmallButton("Add"))
								{
									auto before = clonePipeline(openDocument);
									auto parameters = info.parameters;
									auto total = parameter.count * parameter.elements;
									if (parameter.type == program::GLSLType::Float ||
									    parameter.type == program::GLSLType::FloatMatrix)
									{
										std::vector<float> zeros(total);
										parameters.setUniform(parameter.name,
										                      parameter.type,
										                      parameter.count,
										                      parameter.elements,
										                      reinterpret_cast<char const*>(zeros.data()));
									}
									else if (parameter.type == program::GLSLType::Double ||
									         parameter.type == program::GLSLType::DoubleMatrix)
									{
										std::vector<double> zeros(total);
										parameters.setUniform(parameter.name,
										                      parameter.type,
										                      parameter.count,
										                      parameter.elements,
										                      reinterpret_cast<char const*>(zeros.data()));
									}
									else
									{
										std::vector<int> zeros(total);
										parameters.setUniform(parameter.name,
										                      parameter.type,
										                      parameter.count,
										                      parameter.elements,
										                      reinterpret_cast<char const*>(zeros.data()));
									}
									openDocument->graph->setPassParameters({(uint32_t)selectedPass}, parameters);
									auto after = clonePipeline(openDocument);
									pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
									    "Add Pass Parameter", &openDocument, before, after));
									pipelineDirty = true;
									documentChangedSincePreview = true;
								}
								ImGui::PopID();
							}
						}
					}
					if (!metadata->materialSlots.empty() && ImGui::CollapsingHeader("Material Slots"))
					{
						for (auto const& slot : metadata->materialSlots)
						{
							ImGui::TextDisabled("%s", slot.c_str());
							for (size_t bindingIndex = 0; bindingIndex < openDocument->previewBindings.size();
							     ++bindingIndex)
							{
								auto binding = openDocument->previewBindings[bindingIndex];
								ImGui::PushID((int)bindingIndex);
								if (ImGui::BeginCombo(binding.binding.c_str(), binding.materialResource.c_str()))
								{
									for (auto const& resource : openDocument->localResources)
										if (resource.kind == PbrPipelineResourceKind::PbrMaterial &&
										    ImGui::Selectable(resource.name.c_str(),
										                      resource.name == binding.materialResource))
										{
											auto before = clonePipeline(openDocument);
											openDocument->previewBindings[bindingIndex].materialResource =
											    resource.name;
											auto after = clonePipeline(openDocument);
											pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
											    "Edit Material Slot", &openDocument, before, after));
											pipelineDirty = true;
											documentChangedSincePreview = true;
										}
									ImGui::EndCombo();
								}
								ImGui::PopID();
							}
						}
					}
					if (metadata->acceptsProgram)
					{
						char program[256]{};
						strncpy_s(program, info.programResource.c_str(), 255);
						if (ImGui::InputText("Program resource", program, sizeof(program)))
						{
							auto before = clonePipeline(openDocument);
							if (program[0])
								openDocument->graph->setPassProgramResource({(uint32_t)selectedPass}, program);
							else
								openDocument->graph->clearPassProgramResource({(uint32_t)selectedPass});
							auto after = clonePipeline(openDocument);
							pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
							                             "Edit Pass Program", &openDocument, before, after),
							                         true);
							pipelineDirty = true;
							documentChangedSincePreview = true;
						}
					}
				}
				bool enabled = info.enabled;
				if (ImGui::Checkbox("Authored enabled", &enabled))
				{
					auto before = clonePipeline(openDocument);
					openDocument->graph->setPassEnabled({(uint32_t)selectedPass}, enabled);
					auto after = clonePipeline(openDocument);
					documentChangedSincePreview = true;
					pipelineCommands.execute(
					    std::make_unique<PipelineSnapshotCommand>("Toggle Pass", &openDocument, before, after));
					lastEditScene = false;
					pipelineDirty = currentPath.empty() || pipelineCommands.dirty();
				}
				auto effectiveInfo = info;
				effectiveInfo.enabled = enabled;
				bool effectiveEnabled = passEffectivelyEnabled(effectiveInfo);
				if (info.callbackFactory.starts_with("MPP.Bloom"))
					ImGui::TextColored(
					    effectiveEnabled ? ImVec4(0.3f, 0.9f, 0.4f, 1.0f) : ImVec4(0.9f, 0.6f, 0.25f, 1.0f),
					    effectiveEnabled ? "Bloom execution: active" : "Bloom execution: bypassed by bloom settings");
				ImGui::Text("Inputs: %zu  Colour outputs: %zu  Depth outputs: %zu",
				            info.sampledInputs.size(),
				            info.colourOutputs.size(),
				            info.depthOutputs.size());
				if (info.callbackFactory == "MPP.PbrScene")
					ImGui::TextDisabled(openDocument->bloom.enabled
					    ? "Bloom enabled: emissive RGB is written to SceneEmissive."
					    : "Bloom disabled: no emissive render target is attached.");
				if (ImGui::CollapsingHeader("Raster State"))
				{
					auto raster = info.rasterState;
					bool changed = ImGui::Checkbox("Explicit state", &raster.explicitState);
					int fill = (int)raster.fillMode, front = (int)raster.frontFace, cull = (int)raster.cullMode,
					    depth = (int)raster.depthCompare;
					if (ImGui::Combo("Fill mode", &fill, "Fill\0Line\0"))
					{
						raster.fillMode = (GraphFillMode)fill;
						changed = true;
					}
					if (ImGui::Combo("Front face", &front, "Counter-clockwise\0Clockwise\0"))
					{
						raster.frontFace = (GraphFrontFace)front;
						changed = true;
					}
					if (ImGui::Combo("Cull mode", &cull, "None\0Front\0Back\0"))
					{
						raster.cullMode = (GraphCullMode)cull;
						changed = true;
					}
					changed |= ImGui::Checkbox("Depth test", &raster.depthTest);
					changed |= ImGui::Checkbox("Depth write", &raster.depthWrite);
					if (ImGui::Combo(
					        "Depth comparison",
					        &depth,
					        "Never\0Less\0Equal\0Less or equal\0Greater\0Not equal\0Greater or equal\0Always\0"))
					{
						raster.depthCompare = (GraphCompareOp)depth;
						changed = true;
					}
					changed |= ImGui::Checkbox("Blend", &raster.blend);
					if (raster.blend)
					{
						int colourOp = (int)raster.colourBlendOp, alphaOp = (int)raster.alphaBlendOp,
						    sourceColour = (int)raster.sourceColourBlend,
						    destColour = (int)raster.destinationColourBlend, sourceAlpha = (int)raster.sourceAlphaBlend,
						    destAlpha = (int)raster.destinationAlphaBlend;
						char const* operations = "Add\0Subtract\0Reverse subtract\0Minimum\0Maximum\0";
						char const* factors = "Zero\0One\0Source colour\0One minus source colour\0Destination "
						                      "colour\0One minus destination colour\0Source alpha\0One minus source "
						                      "alpha\0Destination alpha\0One minus destination alpha\0";
						if (ImGui::Combo("Colour operation", &colourOp, operations))
						{
							raster.colourBlendOp = (GraphBlendOp)colourOp;
							changed = true;
						}
						if (ImGui::Combo("Alpha operation", &alphaOp, operations))
						{
							raster.alphaBlendOp = (GraphBlendOp)alphaOp;
							changed = true;
						}
						if (ImGui::Combo("Source colour", &sourceColour, factors))
						{
							raster.sourceColourBlend = (GraphBlendFactor)sourceColour;
							changed = true;
						}
						if (ImGui::Combo("Destination colour", &destColour, factors))
						{
							raster.destinationColourBlend = (GraphBlendFactor)destColour;
							changed = true;
						}
						if (ImGui::Combo("Source alpha", &sourceAlpha, factors))
						{
							raster.sourceAlphaBlend = (GraphBlendFactor)sourceAlpha;
							changed = true;
						}
						if (ImGui::Combo("Destination alpha", &destAlpha, factors))
						{
							raster.destinationAlphaBlend = (GraphBlendFactor)destAlpha;
							changed = true;
						}
					}
					changed |= ImGui::Checkbox("Multisample", &raster.multisample);
					changed |= ImGui::Checkbox("Alpha to coverage", &raster.alphaToCoverage);
					changed |= ImGui::Checkbox("Scissor", &raster.scissor);
					if (raster.scissor)
					{
						int rectangle[4] = {(int)raster.scissorRectangle.x,
						                    (int)raster.scissorRectangle.y,
						                    (int)raster.scissorRectangle.z,
						                    (int)raster.scissorRectangle.w};
						if (ImGui::InputInt4("Scissor x/y/width/height", rectangle))
						{
							raster.scissorRectangle = {std::max(0, rectangle[0]),
							                           std::max(0, rectangle[1]),
							                           std::max(0, rectangle[2]),
							                           std::max(0, rectangle[3])};
							changed = true;
						}
					}
					auto outputCount = info.colourOutputs.size();
					if (raster.colourWriteMasks.size() < outputCount)
						raster.colourWriteMasks.resize(outputCount);
					for (size_t mask = 0; mask < raster.colourWriteMasks.size(); ++mask)
					{
						ImGui::PushID((int)mask);
						auto& value = raster.colourWriteMasks[mask];
						bool channels[4] = {value.red, value.green, value.blue, value.alpha};
						if (ImGui::Checkbox("R", &channels[0]) | ImGui::Checkbox("G", &channels[1]) |
						    ImGui::Checkbox("B", &channels[2]) | ImGui::Checkbox("A", &channels[3]))
						{
							value = {channels[0], channels[1], channels[2], channels[3]};
							changed = true;
						}
						ImGui::PopID();
					}
					if (changed)
					{
						auto before = clonePipeline(openDocument);
						openDocument->graph->setPassRasterState({(uint32_t)selectedPass}, raster);
						auto after = clonePipeline(openDocument);
						documentChangedSincePreview = true;
						pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
						                             "Edit Raster State", &openDocument, before, after),
						                         true);
						lastEditScene = false;
						pipelineDirty = currentPath.empty() || pipelineCommands.dirty();
					}
				}
				if (ImGui::CollapsingHeader("Attachments"))
				{
					for (size_t index = 0; index < info.colourOutputs.size(); ++index)
					{
						auto output = info.colourOutputs[index];
						auto image = openDocument->graph->getImageInfo(output.image);
						ImGui::PushID((int)index);
						ImGui::Text("Colour %zu: %s.v%u", index, image.name.c_str(), output.image.version);
						uint32_t targetImage = output.image.id;
						bool targetChanged = false;
						if (ImGui::BeginCombo("Attachment image", image.name.c_str()))
						{
							for (uint32_t candidate = 0; candidate < openDocument->graph->getImageCount(); ++candidate)
							{
								auto candidateInfo = openDocument->graph->getImageInfo({candidate, 0});
								if (hasGraphImageUsage(candidateInfo.desc.usage, GraphImageUsage::ColourAttachment) &&
								    ImGui::Selectable(candidateInfo.name.c_str(), candidate == targetImage))
								{
									targetImage = candidate;
									targetChanged = targetImage != output.image.id;
								}
							}
							ImGui::EndCombo();
						}
						int mip = (int)output.mipLevel, load = (int)output.load, store = (int)output.store;
						bool changed = targetChanged || ImGui::InputInt("Mip level", &mip);
						if (ImGui::Combo("Load", &load, "Load\0Clear\0Don't care\0"))
							changed = true;
						if (ImGui::Combo("Store", &store, "Store\0Don't care\0"))
							changed = true;
						changed |= ImGui::ColorEdit4("Clear colour", &output.clearColour.x);
						if (changed)
						{
							auto before = clonePipeline(openDocument);
							try
							{
								if (targetChanged)
									output.image = openDocument->graph->retargetColourOutput(
									    {(uint32_t)selectedPass},
									    index,
									    {targetImage,
									     (uint32_t)openDocument->graph->getImageVersionCount(targetImage) - 1});
								openDocument->graph->setColourOutput({(uint32_t)selectedPass},
								                                     index,
								                                     (GraphLoadOp)load,
								                                     (GraphStoreOp)store,
								                                     output.clearColour,
								                                     (uint32_t)std::max(0, mip));
								auto after = clonePipeline(openDocument);
								pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
								                             "Edit Colour Attachment", &openDocument, before, after),
								                         true);
								pipelineDirty = true;
								documentChangedSincePreview = true;
							}
							catch (std::exception const& error)
							{
								previewFailure = error.what();
							}
						}
						if (ImGui::Button("Remove colour attachment"))
						{
							auto before = clonePipeline(openDocument);
							openDocument->graph->removeColourOutput({(uint32_t)selectedPass}, index);
							auto after = clonePipeline(openDocument);
							pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
							    "Remove Colour Attachment", &openDocument, before, after));
							pipelineDirty = true;
							documentChangedSincePreview = true;
							ImGui::PopID();
							break;
						}
						ImGui::PopID();
					}
					for (size_t index = 0; index < info.depthOutputs.size(); ++index)
					{
						auto output = info.depthOutputs[index];
						auto image = openDocument->graph->getImageInfo(output.image);
						ImGui::PushID(1000 + (int)index);
						ImGui::Text("Depth: %s.v%u", image.name.c_str(), output.image.version);
						uint32_t targetImage = output.image.id;
						bool targetChanged = false;
						if (ImGui::BeginCombo("Attachment image", image.name.c_str()))
						{
							for (uint32_t candidate = 0; candidate < openDocument->graph->getImageCount(); ++candidate)
							{
								auto candidateInfo = openDocument->graph->getImageInfo({candidate, 0});
								if (hasGraphImageUsage(candidateInfo.desc.usage, GraphImageUsage::DepthAttachment) &&
								    ImGui::Selectable(candidateInfo.name.c_str(), candidate == targetImage))
								{
									targetImage = candidate;
									targetChanged = targetImage != output.image.id;
								}
							}
							ImGui::EndCombo();
						}
						int mip = (int)output.mipLevel, load = (int)output.load, store = (int)output.store;
						bool changed = targetChanged || ImGui::InputInt("Mip level", &mip);
						if (ImGui::Combo("Load", &load, "Load\0Clear\0Don't care\0"))
							changed = true;
						if (ImGui::Combo("Store", &store, "Store\0Don't care\0"))
							changed = true;
						changed |= ImGui::InputFloat("Clear depth", &output.clearDepth);
						if (changed)
						{
							auto before = clonePipeline(openDocument);
							try
							{
								if (targetChanged)
									output.image = openDocument->graph->retargetDepthOutput(
									    {(uint32_t)selectedPass},
									    index,
									    {targetImage,
									     (uint32_t)openDocument->graph->getImageVersionCount(targetImage) - 1});
								openDocument->graph->setDepthOutput({(uint32_t)selectedPass},
								                                    index,
								                                    (GraphLoadOp)load,
								                                    (GraphStoreOp)store,
								                                    output.clearDepth,
								                                    (uint32_t)std::max(0, mip));
								auto after = clonePipeline(openDocument);
								pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
								                             "Edit Depth Attachment", &openDocument, before, after),
								                         true);
								pipelineDirty = true;
								documentChangedSincePreview = true;
							}
							catch (std::exception const& error)
							{
								previewFailure = error.what();
							}
						}
						if (ImGui::Button("Remove depth attachment"))
						{
							auto before = clonePipeline(openDocument);
							openDocument->graph->removeDepthOutput({(uint32_t)selectedPass}, index);
							auto after = clonePipeline(openDocument);
							pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
							    "Remove Depth Attachment", &openDocument, before, after));
							pipelineDirty = true;
							documentChangedSincePreview = true;
							ImGui::PopID();
							break;
						}
						ImGui::PopID();
					}
					if (ImGui::Button("Add colour attachment"))
					{
						for (uint32_t image = 0; image < openDocument->graph->getImageCount(); ++image)
						{
							auto imageInfo = openDocument->graph->getImageInfo({image, 0});
							if (hasGraphImageUsage(imageInfo.desc.usage, GraphImageUsage::ColourAttachment))
							{
								auto before = clonePipeline(openDocument);
								openDocument->graph->writeColour(
								    {(uint32_t)selectedPass},
								    {image, (uint32_t)openDocument->graph->getImageVersionCount(image) - 1});
								auto after = clonePipeline(openDocument);
								pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
								    "Add Colour Attachment", &openDocument, before, after));
								pipelineDirty = true;
								documentChangedSincePreview = true;
								break;
							}
						}
					}
					ImGui::SameLine();
					if (ImGui::Button("Add depth attachment"))
					{
						for (uint32_t image = 0; image < openDocument->graph->getImageCount(); ++image)
						{
							auto imageInfo = openDocument->graph->getImageInfo({image, 0});
							if (hasGraphImageUsage(imageInfo.desc.usage, GraphImageUsage::DepthAttachment))
							{
								auto before = clonePipeline(openDocument);
								openDocument->graph->writeDepth(
								    {(uint32_t)selectedPass},
								    {image, (uint32_t)openDocument->graph->getImageVersionCount(image) - 1});
								auto after = clonePipeline(openDocument);
								pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
								    "Add Depth Attachment", &openDocument, before, after));
								pipelineDirty = true;
								documentChangedSincePreview = true;
								break;
							}
						}
					}
				}
				if (ImGui::CollapsingHeader("Sampler Bindings", ImGuiTreeNodeFlags_DefaultOpen))
				{
					for (size_t binding = 0; binding < info.samplerBindings.size(); ++binding)
					{
						auto value = info.samplerBindings[binding];
						ImGui::PushID((int)binding);
						char sampler[128]{};
						strncpy_s(sampler, value.sampler.c_str(), 127);
						bool changed = ImGui::InputText("Sampler", sampler, sizeof(sampler));
						std::string preview = openDocument->graph->getValueId(value.image);
						if (ImGui::BeginCombo("Image value", preview.c_str()))
						{
							for (uint32_t image = 0; image < openDocument->graph->getImageCount(); ++image)
								for (uint32_t version = 0; version < openDocument->graph->getImageVersionCount(image);
								     ++version)
								{
									GraphImageHandle candidate{image, version};
									auto label = openDocument->graph->getValueId(candidate);
									if (ImGui::Selectable(label.c_str(),
									                      candidate.id == value.image.id &&
									                          candidate.version == value.image.version))
									{
										value.image = candidate;
										changed = true;
									}
								}
							ImGui::EndCombo();
						}
						int mip = value.mipLevel == UINT32_MAX ? -1 : (int)value.mipLevel;
						if (ImGui::InputInt("Mip (-1 = chain)", &mip))
						{
							value.mipLevel = mip < 0 ? UINT32_MAX : (uint32_t)mip;
							changed = true;
						}
						if (changed)
						{
							auto before = clonePipeline(openDocument);
							try
							{
								openDocument->graph->setSamplerBinding(
								    {(uint32_t)selectedPass}, binding, sampler, value.image, value.mipLevel);
								auto after = clonePipeline(openDocument);
								pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
								                             "Edit Sampler Binding", &openDocument, before, after),
								                         true);
								pipelineDirty = true;
								documentChangedSincePreview = true;
							}
							catch (std::exception const& error)
							{
								previewFailure = error.what();
							}
						}
						ImGui::SameLine();
						if (ImGui::Button("Remove"))
						{
							auto before = clonePipeline(openDocument);
							openDocument->graph->removeSamplerBinding({(uint32_t)selectedPass}, binding);
							auto after = clonePipeline(openDocument);
							pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
							    "Remove Sampler Binding", &openDocument, before, after));
							pipelineDirty = true;
							documentChangedSincePreview = true;
							ImGui::PopID();
							break;
						}
						ImGui::PopID();
					}
					if (ImGui::Button("Add sampler") && openDocument->graph->getImageCount())
					{
						auto before = clonePipeline(openDocument);
						auto name = uniqueName("TEX",
						                       [&](auto const& candidate)
						                       {
							                       return std::any_of(info.samplerBindings.begin(),
							                                          info.samplerBindings.end(),
							                                          [&](auto const& item)
							                                          { return item.sampler == candidate; });
						                       });
						openDocument->graph->bindSampler(
						    {(uint32_t)selectedPass},
						    name,
						    {0, (uint32_t)openDocument->graph->getImageVersionCount(0) - 1});
						auto after = clonePipeline(openDocument);
						pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
						    "Add Sampler Binding", &openDocument, before, after));
						pipelineDirty = true;
						documentChangedSincePreview = true;
					}
				}
				if (ImGui::CollapsingHeader("Uniform Parameters", ImGuiTreeNodeFlags_DefaultOpen))
				{
					bool changed = false;
					for (auto const& entry : info.parameters.getUniformData())
					{
						auto const& value = entry.second;
						auto total = value.count * value.numElements;
						auto parameter = metadata
						                     ? std::find_if(metadata->parameters.begin(),
						                                    metadata->parameters.end(),
						                                    [&](auto const& item) { return item.name == entry.first; })
						                     : std::vector<GraphPassParameterMetadata>::const_iterator{};
						GraphPassParameterMetadata const* parameterInfo =
						    metadata && parameter != metadata->parameters.end() ? &*parameter : nullptr;
						ImGui::Text("%s%s", entry.first.c_str(), value.count > 1 ? " (array)" : "");
						if (parameterInfo && !parameterInfo->uiHint.empty())
						{
							ImGui::SameLine();
							ImGui::TextDisabled("[%s]", parameterInfo->uiHint.c_str());
						}
						ImGui::PushID(entry.first.c_str());
						if (value.type == program::GLSLType::Float || value.type == program::GLSLType::FloatMatrix)
						{
							std::vector<float> values(total);
							memcpy(values.data(), value.data, total * sizeof(float));
							bool edited = false;
							for (size_t component = 0; component < total; ++component)
							{
								ImGui::PushID((int)component);
								if (total == 1 && parameterInfo && parameterInfo->hasRange)
									edited |= ImGui::SliderFloat("##value",
									                             &values[component],
									                             (float)parameterInfo->minimum,
									                             (float)parameterInfo->maximum);
								else
									edited |= ImGui::InputFloat("##value", &values[component]);
								if ((component + 1) % 4)
									ImGui::SameLine();
								ImGui::PopID();
							}
							if (edited)
							{
								info.parameters.setUniform(entry.first,
								                           value.type,
								                           value.count,
								                           value.numElements,
								                           reinterpret_cast<char const*>(values.data()));
								changed = true;
							}
						}
						else if (value.type == program::GLSLType::Double ||
						         value.type == program::GLSLType::DoubleMatrix)
						{
							std::vector<double> values(total);
							memcpy(values.data(), value.data, total * sizeof(double));
							bool edited = false;
							for (size_t component = 0; component < total; ++component)
							{
								ImGui::PushID((int)component);
								edited |= ImGui::InputDouble("##value", &values[component]);
								if ((component + 1) % 4)
									ImGui::SameLine();
								ImGui::PopID();
							}
							if (edited)
							{
								info.parameters.setUniform(entry.first,
								                           value.type,
								                           value.count,
								                           value.numElements,
								                           reinterpret_cast<char const*>(values.data()));
								changed = true;
							}
						}
						else if (value.type == program::GLSLType::Int || value.type == program::GLSLType::Uint ||
						         value.type == program::GLSLType::Bool)
						{
							std::vector<int> values(total);
							memcpy(values.data(), value.data, total * sizeof(int));
							bool edited = false;
							for (size_t component = 0; component < total; ++component)
							{
								ImGui::PushID((int)component);
								if (value.type == program::GLSLType::Bool)
								{
									bool booleanValue = values[component] != 0;
									if (ImGui::Checkbox("##value", &booleanValue))
									{
										values[component] = booleanValue ? 1 : 0;
										edited = true;
									}
								}
								else if (total == 1 && parameterInfo && parameterInfo->hasRange)
									edited |= ImGui::SliderInt("##value",
									                           &values[component],
									                           (int)parameterInfo->minimum,
									                           (int)parameterInfo->maximum);
								else
									edited |= ImGui::InputInt("##value", &values[component]);
								if ((component + 1) % 4)
									ImGui::SameLine();
								ImGui::PopID();
							}
							if (edited)
							{
								info.parameters.setUniform(entry.first,
								                           value.type,
								                           value.count,
								                           value.numElements,
								                           reinterpret_cast<char const*>(values.data()));
								changed = true;
							}
						}
						else
							ImGui::Text("Reflected type %d", (int)value.type);
						ImGui::PopID();
					}
					if (changed)
					{
						auto before = clonePipeline(openDocument);
						openDocument->graph->setPassParameters({(uint32_t)selectedPass}, info.parameters);
						auto after = clonePipeline(openDocument);
						documentChangedSincePreview = true;
						pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
						                             "Edit Pass Parameters", &openDocument, before, after),
						                         true);
						lastEditScene = false;
						pipelineDirty = currentPath.empty() || pipelineCommands.dirty();
					}
				}
			}
			else if (openDocument && openDocument->graph && selectedImage >= 0 &&
			         (size_t)selectedImage < openDocument->graph->getImageCount())
			{
				auto handle = GraphImageHandle{(uint32_t)selectedImage, 0};
				auto info = openDocument->graph->getImageInfo(handle);
				char imageName[256]{};
				strncpy_s(imageName, info.name.c_str(), 255);
				if (ImGui::InputText("Image name", imageName, sizeof(imageName)))
				{
					auto before = clonePipeline(openDocument);
					try
					{
						auto previousName = info.name;
						openDocument->graph->setImageName(handle, imageName);
						for (auto& output : openDocument->outputs)
						{
							if (output.image == previousName)
								output.image = imageName;
							if (output.taaDepth == previousName)
								output.taaDepth = imageName;
						}
						auto after = clonePipeline(openDocument);
						pipelineCommands.execute(
						    std::make_unique<PipelineSnapshotCommand>("Rename Image", &openDocument, before, after),
						    true);
						pipelineDirty = true;
						documentChangedSincePreview = true;
					}
					catch (std::exception const& error)
					{
						previewFailure = error.what();
					}
				}
				bool presentationImage = hasGraphImageUsage(info.desc.usage, GraphImageUsage::Presentation);
				if (presentationImage)
				{
					bool foundOutput = false;
					for (size_t outputIndex = 0; outputIndex < openDocument->outputs.size(); ++outputIndex)
					{
						auto const& presentationOutput = openDocument->outputs[outputIndex];
						if (presentationOutput.image != info.name)
							continue;
						foundOutput = true;
						ImGui::PushID((int)outputIndex + 4000);
						ImGui::SeparatorText(("Presentation output: " + presentationOutput.name).c_str());
						auto sampleIndex = [](std::optional<AntiAliasingSamples> const& value)
						{
							if (!value)
								return 0;
							return (int)*value + 1;
						};
						auto boolIndex = [](std::optional<bool> const& value) { return !value ? 0 : (*value ? 2 : 1); };
						auto setSamples = [&](char const* label,
						                      std::optional<AntiAliasingSamples> AntiAliasingOverrides::* member,
						                      bool shared)
						{
							int selected = sampleIndex(openDocument->outputs[outputIndex].antiAliasing.*member);
							if (ImGui::Combo(label,
							                 &selected,
							                 "Inherit\0Off\0"
							                 "2x\0"
							                 "4x\0"
							                 "8x\0"))
							{
								std::optional<AntiAliasingSamples> value =
								    selected == 0
								        ? std::nullopt
								        : std::optional<AntiAliasingSamples>((AntiAliasingSamples)(selected - 1));
								applyAntiAliasingOverride(
								    label, member, value, shared ? std::nullopt : std::optional<size_t>(outputIndex));
							}
						};
						auto setBoolean =
						    [&](char const* label, std::optional<bool> AntiAliasingOverrides::* member, bool shared)
						{
							int selected = boolIndex(openDocument->outputs[outputIndex].antiAliasing.*member);
							if (ImGui::Combo(label, &selected, "Inherit\0Off\0On\0"))
							{
								std::optional<bool> value =
								    selected == 0 ? std::nullopt : std::optional<bool>(selected == 2);
								applyAntiAliasingOverride(
								    label, member, value, shared ? std::nullopt : std::optional<size_t>(outputIndex));
							}
						};
						setSamples("MSAA", &AntiAliasingOverrides::msaa, true);
						setSamples("SSAA", &AntiAliasingOverrides::ssaa, true);
						setBoolean("TAA", &AntiAliasingOverrides::taa, true);
						setBoolean("FXAA", &AntiAliasingOverrides::fxaa, false);
						auto effective = resolveAntiAliasing(renderSystem.getOptions().antiAliasing,
						                                     openDocument->outputs[outputIndex].antiAliasing);
						ImGui::TextDisabled("Effective: MSAA %s, SSAA %s, TAA %s, FXAA %s",
						                    antiAliasingSamplesName(effective.msaa).c_str(),
						                    antiAliasingSamplesName(effective.ssaa).c_str(),
						                    effective.taa ? "on" : "off",
						                    effective.fxaa ? "on" : "off");
						ImGui::TextDisabled("MSAA, SSAA and TAA are shared by every pipeline output.");
						ImGui::PopID();
					}
					if (!foundOutput)
						ImGui::TextDisabled("This presentation image is not assigned to a named pipeline output.");
				}
				bool changed = false;
				int format = (int)info.desc.format;
				if (ImGui::Combo(
				        "Format",
				        &format,
				        "R8\0RG8\0RGBA8\0SRGB8_ALPHA8\0R16F\0RG16F\0RGBA16F\0R32F\0RG32F\0RGBA32F\0R11G11B10F\0RGB10_"
				        "A2\0DEPTH16\0DEPTH24\0DEPTH32F\0DEPTH24_STENCIL8\0DEPTH32F_STENCIL8\0"))
				{
					info.desc.format = (GraphImageFormat)format;
					bool depthFormat = format >= (int)GraphImageFormat::Depth16;
					info.desc.usage = depthFormat ? GraphImageUsage::DepthAttachment
					                              : (GraphImageUsage::ColourAttachment | GraphImageUsage::Sampled);
					changed = true;
				}
				auto usage = [&](char const* label, GraphImageUsage flag)
				{
					bool enabled = hasGraphImageUsage(info.desc.usage, flag);
					if (ImGui::Checkbox(label, &enabled))
					{
						auto bits = (uint32_t)info.desc.usage;
						if (enabled)
							bits |= (uint32_t)flag;
						else
							bits &= ~(uint32_t)flag;
						info.desc.usage = (GraphImageUsage)bits;
						changed = true;
					}
				};
				usage("Sampled", GraphImageUsage::Sampled);
				ImGui::SameLine();
				usage("Colour attachment", GraphImageUsage::ColourAttachment);
				usage("Depth attachment", GraphImageUsage::DepthAttachment);
				ImGui::SameLine();
				usage("Presentation", GraphImageUsage::Presentation);
				ImGui::SameLine();
				usage("Exported", GraphImageUsage::Exported);
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Keep the final value and every contributing pass live for host-side consumption.");
				int absolute[2] = {(int)info.desc.absoluteSize.x, (int)info.desc.absoluteSize.y};
				if (ImGui::InputInt2("Absolute size (0 = relative)", absolute))
				{
					info.desc.absoluteSize = {(uint32_t)std::max(0, absolute[0]), (uint32_t)std::max(0, absolute[1])};
					changed = true;
				}
				changed |= ImGui::InputFloat2("Relative size", &info.desc.relativeSize.x);
				int mips = (int)info.desc.mipLevels;
				if (ImGui::InputInt("Mip levels", &mips))
				{
					info.desc.mipLevels = (uint32_t)std::max(0, mips);
					changed = true;
				}
				uint32_t minFilters[] = {0x2600, 0x2601, 0x2700, 0x2701, 0x2702, 0x2703},
				         magFilters[] = {0x2600, 0x2601}, wrapModes[] = {0x2901, 0x8370, 0x812F, 0x812D};
				auto optionIndex = [](uint32_t value, uint32_t const* options, size_t count)
				{
					for (size_t index = 0; index < count; ++index)
						if (options[index] == value)
							return (int)index;
					return 0;
				};
				int minFilter = optionIndex(info.desc.params.minFilter, minFilters, 6),
				    magFilter = optionIndex(info.desc.params.magFilter, magFilters, 2),
				    wrap = optionIndex(info.desc.params.wrap, wrapModes, 4), colourSpace = (int)info.desc.colourSpace;
				if (ImGui::Combo("Minimum filter",
				                 &minFilter,
				                 "Nearest\0Linear\0Nearest mip nearest\0Linear mip nearest\0Nearest mip linear\0Linear "
				                 "mip linear\0"))
				{
					info.desc.params.minFilter = minFilters[minFilter];
					changed = true;
				}
				if (ImGui::Combo("Magnification filter", &magFilter, "Nearest\0Linear\0"))
				{
					info.desc.params.magFilter = magFilters[magFilter];
					changed = true;
				}
				if (ImGui::Combo("Wrapping", &wrap, "Repeat\0Mirrored repeat\0Clamp to edge\0Clamp to border\0"))
				{
					info.desc.params.wrap = wrapModes[wrap];
					changed = true;
				}
				if (ImGui::Combo("Colour space", &colourSpace, "Linear\0sRGB\0"))
				{
					info.desc.colourSpace = (TextureColourSpace)colourSpace;
					changed = true;
				}
				changed |= ImGui::Checkbox("Use mipmaps", &info.desc.params.useMipmaps);
				changed |= ImGui::InputInt("LOD base", &info.desc.params.lodBaseLevel);
				changed |= ImGui::InputInt("LOD maximum", &info.desc.params.lodMaxLevel);
				changed |= ImGui::InputFloat("LOD bias", &info.desc.params.lodBias);
				changed |= ImGui::InputFloat("Maximum anisotropy", &info.desc.params.maxAnisotropy);
				changed |= ImGui::Checkbox("External", &info.desc.external);
				changed |= ImGui::Checkbox("Transient", &info.desc.transient);
				std::string importName = info.importName;
				bool importChanged = false;
				if (info.desc.external &&
				    ImGui::BeginCombo("Typed import", importName.empty() ? "(unbound)" : importName.c_str()))
				{
					if (ImGui::Selectable("(unbound)", importName.empty()))
					{
						importName.clear();
						importChanged = true;
					}
					for (auto const& item : openDocument->imports)
						if (ImGui::Selectable(item.id.c_str(), item.id == importName))
						{
							importName = item.id;
							importChanged = true;
						}
					ImGui::EndCombo();
				}
				changed |= importChanged;
				if (changed)
				{
					auto before = clonePipeline(openDocument);
					try
					{
						openDocument->graph->setImageDesc(handle, info.desc);
						if (info.desc.external)
						{
							if (importName.empty())
								openDocument->graph->clearImageImportName(handle);
							else
								openDocument->graph->setImageImportName(handle, importName);
						}
						auto after = clonePipeline(openDocument);
						documentChangedSincePreview = true;
						pipelineCommands.execute(
						    std::make_unique<PipelineSnapshotCommand>("Edit Graph Image", &openDocument, before, after),
						    true);
						lastEditScene = false;
						pipelineDirty = currentPath.empty() || pipelineCommands.dirty();
					}
					catch (std::exception const& error)
					{
						previewFailure = error.what();
					}
				}
			}
			else if (openDocument && selectedImport >= 0 && (size_t)selectedImport < openDocument->imports.size())
			{
				auto before = clonePipeline(openDocument);
				auto& value = openDocument->imports[(size_t)selectedImport];
				int format = (int)value.format;
				bool changed = false;
				char importId[256]{};
				strncpy_s(importId, value.id.c_str(), 255);
				if (ImGui::InputText("Import ID", importId, sizeof(importId)))
				{
					auto old = value.id;
					value.id = importId;
					if (openDocument->graph)
						for (uint32_t image = 0; image < openDocument->graph->getImageCount(); ++image)
							if (openDocument->graph->getImageInfo({image, 0}).importName == old)
							{
								if (value.id.empty())
									openDocument->graph->clearImageImportName({image, 0});
								else
									openDocument->graph->setImageImportName({image, 0}, value.id);
							}
					changed = true;
				}
				if (ImGui::Combo(
				        "Format",
				        &format,
				        "R8\0RG8\0RGBA8\0SRGB8_ALPHA8\0R16F\0RG16F\0RGBA16F\0R32F\0RG32F\0RGBA32F\0R11G11B10F\0RGB10_"
				        "A2\0DEPTH16\0DEPTH24\0DEPTH32F\0DEPTH24_STENCIL8\0DEPTH32F_STENCIL8\0"))
				{
					value.format = (GraphImageFormat)format;
					changed = true;
				}
				auto usage = [&](char const* label, GraphImageUsage flag)
				{
					bool enabled = hasGraphImageUsage(value.usage, flag);
					if (ImGui::Checkbox(label, &enabled))
					{
						auto bits = (uint32_t)value.usage;
						if (enabled)
							bits |= (uint32_t)flag;
						else
							bits &= ~(uint32_t)flag;
						value.usage = (GraphImageUsage)bits;
						changed = true;
					}
				};
				usage("Sampled", GraphImageUsage::Sampled);
				ImGui::SameLine();
				usage("Colour attachment", GraphImageUsage::ColourAttachment);
				usage("Depth attachment", GraphImageUsage::DepthAttachment);
				ImGui::SameLine();
				usage("Presentation", GraphImageUsage::Presentation);
				changed |= ImGui::Checkbox("Required", &value.required);
				char semantic[256]{}, fallback[256]{};
				strncpy_s(semantic, value.semantic.c_str(), 255);
				strncpy_s(fallback, value.fallback.c_str(), 255);
				if (ImGui::InputText("Semantic", semantic, sizeof(semantic)))
				{
					value.semantic = semantic;
					changed = true;
				}
				if (ImGui::InputText("Fallback", fallback, sizeof(fallback)))
				{
					value.fallback = fallback;
					changed = true;
				}
				if (changed)
				{
					auto after = clonePipeline(openDocument);
					documentChangedSincePreview = true;
					pipelineCommands.execute(
					    std::make_unique<PipelineSnapshotCommand>("Edit Typed Import", &openDocument, before, after),
					    true);
					lastEditScene = false;
					pipelineDirty = currentPath.empty() || pipelineCommands.dirty();
				}
			}
			else if (openDocument && selectedLocalResource >= 0 &&
			         (size_t)selectedLocalResource < openDocument->localResources.size())
			{
				auto before = clonePipeline(openDocument);
				auto& value = openDocument->localResources[(size_t)selectedLocalResource];
				auto& definition = value.definition;
				ImGui::Text("Resource kind: %s", localResourceKindName(value.kind));
				if (value.kind == PbrPipelineResourceKind::PbrMaterial)
					showMaterialPassUsage();
				bool changed = false;
				char resourceName[256]{};
				strncpy_s(resourceName, value.name.c_str(), 255);
				if (ImGui::InputText("Resource name", resourceName, sizeof(resourceName)))
				{
					auto old = value.name;
					renameResource(value, resourceName);
					renameResourceReferences(*openDocument, old, value.name);
					changed = true;
				}
				auto text = [&](mpp::data::StructuredData& data, char const* key, char const* label)
				{
					char buffer[512]{};
					if (data.hasEntry(key))
						strncpy_s(buffer, data.getEntry(key).getValue().c_str(), 511);
					if (ImGui::InputText(label, buffer, sizeof(buffer)))
					{
						data.setEntryValue(key, buffer);
						changed = true;
					}
				};
				auto number = [&](mpp::data::StructuredData& data, char const* key, char const* label, float fallback)
				{
					float current = fallback;
					if (data.hasEntry(key))
						try
						{
							current = std::stof(data.getEntry(key).getValue());
						}
						catch (...)
						{
						}
					if (ImGui::InputFloat(label, &current))
					{
						data.setEntryValue(key, std::to_string(current));
						changed = true;
					}
				};
				auto choice = [&](mpp::data::StructuredData& data,
				                  char const* key,
				                  char const* label,
				                  std::vector<std::string> const& choices)
				{
					auto current = data.hasEntry(key) ? data.getEntry(key).getValue() : std::string();
					if (ImGui::BeginCombo(label, current.c_str()))
					{
						for (auto const& item : choices)
							if (ImGui::Selectable(item.c_str(), item == current))
							{
								data.setEntryValue(key, item);
								changed = true;
							}
						ImGui::EndCombo();
					}
				};
				if (value.kind == PbrPipelineResourceKind::Sampler)
				{
					choice(definition,
					       "minFilter",
					       "Min filter",
					       {"NEAREST",
					        "LINEAR",
					        "NEAREST_MIPMAP_NEAREST",
					        "LINEAR_MIPMAP_NEAREST",
					        "NEAREST_MIPMAP_LINEAR",
					        "LINEAR_MIPMAP_LINEAR"});
					choice(definition, "magFilter", "Mag filter", {"NEAREST", "LINEAR"});
					choice(
					    definition, "wrap", "Wrap", {"REPEAT", "MIRRORED_REPEAT", "CLAMP_TO_EDGE", "CLAMP_TO_BORDER"});
					number(definition, "lodMinLevel", "Minimum LOD", -1000);
					number(definition, "lodMaxLevel", "Maximum LOD", 1000);
					number(definition, "lodBias", "LOD bias", 0);
					number(definition, "maxAnisotropy", "Maximum anisotropy", 1);
				}
				else if (value.kind == PbrPipelineResourceKind::Texture)
				{
					text(definition, "filename", "Image file");
					ImGui::SameLine();
					if (ImGui::Button("Browse image"))
						if (auto selected = mpp::app::openImageFileDialog(window.getWindow(), "Select texture image"))
						{
							definition.setEntryValue("filename", *selected);
							changed = true;
						}
					choice(definition, "target", "Target", {"1D", "2D", "3D", "CUBEMAP"});
					text(definition, "internalFormat", "Internal format");
					choice(definition, "colourSpace", "Colour space", {"LINEAR", "SRGB"});
					choice(definition,
					       "minFilter",
					       "Min filter",
					       {"NEAREST",
					        "LINEAR",
					        "NEAREST_MIPMAP_NEAREST",
					        "LINEAR_MIPMAP_NEAREST",
					        "NEAREST_MIPMAP_LINEAR",
					        "LINEAR_MIPMAP_LINEAR"});
					choice(definition, "magFilter", "Mag filter", {"NEAREST", "LINEAR"});
					choice(
					    definition, "wrap", "Wrap", {"REPEAT", "MIRRORED_REPEAT", "CLAMP_TO_EDGE", "CLAMP_TO_BORDER"});
					number(definition, "maxAnisotropy", "Maximum anisotropy", 1);
				}
				else if (value.kind == PbrPipelineResourceKind::PbrMaterial)
				{
					if (definition.hasEntry("Surface"))
					{
						auto* surface = &definition.getEntry("Surface");
						bool albedoMap = definition.hasEntry("BaseColourMap");
						if (ImGui::Checkbox("Use albedo texture", &albedoMap))
						{
							if (albedoMap)
							{
								auto texture = makeLocalResource(PbrPipelineResourceKind::Texture, value.name + ".Albedo").definition;
								texture.setEntryValue("colourSpace", "SRGB");
								mpp::data::StructuredData map("BaseColourMap"); map.addEntry("Resource", texture);
								surface->setEntryValue("baseColourFactor", "1 1 1 1"); definition.addEntry("BaseColourMap", map); surface = &definition.getEntry("Surface");
							}
							else { mpp::data::StructuredData withoutMap(definition.getName()); for (auto const& entry : definition) if (entry.first != "BaseColourMap") withoutMap.addEntry(entry.first, entry.second); definition = std::move(withoutMap); surface = &definition.getEntry("Surface"); }
							changed = true;
						}
						if (albedoMap)
						{
							auto& resource = definition.getEntry("BaseColourMap").getEntry("Resource"); text(resource, "filename", "Albedo texture"); ImGui::SameLine();
							if (ImGui::Button("Browse albedo texture")) if (auto selected = mpp::app::openImageFileDialog(window.getWindow(), "Select albedo image")) { resource.setEntryValue("filename", *selected); changed = true; }
							ImGui::TextDisabled("Albedo textures are sampled as sRGB.");
						}
						else
						{
							glm::vec4 albedo(1.0f); if (surface->hasEntry("baseColourFactor")) { std::istringstream input(surface->getEntry("baseColourFactor").getValue()); input >> albedo.r >> albedo.g >> albedo.b >> albedo.a; }
							if (ImGui::ColorEdit4("Albedo", &albedo.x, ImGuiColorEditFlags_Float)) { surface->setEntryValue("baseColourFactor", std::to_string(albedo.r)+" "+std::to_string(albedo.g)+" "+std::to_string(albedo.b)+" "+std::to_string(albedo.a)); changed = true; }
						}
						number(*surface, "metallicFactor", "Metallic", 1);
						number(*surface, "roughnessFactor", "Roughness", 1);
						bool emissiveMap = definition.hasEntry("EmissiveMap");
						if (ImGui::Checkbox("Use emissive image", &emissiveMap))
						{
							if (emissiveMap)
							{
								auto texture = makeLocalResource(PbrPipelineResourceKind::Texture, value.name + ".EmissiveMap").definition;
								mpp::data::StructuredData map("EmissiveMap"); map.addEntry("Resource", texture);
								surface->setEntryValue("emissiveFactor", "1 1 1");
								definition.addEntry("EmissiveMap", map);
								surface = &definition.getEntry("Surface");
							}
							else
							{
								mpp::data::StructuredData withoutMap(definition.getName());
								for (auto const& entry : definition)
									if (entry.first != "EmissiveMap") withoutMap.addEntry(entry.first, entry.second);
								definition = std::move(withoutMap);
								surface = &definition.getEntry("Surface");
							}
							changed = true;
						}
						if (emissiveMap)
						{
							auto& resource = definition.getEntry("EmissiveMap").getEntry("Resource");
							text(resource, "filename", "Emissive image");
							ImGui::SameLine();
							if (ImGui::Button("Browse emissive image"))
								if (auto selected = mpp::app::openImageFileDialog(window.getWindow(), "Select emissive RGB image"))
								{
									resource.setEntryValue("filename", *selected);
									changed = true;
								}
							ImGui::TextDisabled("Emissive images are sampled as sRGB RGB and multiplied by the factor below.");
							glm::vec3 multiplier(1.0f);
							if (surface->hasEntry("emissiveFactor")) { std::istringstream input(surface->getEntry("emissiveFactor").getValue()); input >> multiplier.r >> multiplier.g >> multiplier.b; }
							if (ImGui::ColorEdit3("Emissive multiplier", &multiplier.x, ImGuiColorEditFlags_Float)) { surface->setEntryValue("emissiveFactor", std::to_string(multiplier.r) + " " + std::to_string(multiplier.g) + " " + std::to_string(multiplier.b)); changed = true; }
						}
						else
						{
							glm::vec3 emissive(0.0f);
							if (surface->hasEntry("emissiveFactor"))
							{
								std::istringstream input(surface->getEntry("emissiveFactor").getValue());
								input >> emissive.r >> emissive.g >> emissive.b;
							}
							if (ImGui::ColorEdit3("Emissive colour", &emissive.x, ImGuiColorEditFlags_Float))
							{
								surface->setEntryValue("emissiveFactor", std::to_string(emissive.r) + " " + std::to_string(emissive.g) + " " + std::to_string(emissive.b));
								changed = true;
							}
						}
						number(*surface, "normalScale", "Normal scale", 1);
						number(*surface, "occlusionStrength", "Occlusion strength", 1);
						text(*surface, "alphaMode", "Alpha mode");
						number(*surface, "alphaCutoff", "Alpha cutoff", 0.5f);
						text(*surface, "doubleSided", "Double sided");
						// Water (screen-space reflections). Adding the block selects the
						// PbrMaterialFeature::Water specialization; it needs a pipeline
						// whose graph carries an MPP.WaterScene pass, otherwise the
						// material shades with the cubemap only. See doc/WATER_SSR.md.
						if (surface->hasEntry("Water"))
						{
							auto& water = surface->getEntry("Water");
							ImGui::SeparatorText("Water (screen-space reflections)");
							text(water, "enabled", "Water enabled");
							ImGui::TextDisabled("Distortion strength 0 is a raw mirror; useful for checking the ray march.");
							number(water, "distortionScale", "Distortion scale", 6.0f);
							number(water, "distortionStrength", "Distortion strength", 0.06f);
							text(water, "scrollSpeed", "Scroll speed (x y)");
							number(water, "microRoughness", "Micro roughness", 0.05f);
							ImGui::TextDisabled("Steps trade quality for cost linearly; see doc/WATER_SSR.md.");
							number(water, "ssrMaxDistance", "SSR max distance", 40.0f);
							number(water, "ssrSteps", "SSR steps", 32.0f);
							number(water, "ssrThickness", "SSR thickness", 0.5f);
							number(water, "edgeFade", "Screen-edge fade", 0.1f);
							ImGui::TextDisabled("Below the grazing end the reflection is the cubemap only, which is the pre-SSR look.");
							number(water, "grazingFallbackStart", "Grazing fallback start", 0.35f);
							number(water, "grazingFallbackEnd", "Grazing fallback end", 0.1f);
							if (ImGui::SmallButton("Remove Water"))
							{
								mpp::data::StructuredData withoutWater(surface->getName());
								for (auto const& entry : *surface) if (entry.first != "Water") withoutWater.addEntry(entry.first, entry.second);
								*surface = std::move(withoutWater);
								changed = true;
							}
						}
						else if (ImGui::SmallButton("Add Water (SSR)"))
						{
							surface->addEntry("Water", mpp::data::StructuredData("Water"));
							changed = true;
						}
					}
					else
						ImGui::TextDisabled("Add a Surface block through a template before editing factors.");
					if (ImGui::CollapsingHeader("PBR Maps and Extensions"))
					{
						char const* maps[] = {
						    "MetallicMap", "RoughnessMap", "MetallicRoughnessMap", "NormalMap", "OcclusionMap",
						    // Only meaningful alongside a Surface/Water block; without one the
						    // shader has no PBR_SPEC_WATER path to sample them from.
						    "WaterNormalMap", "WaterDetailNormalMap"};
						for (auto map : maps)
						{
							if (definition.hasEntry(map))
							{
								auto& block = definition.getEntry(map);
								ImGui::PushID(map);
								ImGui::TextUnformatted(map);
								if (std::string_view(map) == "MetallicMap" || std::string_view(map) == "RoughnessMap")
								{
									choice(block, "channel", "Channel", {"R", "G", "B", "A"});
									ImGui::TextDisabled("Single-channel images use R; choose a component for RGB/RGBA images.");
								}
								if (std::string_view(map) == "EmissiveMap") ImGui::TextDisabled("Emissive maps are sampled as RGB.");
								if (block.hasEntry("Resource"))
								{
									auto& resource = block.getEntry("Resource");
									text(resource, "filename", "Image file");
									text(resource, "colourSpace", "Colour space");
									text(resource, "minFilter", "Minimum filter");
									text(resource, "magFilter", "Magnification filter");
									text(resource, "wrap", "Wrapping");
								}
								ImGui::PopID();
							}
							else
							{
								ImGui::PushID(map);
								if (ImGui::SmallButton((std::string("Add ") + map).c_str()))
								{
									auto texture =
									    makeLocalResource(PbrPipelineResourceKind::Texture, value.name + "." + map)
									        .definition;
									mpp::data::StructuredData block(map);
									block.addEntry("Resource", texture);
									if (std::string_view(map) == "MetallicMap" || std::string_view(map) == "RoughnessMap") block.addEntry("channel", "R");
									if (definition.hasEntry("Surface"))
									{
										auto& surface = definition.getEntry("Surface");
										if (std::string_view(map) == "MetallicMap") surface.setEntryValue("metallicFactor", "1");
										if (std::string_view(map) == "EmissiveMap") surface.setEntryValue("emissiveFactor", "1 1 1");
									}
									definition.addEntry(map, block);
									changed = true;
								}
								ImGui::PopID();
							}
						}
						if (definition.hasEntry("Extensions"))
						{
							auto const& extensions = definition.getEntry("Extensions");
							ImGui::Text("Extension payload entries: %zu",
							            (size_t)std::distance(extensions.begin(), extensions.end()));
						}
						else if (ImGui::Button("Add PBR extension texture"))
						{
							mpp::data::StructuredData extensions("Extensions"), texture("Texture");
							texture.addEntry("name", "PBR_EXT_CUSTOM_MAP");
							texture.addEntry(
							    "Resource",
							    makeLocalResource(PbrPipelineResourceKind::Texture, value.name + ".Extension")
							        .definition);
							extensions.addEntry("Texture", texture);
							definition.addEntry("Extensions", extensions);
							changed = true;
						}
					}
					if (definition.hasEntry("Program"))
						ImGui::TextUnformatted("Program definition present (reflection validates on Apply). ");
					if (auto runtimeMaterial =
					        std::dynamic_pointer_cast<PbrMaterial>(pipelineRuntime.getResolvedResource(value.name)))
					{
						ImGui::Separator();
						ImGui::Text("Active features: %s", runtimeMaterial->getFeatureSummary().c_str());
						ImGui::Text("Program: %s",
						            runtimeMaterial->getProgram() ? runtimeMaterial->getProgram()->getName().c_str()
						                                          : "unresolved");
					}
				}
				else if (value.kind == PbrPipelineResourceKind::ParticleEffect)
				{
					text(definition, "name", "Asset name");
					if (definition.hasEntry("Emitters")) ImGui::Text("Emitter templates: %zu", (size_t)std::distance(definition.getEntry("Emitters").begin(), definition.getEntry("Emitters").end()));
					ImGui::TextDisabled("Edit emitter-template details in the serialized resource document.");
				}
				else
				{
					choice(definition, "positionType", "Position type", {"2D", "3D"});
					number(definition, "textures", "Texture slots", 0);
					if (definition.hasEntry("VertexShader"))
						ImGui::TextUnformatted("Vertex shader definition present.");
					if (definition.hasEntry("FragmentShader"))
						ImGui::TextUnformatted("Fragment shader definition present.");
					ImGui::TextDisabled("Shader source remains read-only; edit file/resource references in XML.");
					if (auto runtimeProgram =
					        std::dynamic_pointer_cast<Program>(pipelineRuntime.getResolvedResource(value.name)))
					{
						ImGui::Separator();
						ImGui::Text("Active reflection (%zu uniforms)", runtimeProgram->getUniformNames().size());
						for (auto const& uniform : runtimeProgram->getUniformNames())
							ImGui::BulletText(
							    "%s (GL 0x%X)", uniform.c_str(), runtimeProgram->getUniformGlType(uniform));
					}
				}
				if (changed)
				{
					auto after = clonePipeline(openDocument);
					documentChangedSincePreview = true;
					pipelineCommands.execute(
					    std::make_unique<PipelineSnapshotCommand>("Edit Local Resource", &openDocument, before, after),
					    true);
					lastEditScene = false;
					pipelineDirty = currentPath.empty() || pipelineCommands.dirty();
				}
			}
			else if (openDocument && selectedExternalResource >= 0 &&
			         (size_t)selectedExternalResource < openDocument->externalResources.size())
			{
				auto const& value = openDocument->externalResources[(size_t)selectedExternalResource];
				auto qualified = value.libraryName + "::" + value.resource.name;
				ImGui::Text("External resource: %s", qualified.c_str());
				ImGui::Text("Library: %s", value.libraryPath.c_str());
				ImGui::Text("Type: %s", value.resource.definition.getName().c_str());
				if (value.resource.kind == PbrPipelineResourceKind::PbrMaterial)
					showMaterialPassUsage();
				ImGui::TextDisabled("Read-only");
				if (ImGui::Button("Make Local Copy"))
				{
					auto localName = value.resource.name + ".Local";
					unsigned suffix = 2;
					while (std::any_of(openDocument->localResources.begin(),
					                   openDocument->localResources.end(),
					                   [&](auto const& current) { return current.name == localName; }))
						localName = value.resource.name + ".Local" + std::to_string(suffix++);
					auto before = clonePipeline(openDocument);
					if (openDocument->makeLocalCopy(qualified, localName))
					{
						auto after = clonePipeline(openDocument);
						documentChangedSincePreview = true;
						pipelineCommands.execute(
						    std::make_unique<PipelineSnapshotCommand>("Make Local Copy", &openDocument, before, after));
						selectedLocalResource = (int)openDocument->localResources.size() - 1;
						selectedExternalResource = -1;
						lastEditScene = false;
						pipelineDirty = currentPath.empty() || pipelineCommands.dirty();
					}
				}
			}
			else if (openDocument && selectedBinding == -2)
			{
				auto before = clonePipeline(openDocument);
				auto& value = openDocument->environment;
				bool changed = false;
				auto field = [&](char const* label, std::string& target)
				{
					char text[256]{};
					strncpy_s(text, target.c_str(), 255);
					if (ImGui::InputText(label, text, sizeof(text)))
					{
						target = text;
						changed = true;
					}
				};
				field("Environment binding", value.binding);
				ImGui::SeparatorText("HDR IBL Environment");
				field("HDR equirectangular EXR", value.hdrEquirectangular);
				ImGui::SameLine();
				if (ImGui::Button("Browse EXR")) if (auto selected = mpp::app::openHdrExrFileDialog(window.getWindow(), "Select HDR IBL OpenEXR environment")) { std::error_code error; auto base = openDocument->sourcePath.empty() ? std::filesystem::current_path() : std::filesystem::path(openDocument->sourcePath).parent_path(); auto relative = std::filesystem::relative(*selected, base, error); value.hdrEquirectangular = error ? *selected : relative.generic_string(); changed = true; }
				ImGui::SameLine();
				if (ImGui::Button("Clear HDR IBL")) { value.hdrEquirectangular.clear(); value.environmentResolution = 512; value.irradianceResolution = 32; value.prefilterResolution = 128; changed = true; }
				if (!value.hdrEquirectangular.empty()) { ImGui::SameLine(); if (ImGui::Button("Regenerate HDR IBL")) forceWorkingPreviewRebuild(); bool cached = false; std::error_code cacheError; auto hdrPath = std::filesystem::path(value.hdrEquirectangular); if (hdrPath.is_relative()) hdrPath = std::filesystem::path(openDocument->sourcePath).parent_path() / hdrPath; hdrPath = std::filesystem::absolute(hdrPath, cacheError).lexically_normal(); if (!cacheError) { IblEnvironmentCacheKey key; key.source = hdrPath; key.environmentResolution = value.environmentResolution; key.irradianceResolution = value.irradianceResolution; key.prefilterResolution = value.prefilterResolution; cached = renderSystem.getIblEnvironmentCache().find(key) != nullptr; } ImGui::TextDisabled("HDR IBL status: %s", !previewFailure.empty() ? "failed (see diagnostics)" : previewStale ? "pending rebuild" : cached ? "active preview; cache hit" : "active preview; cache miss/generating"); }
				int environmentResolution = (int)value.environmentResolution, irradianceResolution = (int)value.irradianceResolution, prefilterResolution = (int)value.prefilterResolution;
				if (ImGui::InputInt("Environment cubemap resolution", &environmentResolution)) { value.environmentResolution = (uint32_t)std::max(1, environmentResolution); changed = true; }
				if (ImGui::InputInt("Irradiance cubemap resolution", &irradianceResolution)) { value.irradianceResolution = (uint32_t)std::max(1, irradianceResolution); changed = true; }
				if (ImGui::InputInt("Prefilter cubemap resolution", &prefilterResolution)) { value.prefilterResolution = (uint32_t)std::max(1, prefilterResolution); changed = true; }
				bool const hdrIblActive = !value.hdrEquirectangular.empty();
				ImGui::TextDisabled("EXR source is linear HDR equirectangular; derived cubemaps are renderer-generated.");
				if (hdrIblActive) { ImGui::TextDisabled("Generated HDR IBL maps take precedence over manual irradiance/prefilter maps."); ImGui::BeginDisabled(); }
				field("Irradiance texture", value.irradiance);
				field("Prefiltered specular", value.prefilteredSpecular);
				if (hdrIblActive) ImGui::EndDisabled();
				field("BRDF LUT (advanced override)", value.brdfLut);
				ImGui::TextDisabled("Leave BRDF LUT empty to use the renderer-generated integration LUT.");
				field("Background texture", value.background);
				if (changed)
				{
					auto after = clonePipeline(openDocument);
					pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
					                             "Edit Pipeline Environment", &openDocument, before, after),
					                         true);
					pipelineDirty = true;
					documentChangedSincePreview = true;
					lastEditScene = false;
				}
			}
			else if (openDocument && selectedBinding == -3)
			{
				auto before = clonePipeline(openDocument);
				bool bloomEnabled = openDocument->bloom.enabled;
				bool changed = ImGui::Checkbox("Enable bloom", &bloomEnabled);
				if (changed) openDocument->setBloomEnabled(bloomEnabled);
				uint32_t horizontal = 0, vertical = 0;
				if (openDocument->graph)
					for (uint32_t pass = 0; pass < openDocument->graph->getPassCount(); ++pass)
					{
						auto factory = openDocument->graph->getPassInfo({pass}).callbackFactory;
						if (factory == "MPP.BloomBlurHorizontal")
							++horizontal;
						else if (factory == "MPP.BloomBlurVertical")
							++vertical;
					}
				auto authoredPairs = std::min(horizontal, vertical);
				int blurPasses = (int)openDocument->bloom.blurPasses;
				if (ImGui::InputInt("Blur passes", &blurPasses))
				{
					openDocument->bloom.blurPasses = (uint32_t)std::clamp(blurPasses, 0, 64);
					changed = true;
				}
				ImGui::TextDisabled("Authored horizontal/vertical pairs: %u", authoredPairs);
				if (openDocument->bloom.blurPasses > authoredPairs)
					ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.2f, 1.0f),
					                   "The requested count exceeds the authored blur chain.");
				if (changed)
				{
					auto after = clonePipeline(openDocument);
					pipelineCommands.execute(
					    std::make_unique<PipelineSnapshotCommand>("Edit Bloom Settings", &openDocument, before, after),
					    true);
					pipelineDirty = true;
					documentChangedSincePreview = true;
					lastEditScene = false;
				}
			}
			else if (openDocument && selectedBinding == -4)
			{
				auto before = clonePipeline(openDocument);
				auto& ambientOcclusion = openDocument->ambientOcclusion;
				int selectedMethod = (int)ambientOcclusion.method;
				bool methodChanged = ImGui::Combo("Method", &selectedMethod, "None\0SSAO\0GTAO\0");
				bool changed = methodChanged;
				if (methodChanged) openDocument->setAmbientOcclusionMethod((AmbientOcclusionMethod)selectedMethod);
				if (ambientOcclusion.method == AmbientOcclusionMethod::Ssao)
				{
					auto& options = ambientOcclusion.ssao;
					changed |= ImGui::InputFloat("Radius", &options.radius);
					changed |= ImGui::InputFloat("Intensity", &options.intensity);
					changed |= ImGui::InputFloat("Bias", &options.bias);
					changed |= ImGui::InputFloat("Power", &options.power);
					changed |= ImGui::InputInt("Sample count", &options.sampleCount);
					changed |= ImGui::InputInt("Blur radius", &options.blurRadius);
				}
				else if (ambientOcclusion.method == AmbientOcclusionMethod::Gtao)
				{
					auto& options = ambientOcclusion.gtao;
					changed |= ImGui::InputFloat("Radius", &options.radius);
					changed |= ImGui::InputFloat("Intensity", &options.intensity);
					changed |= ImGui::InputFloat("Thickness", &options.thickness);
					changed |= ImGui::InputFloat("Horizon bias", &options.horizonBias);
					changed |= ImGui::InputFloat("Falloff start", &options.falloffStart);
					changed |= ImGui::InputFloat("Falloff end", &options.falloffEnd);
					changed |= ImGui::InputInt("Slice count", &options.sliceCount);
					changed |= ImGui::InputInt("Steps per slice", &options.stepsPerSlice);
					changed |= ImGui::InputFloat("Power", &options.power);
					changed |= ImGui::InputInt("Blur radius", &options.blurRadius);
					int normalSource = (int)options.normalSource;
					if (ImGui::Combo("Normal source", &normalSource, "Depth reconstruction\0MRT shading normals\0"))
					{
						options.normalSource = (GTAONormalSource)normalSource;
						changed = true;
					}
				}
				if (changed && !methodChanged && ambientOcclusion.method != AmbientOcclusionMethod::None)
					openDocument->setAmbientOcclusionMethod(ambientOcclusion.method);

				bool aoActive = ambientOcclusion.method == AmbientOcclusionMethod::None;
				if (openDocument->graph && ambientOcclusion.method != AmbientOcclusionMethod::None)
				{
					auto compilation = openDocument->graph->compile();
					bool raw = false, blur = false, composite = false, mrtContract = ambientOcclusion.gtao.normalSource != GTAONormalSource::Mrt;
					GraphImageHandle normals;
					for (auto pass : compilation.passOrder)
					{
						auto const& info = openDocument->graph->getPassInfo(pass);
						if (info.callbackFactory == "MPP.PbrScene" && info.colourOutputs.size() >= 3 && openDocument->graph->getImageInfo(info.colourOutputs[2].image).desc.format == GraphImageFormat::Rg16f)
							normals = info.colourOutputs[2].image;
						raw |= info.callbackFactory == (ambientOcclusion.method == AmbientOcclusionMethod::Ssao ? "MPP.SSAORaw" : "MPP.GTAORaw");
						blur |= info.callbackFactory == "MPP.AmbientOcclusionBlur";
						composite |= info.callbackFactory == "MPP.AmbientOcclusionComposite";
						if (info.callbackFactory == "MPP.GTAORaw") for (auto const& binding : info.samplerBindings)
							if (binding.sampler == "NORMALS" && normals.isValid() && binding.image.id == normals.id) mrtContract = true;
					}
					aoActive = compilation.valid && raw && blur && composite && mrtContract;
				}
				ImGui::TextColored(aoActive ? ImVec4(0.3f, 0.9f, 0.4f, 1.0f) : ImVec4(0.9f, 0.6f, 0.25f, 1.0f),
				                   aoActive ? "Ambient occlusion graph is valid" : "Ambient occlusion graph is invalid or bypassed in the compiled graph");
				if (changed)
				{
					auto after = clonePipeline(openDocument);
					pipelineCommands.execute(
					    std::make_unique<PipelineSnapshotCommand>("Edit Ambient Occlusion", &openDocument, before, after),
					    true);
					pipelineDirty = true;
					documentChangedSincePreview = true;
					lastEditScene = false;
				}
			}
			else if (openDocument && selectedBinding >= 0 &&
			         (size_t)selectedBinding < openDocument->previewBindings.size())
			{
				auto before = clonePipeline(openDocument);
				auto& value = openDocument->previewBindings[(size_t)selectedBinding];
				char binding[256]{};
				strncpy_s(binding, value.binding.c_str(), 255);
				bool changed = ImGui::InputText("Logical binding", binding, sizeof(binding));
				if (changed)
					value.binding = binding;
				if (ImGui::BeginCombo("PBR material", value.materialResource.c_str()))
				{
					for (auto const& item : openDocument->localResources)
						if (item.kind == PbrPipelineResourceKind::PbrMaterial &&
						    ImGui::Selectable(item.name.c_str(), item.name == value.materialResource))
						{
							value.materialResource = item.name;
							changed = true;
						}
					for (auto const& item : openDocument->externalResources)
						if (item.resource.kind == PbrPipelineResourceKind::PbrMaterial)
						{
							auto name = item.libraryName + "::" + item.resource.name;
							if (ImGui::Selectable(name.c_str(), name == value.materialResource))
							{
								value.materialResource = name;
								changed = true;
							}
						}
					ImGui::EndCombo();
				}
				if (changed)
				{
					auto after = clonePipeline(openDocument);
					pipelineCommands.execute(
					    std::make_unique<PipelineSnapshotCommand>("Edit Preview Binding", &openDocument, before, after),
					    true);
					pipelineDirty = true;
					documentChangedSincePreview = true;
					lastEditScene = false;
				}
			}
			else if (openDocument && selectedOverride >= 0 &&
			         (size_t)selectedOverride < openDocument->previewOverrides.size())
			{
				auto before = clonePipeline(openDocument);
				auto& value = openDocument->previewOverrides[(size_t)selectedOverride];
				char model[256]{}, binding[256]{};
				strncpy_s(model, value.modelId.c_str(), 255);
				strncpy_s(binding, value.binding.c_str(), 255);
				bool changed = false;
				if (ImGui::InputText("Model ID", model, sizeof(model)))
				{
					value.modelId = model;
					changed = true;
				}
				if (ImGui::InputText("Binding", binding, sizeof(binding)))
				{
					value.binding = binding;
					changed = true;
				}
				for (auto const& entry : value.values.getUniformData())
				{
					auto uniform = entry.second;
					auto total = uniform.count * uniform.numElements;
					ImGui::PushID(entry.first.c_str());
					ImGui::Text("%s%s", entry.first.c_str(), uniform.count > 1 ? " (array)" : "");
					if (uniform.type == program::GLSLType::Float || uniform.type == program::GLSLType::FloatMatrix)
					{
						std::vector<float> values(total);
						memcpy(values.data(), uniform.data, total * sizeof(float));
						bool edited = false;
						for (size_t component = 0; component < total; ++component)
						{
							ImGui::PushID((int)component);
							edited |= ImGui::InputFloat("##component", &values[component]);
							if ((component + 1) % 4)
								ImGui::SameLine();
							ImGui::PopID();
						}
						if (edited)
						{
							value.values.setUniform(entry.first,
							                        uniform.type,
							                        uniform.count,
							                        uniform.numElements,
							                        reinterpret_cast<char const*>(values.data()));
							changed = true;
						}
					}
					else if (uniform.type == program::GLSLType::Int || uniform.type == program::GLSLType::Bool)
					{
						std::vector<int> values(total);
						memcpy(values.data(), uniform.data, total * sizeof(int));
						bool edited = false;
						for (size_t component = 0; component < total; ++component)
						{
							ImGui::PushID((int)component);
							edited |= ImGui::InputInt("##component", &values[component]);
							if ((component + 1) % 4)
								ImGui::SameLine();
							ImGui::PopID();
						}
						if (edited)
						{
							value.values.setUniform(entry.first,
							                        uniform.type,
							                        uniform.count,
							                        uniform.numElements,
							                        reinterpret_cast<char const*>(values.data()));
							changed = true;
						}
					}
					ImGui::PopID();
				}
				if (ImGui::Button("Add float override"))
				{
					auto name = uniqueName("PBR_VALUE",
					                       [&](auto const& id) { return value.values.getUniformData().contains(id); });
					float initial = 0;
					value.values.setUniform(
					    name, program::GLSLType::Float, 1, 1, reinterpret_cast<char const*>(&initial));
					changed = true;
				}
				if (changed)
				{
					auto after = clonePipeline(openDocument);
					pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>(
					                             "Edit Instance Override", &openDocument, before, after),
					                         true);
					pipelineDirty = true;
					documentChangedSincePreview = true;
					lastEditScene = false;
				}
			}
			else if (openScene && selectedModel >= 0)
			{
				auto before = std::make_shared<SceneDocument>(*openScene);
				auto& model = openScene->models[(size_t)selectedModel];
				bool changed = false;
				char modelId[256]{}, modelFile[512]{};
				strncpy_s(modelId, model.id.c_str(), 255);
				strncpy_s(modelFile, model.file.c_str(), 511);
				if (ImGui::InputText("Model ID", modelId, sizeof(modelId)))
				{
					model.id = modelId;
					changed = true;
				}
				int source = (int)model.source;
				if (ImGui::Combo("Source", &source, "MPP model\0Box\0Sphere\0Cylinder\0Grid\0"))
				{
					model.source = (SceneModelSource)source;
					changed = true;
				}
				if (model.source == SceneModelSource::MppModel &&
				    ImGui::InputText("Model file", modelFile, sizeof(modelFile)))
				{
					model.file = modelFile;
					changed = true;
				}
				changed |= ImGui::InputFloat3("Translation", &model.translation.x);
				changed |= ImGui::InputFloat3("Rotation (degrees)", &model.rotationDegrees.x);
				changed |= ImGui::InputFloat3("Scale", &model.scale.x);
				if (model.source == SceneModelSource::Box)
				{
					changed |= ImGui::InputFloat("Width", &model.primitive.width);
					changed |= ImGui::InputFloat("Height", &model.primitive.height);
					changed |= ImGui::InputFloat("Depth", &model.primitive.depth);
				}
				else if (model.source == SceneModelSource::Sphere)
				{
					changed |= ImGui::InputFloat("Radius", &model.primitive.radius);
					int resolution = (int)model.primitive.resolution;
					if (ImGui::InputInt("Resolution", &resolution))
					{
						model.primitive.resolution = (uint32_t)std::max(0, resolution);
						changed = true;
					}
				}
				else if (model.source == SceneModelSource::Cylinder)
				{
					changed |= ImGui::InputFloat("Length", &model.primitive.height);
					changed |= ImGui::InputFloat("Bottom radius", &model.primitive.radius);
					changed |= ImGui::InputFloat("Top radius", &model.primitive.topRadius);
					int resolution = (int)model.primitive.resolution;
					if (ImGui::InputInt("Resolution", &resolution))
					{
						model.primitive.resolution = (uint32_t)std::max(0, resolution);
						changed = true;
					}
				}
				else if (model.source == SceneModelSource::Grid)
				{
					changed |= ImGui::InputFloat("Width", &model.primitive.width);
					changed |= ImGui::InputFloat("Depth", &model.primitive.depth);
					int x = (int)model.primitive.segmentsX, z = (int)model.primitive.segmentsZ;
					if (ImGui::InputInt("X segments", &x))
					{
						model.primitive.segmentsX = (uint32_t)std::max(0, x);
						changed = true;
					}
					if (ImGui::InputInt("Z segments", &z))
					{
						model.primitive.segmentsZ = (uint32_t)std::max(0, z);
						changed = true;
					}
					changed |= ImGui::InputFloat("Texture repeat U", &model.primitive.textureRepeatU);
					changed |= ImGui::InputFloat("Texture repeat V", &model.primitive.textureRepeatV);
				}
				changed |= ImGui::Checkbox("Visible", &model.visible);
				changed |= ImGui::Checkbox("Shadow caster", &model.shadowCaster);
				auto materialPreview = model.materialBinding.empty() ? std::string("(none)") : model.materialBinding;
				if (ImGui::BeginCombo("Material binding", materialPreview.c_str()))
				{
					if (ImGui::Selectable("(none)", model.materialBinding.empty()))
					{
						model.materialBinding.clear();
						changed = true;
					}
					if (openDocument)
						for (size_t index = 0; index < openDocument->previewBindings.size(); ++index)
						{
							auto const& binding = openDocument->previewBindings[index];
							auto label = binding.binding + (binding.materialResource.empty()
							                                    ? std::string()
							                                    : "  [" + binding.materialResource + "]");
							ImGui::PushID((int)index);
							if (ImGui::Selectable(label.c_str(), model.materialBinding == binding.binding))
							{
								model.materialBinding = binding.binding;
								changed = true;
							}
							ImGui::PopID();
						}
					ImGui::EndCombo();
				}
				if (openDocument && !model.materialBinding.empty())
				{
					auto binding = std::find_if(openDocument->previewBindings.begin(), openDocument->previewBindings.end(),
					                            [&](auto const& value) { return value.binding == model.materialBinding; });
					if (binding != openDocument->previewBindings.end())
						ImGui::TextDisabled("PbrScene material: %s", binding->materialResource.c_str());
				}
				if (openDocument && ImGui::Button("Import glTF material..."))
				{
					if (auto selected = mpp::app::openGltfFileDialog(window.getWindow(), "Import glTF 2.0 material"))
						try
						{
							auto loaded = resource_parsers::GltfPbrMaterialLoader::loadFirstMaterial(*selected);
							PbrPipelineResourceDocument importedMaterial;
							importedMaterial.name = loaded.materialName;
							importedMaterial.kind = PbrPipelineResourceKind::PbrMaterial;
							importedMaterial.definition = std::move(loaded.definition);
							std::vector<PbrPipelineResourceDocument> imported;
							imported.push_back(std::move(importedMaterial));
							auto pipelineBefore = clonePipeline(openDocument);
							std::string assignedBinding;
							for (auto& material : imported)
							{
								auto base = material.name.empty() ? "ImportedMaterial" : material.name;
								unsigned suffix = 2;
								auto resourceExists = [&](std::string const& name) { return std::any_of(openDocument->localResources.begin(), openDocument->localResources.end(), [&](auto const& item) { return item.name == name; }); };
								while (resourceExists(material.name)) material.name = base + "." + std::to_string(suffix++);
								material.definition.setEntryValue("name", material.name);
								std::string binding = "Imported." + material.name;
								suffix = 2;
								auto bindingExists = [&](std::string const& name) { return std::any_of(openDocument->previewBindings.begin(), openDocument->previewBindings.end(), [&](auto const& item) { return item.binding == name; }); };
								while (bindingExists(binding)) binding = "Imported." + material.name + "." + std::to_string(suffix++);
								openDocument->localResources.push_back(std::move(material));
								openDocument->previewBindings.push_back({binding, openDocument->localResources.back().name});
								if (assignedBinding.empty()) assignedBinding = binding;
							}
							model.materialBinding = assignedBinding;
							changed = true;
							auto pipelineAfter = clonePipeline(openDocument);
							pipelineCommands.execute(std::make_unique<PipelineSnapshotCommand>("Import glTF Materials", &openDocument, pipelineBefore, pipelineAfter));
							pipelineDirty = true;
							documentChangedSincePreview = true;
						}
						catch (std::exception const& error) { previewFailure = error.what(); }
				}
				std::string layerSummary;
				for (auto const& layer : model.layers)
				{
					if (!layerSummary.empty())
						layerSummary += ", ";
					layerSummary += layer;
				}
				if (layerSummary.empty())
					layerSummary = "(none)";
				if (ImGui::BeginCombo("Layers", layerSummary.c_str()))
				{
					std::vector<std::string> availableLayers = openScene->layers;
					for (auto const& layer : model.layers)
						if (std::find(availableLayers.begin(), availableLayers.end(), layer) == availableLayers.end())
							availableLayers.push_back(layer);
					if (ImGui::SmallButton("All"))
					{
						model.layers = openScene->layers;
						changed = true;
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("None"))
					{
						model.layers.clear();
						changed = true;
					}
					ImGui::Separator();
					for (size_t index = 0; index < availableLayers.size(); ++index)
					{
						auto const& layer = availableLayers[index];
						auto found = std::find(model.layers.begin(), model.layers.end(), layer);
						bool selected = found != model.layers.end();
						bool declared = std::find(openScene->layers.begin(), openScene->layers.end(), layer) !=
						                openScene->layers.end();
						auto label = declared ? layer : layer + "  [undeclared]";
						ImGui::PushID((int)index);
						if (ImGui::Selectable(label.c_str(), selected, ImGuiSelectableFlags_DontClosePopups))
						{
							if (selected)
								model.layers.erase(std::remove(model.layers.begin(), model.layers.end(), layer),
								                   model.layers.end());
							else
								model.layers.push_back(layer);
							changed = true;
						}
						ImGui::PopID();
					}
					ImGui::EndCombo();
				}
				if (changed)
				{
					auto after = std::make_shared<SceneDocument>(*openScene);
					documentChangedSincePreview = true;
					sceneCommands.execute(
					    std::make_unique<SceneSnapshotCommand>("Edit Scene Model", &openScene, before, after), true);
					lastEditScene = true;
					sceneDirty = scenePath.empty() || sceneCommands.dirty();
				}
			}
			else if (openScene && selectedModel == -2)
			{
				auto before = std::make_shared<SceneDocument>(*openScene);
				auto& value = openScene->camera;
				ImGui::TextUnformatted("Camera");
				bool changed = false;
				changed |= ImGui::InputFloat3("Position", &value.position.x);
				changed |= ImGui::InputFloat3("Target", &value.target.x);
				changed |= ImGui::InputFloat("Vertical FOV", &value.fov);
				changed |= ImGui::InputFloat("Near plane", &value.nearPlane);
				changed |= ImGui::InputFloat("Far plane", &value.farPlane);
				if (changed)
				{
					auto after = std::make_shared<SceneDocument>(*openScene);
					documentChangedSincePreview = true;
					sceneCommands.execute(
					    std::make_unique<SceneSnapshotCommand>("Edit Scene Camera", &openScene, before, after), true);
					lastEditScene = true;
					sceneDirty = scenePath.empty() || sceneCommands.dirty();
				}
			}
			else if (openScene && selectedModel == -4)
			{
				char layers[512]{};
				std::string text;
				for (auto const& layer : openScene->layers)
				{
					if (!text.empty())
						text += ",";
					text += layer;
				}
				strncpy_s(layers, text.c_str(), 511);
				if (ImGui::InputText("Declared layers (comma separated)", layers, sizeof(layers)))
				{
					auto before = std::make_shared<SceneDocument>(*openScene);
					openScene->layers.clear();
					std::stringstream stream(layers);
					std::string layer;
					while (std::getline(stream, layer, ','))
						if (!layer.empty())
							openScene->layers.push_back(layer);
					auto after = std::make_shared<SceneDocument>(*openScene);
					documentChangedSincePreview = true;
					sceneCommands.execute(
					    std::make_unique<SceneSnapshotCommand>("Edit Render Layers", &openScene, before, after), true);
					lastEditScene = true;
					sceneDirty = scenePath.empty() || sceneCommands.dirty();
				}
			}
			else if (openScene && selectedModel == -3)
			{
				char value[256]{};
				strncpy_s(value, openScene->environmentBinding.c_str(), 255);
				if (ImGui::InputText("Environment binding", value, sizeof(value)))
				{
					auto before = std::make_shared<SceneDocument>(*openScene);
					openScene->environmentBinding = value;
					auto after = std::make_shared<SceneDocument>(*openScene);
					documentChangedSincePreview = true;
					sceneCommands.execute(
					    std::make_unique<SceneSnapshotCommand>("Edit Environment Binding", &openScene, before, after),
					    true);
					lastEditScene = true;
					sceneDirty = scenePath.empty() || sceneCommands.dirty();
				}
			}
			else if (openScene && selectedModel <= -100 && selectedModel > -10000)
			{
				auto index = (size_t)(-100 - selectedModel);
				if (index < openScene->lights.size())
				{
					auto before = std::make_shared<SceneDocument>(*openScene);
					auto& value = openScene->lights[index];
					bool changed = false;
					char lightId[256]{};
					strncpy_s(lightId, value.id.c_str(), 255);
					if (ImGui::InputText("Light ID", lightId, sizeof(lightId)))
					{
						value.id = lightId;
						changed = true;
					}
					int type = value.type == SceneLightType::Point ? 1 : 0;
					if (ImGui::Combo("Type", &type, "Directional\0Point\0"))
					{
						value.type = type ? SceneLightType::Point : SceneLightType::Directional;
						changed = true;
					}
					changed |= ImGui::InputFloat3("Position", &value.position.x);
					changed |= ImGui::InputFloat3("Direction", &value.direction.x);
					changed |= ImGui::ColorEdit3("Colour", &value.colour.x);
					changed |= ImGui::InputFloat("Intensity", &value.intensity);
					changed |= ImGui::InputFloat("Range", &value.range);
					changed |= ImGui::Checkbox("Casts shadows", &value.castsShadows);
					if (changed)
					{
						auto after = std::make_shared<SceneDocument>(*openScene);
						documentChangedSincePreview = true;
						sceneCommands.execute(
						    std::make_unique<SceneSnapshotCommand>("Edit Scene Light", &openScene, before, after),
						    true);
						lastEditScene = true;
						sceneDirty = scenePath.empty() || sceneCommands.dirty();
					}
				}
			}
			else if (openScene && selectedModel <= -10000)
			{
				auto index = (size_t)(-10000 - selectedModel);
				if (index < openScene->particleEffects.size())
				{
					auto before = std::make_shared<SceneDocument>(*openScene);
					auto& value = openScene->particleEffects[index];
					bool changed = false;
					char id[256]{}; strncpy_s(id, value.id.c_str(), 255);
					if (ImGui::InputText("Particle effect ID", id, sizeof(id))) { value.id = id; changed = true; }
					auto preview = value.effect.empty() ? std::string("(none)") : value.effect;
					if (ImGui::BeginCombo("Particle effect resource", preview.c_str()))
					{
						if (ImGui::Selectable("(none)", value.effect.empty())) { value.effect.clear(); changed = true; }
						if (openDocument) for (auto const& resource : openDocument->localResources) if (resource.kind == PbrPipelineResourceKind::ParticleEffect)
							if (ImGui::Selectable(resource.name.c_str(), value.effect == resource.name)) { value.effect = resource.name; changed = true; }
						ImGui::EndCombo();
					}
					changed |= ImGui::InputFloat3("Translation", &value.translation.x);
					changed |= ImGui::InputFloat3("Rotation (degrees)", &value.rotationDegrees.x);
					changed |= ImGui::InputFloat3("Scale", &value.scale.x);
					changed |= ImGui::Checkbox("Visible", &value.visible);
					if (changed)
					{
						auto after = std::make_shared<SceneDocument>(*openScene); documentChangedSincePreview = true;
						sceneCommands.execute(std::make_unique<SceneSnapshotCommand>("Edit Particle Effect", &openScene, before, after), true);
						lastEditScene = true; sceneDirty = scenePath.empty() || sceneCommands.dirty();
					}
				}
			}
			else
				ImGui::TextUnformatted("Select a pipeline pass or scene item.");
			ImGui::End();
			if (requestValidateFocus)
				ImGui::SetWindowFocus("Diagnostics");
			ImGui::Begin("Diagnostics");
			if (openDocument)
			{
				auto diagnostics = openDocument->validate(renderSystem.getCaps());
				diagnostics.append(openDocument->validateOutputAntiAliasing(renderSystem.getOptions().antiAliasing,
				                                                            &renderSystem.getCaps()));
				diagnostics.append(resource_parsers::validatePbrPipelineResourceDefinitions(*openDocument));
				if (openScene)
				{
					diagnostics.append(openScene->validate());
					if (openScene->environmentBinding != openDocument->environment.binding)
						diagnostics.error(
						    "MPP-SCENE-RUNTIME-007",
						    "Scene environment binding does not match the active pipeline environment binding.",
						    {openScene->sourcePath},
						    "environment");
				}
				diagnostics.append(pipelineRuntime.getDiagnostics());
				diagnostics.append(sceneRuntime.getDiagnostics());
				auto compiler = openDocument->graph ? openDocument->graph->compile(renderSystem.getCaps(), {viewportWidth, viewportHeight}) : RenderGraphCompileResult{};
				size_t compilerErrors = 0, compilerWarnings = 0, compilerInformation = 0;
				for (auto const& message : compiler.messages)
				{
					if (message.severity == GraphCompileMessageSeverity::Error) ++compilerErrors;
					else if (message.severity == GraphCompileMessageSeverity::Warning) ++compilerWarnings;
					else ++compilerInformation;
				}
				ImGui::Text("%zu error(s), %zu warning(s), %zu compiler info",
				            diagnostics.count(DiagnosticSeverity::Error) + compilerErrors,
				            diagnostics.count(DiagnosticSeverity::Warning) + compilerWarnings,
				            compilerInformation);
				for (auto const& d : diagnostics.getDiagnostics())
					ImGui::BulletText("[%s] %s", d.code.c_str(), d.message.c_str());
				for (size_t index = 0; index < compiler.messages.size(); ++index)
				{
					auto const& message = compiler.messages[index];
					auto colour = message.severity == GraphCompileMessageSeverity::Error ? ImVec4(1.0f, 0.35f, 0.3f, 1.0f) :
						message.severity == GraphCompileMessageSeverity::Warning ? ImVec4(1.0f, 0.68f, 0.18f, 1.0f) : ImVec4(0.45f, 0.75f, 1.0f, 1.0f);
					ImGui::PushStyleColor(ImGuiCol_Text, colour);
					auto label = "[" + message.code + "] " + message.message + "##compiler" + std::to_string(index);
					if (ImGui::Selectable(label.c_str()))
					{
						selectedPass = message.pass.isValid() ? (int)message.pass.id : -1;
						selectedImage = message.image.isValid() ? (int)message.image.id : -1;
						selectedImport = selectedBinding = selectedOverride = selectedModel = selectedLocalResource = selectedExternalResource = -1;
					}
					ImGui::PopStyleColor();
				}
			}
			else
				ImGui::TextColored(ImVec4(0.3f, 1, 0.4f, 1), "No document loaded");
			ImGui::End();
			ImGui::Begin("Allocations");
			if (openDocument && openDocument->graph)
			{
				auto artifact = openDocument->graph->buildCompiledPlan(renderSystem.getCaps(), {viewportWidth, viewportHeight});
				auto const& compiled = artifact.compilation;
				auto const& plan = artifact.allocation;
				size_t reordered = 0;
				for (auto const& message : compiled.messages) if (message.code == "RG-COMPILER-REORDERED-PASS") ++reordered;
				ImGui::Text("Compiler: %zu executed, %zu culled, %zu reordered, %zu unused output%s",
				            compiled.passOrder.size(), compiled.culledPasses.size(), reordered, compiled.unusedOutputs.size(), compiled.unusedOutputs.size() == 1 ? "" : "s");
				auto cacheStats = openDocument->graph->getPlanCacheStats();
				ImGui::TextDisabled("Plan cache: compile %llu hit / %llu miss, allocation %llu hit / %llu miss, %llu invalidation%s",
				                    (unsigned long long)cacheStats.compileHits, (unsigned long long)cacheStats.compileMisses,
				                    (unsigned long long)cacheStats.allocationHits, (unsigned long long)cacheStats.allocationMisses,
				                    (unsigned long long)cacheStats.invalidations, cacheStats.invalidations == 1 ? "" : "s");
				if (plan.valid)
				{
					ImGui::Text("Physical estimate: %.2f MiB", plan.estimatedPhysicalBytes / 1048576.0);
					for (auto const& image : plan.allocatedImages)
						ImGui::BulletText("%s.v%u -> allocation %u, %.1f KiB, passes %u-%u",
						                  image.debugName.c_str(),
						                  image.image.version,
						                  image.physicalAllocation,
						                  image.estimatedBytes / 1024.0,
						                  image.firstPass,
						                  image.lastPass);
				}
				else
					ImGui::TextUnformatted("Allocation unavailable while graph is invalid.");
			}
			ImGui::End();
			ImGui::Begin("Statistics");
			ImGui::Text("Viewport: %u x %u", viewportWidth, viewportHeight);
			ImGui::Text("Frame: %.3f ms (%.1f FPS)", fps > 0 ? 1000.0f / fps : 0.0f, fps);
			if (!activeGraphResource.empty())
			{
				auto const& stats = renderSystem.getRenderPipeline(activePipeline)->getLastGraphExecutionStats();
				double totalCpu = 0, totalGpu = 0;
				uint64_t totalTriangles = 0;
				size_t gpuPasses = 0;
				for (auto const& pass : stats)
				{
					totalCpu += pass.cpuMilliseconds;
					totalTriangles += pass.trianglesSubmitted;
					if (pass.gpuTimingAvailable)
					{
						totalGpu += pass.gpuMilliseconds;
						++gpuPasses;
						ImGui::BulletText("%s: %.3f ms CPU / %.3f ms GPU, %llu triangles, %llu fullscreen",
						                  pass.name.c_str(),
						                  pass.cpuMilliseconds,
						                  pass.gpuMilliseconds,
						                  (unsigned long long)pass.trianglesSubmitted,
						                  (unsigned long long)pass.fullscreenQuads);
					}
					else
						ImGui::BulletText(pass.gpuTimingSupported
						                      ? "%s: %.3f ms CPU / GPU pending, %llu triangles, %llu fullscreen"
						                      : "%s: %.3f ms CPU / GPU unavailable, %llu triangles, %llu fullscreen",
						                  pass.name.c_str(),
						                  pass.cpuMilliseconds,
						                  (unsigned long long)pass.trianglesSubmitted,
						                  (unsigned long long)pass.fullscreenQuads);
				}
				ImGui::Separator();
				if (gpuPasses == stats.size() && !stats.empty())
					ImGui::Text("Graph total: %.3f ms CPU / %.3f ms GPU", totalCpu, totalGpu);
				else
					ImGui::Text(stats.empty() || stats.front().gpuTimingSupported
					                ? "Graph total: %.3f ms CPU / GPU pending"
					                : "Graph total: %.3f ms CPU / GPU unavailable",
					            totalCpu);
				ImGui::Text("%llu submitted triangles", (unsigned long long)totalTriangles);
			}
			ImGui::End();
			if (documentChangedSincePreview && !activeGraphResource.empty())
			{
				previewStale = true;
				if (previewFailure.empty())
					previewFailure = "Working changes have not been applied; showing the last valid generation.";
			}
			ImGui::Begin("Viewport");
			if (previewStale)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.15f, 1.0f), "STALE PREVIEW");
				ImGui::TextWrapped("%s", previewFailure.c_str());
			}
			else if (!previewFailure.empty())
				ImGui::TextColored(
				    ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Preview rebuild failed: %s", previewFailure.c_str());
			ImGui::Checkbox("Inspect selected image", &inspectSelectedImage);
			bool inspectedDepth = false;
			bool inspectionControlsAvailable = false;
			int inspectedVersions = 1, inspectedMipLevels = 1;
			if (inspectSelectedImage && activePreviewDocument && activePreviewDocument->graph && selectedImage >= 0 &&
			    (uint32_t)selectedImage < activePreviewDocument->graph->getImageCount())
			{
				inspectedVersions = (int)activePreviewDocument->graph->getImageVersionCount((uint32_t)selectedImage);
				inspectedVersion = std::clamp(inspectedVersion, 0, std::max(0, inspectedVersions - 1));
				auto imageInfo = activePreviewDocument->graph->getImageInfo({(uint32_t)selectedImage, (uint32_t)inspectedVersion});
				inspectedDepth = imageInfo.desc.format >= GraphImageFormat::Depth16;
				inspectedMipLevels = (int)imageInfo.desc.mipLevels;
				inspectedMip = std::clamp(inspectedMip, 0, std::max(0, inspectedMipLevels - 1));
				inspectionControlsAvailable = true;
			}
			if (inspectionControlsAvailable && inspectedDepth)
			{
				textureDiagnosticMode = (int)RenderSystem::TextureDiagnosticMode::Depth;
				ImGui::SameLine();
				ImGui::SetNextItemWidth(180);
				if (ImGui::DragFloat("Depth value range scale", &diagnosticDepthRangeScale,
				                     std::max(diagnosticDepthRangeScale * 0.01f, 0.000001f),
				                     0.0000001f, 100000.0f, "%.7g", ImGuiSliderFlags_Logarithmic))
					diagnosticDepthFar = diagnosticDepthNear + std::max(diagnosticDepthRangeScale, 0.0000001f);
				ImGui::SameLine();
				ImGui::SetNextItemWidth(180);
				ImGui::DragFloatRange2("Depth range", &diagnosticDepthNear, &diagnosticDepthFar,
				                       std::max(diagnosticDepthRangeScale * 0.01f, 0.000001f),
				                       0.0f, 100000.0f, "Near %.7g", "Far %.7g");
				diagnosticDepthFar = std::max(diagnosticDepthNear + 0.0000001f, diagnosticDepthFar);
				diagnosticDepthRangeScale = diagnosticDepthFar - diagnosticDepthNear;
			}

			if (ImGui::Button("Reset View") && openScene)
			{
				setOrbitView(openScene->camera.position, openScene->camera.target);
				camera->setFov(openScene->camera.fov);
			}
			ImGui::SameLine();
			if (ImGui::Button("Frame Selection") && openScene && selectedModel >= 0 &&
			    (size_t)selectedModel < openScene->models.size())
			{
				auto const& model = openScene->models[(size_t)selectedModel];
				orbitTarget = model.translation;
				orbitDistance = std::max(2.0f, glm::length(model.scale) * 2.5f);
				updateOrbitCamera();
			}
			ImGui::SameLine();
			if (ImGui::Button("Save Current View") && openScene)
			{
				auto before = std::make_shared<SceneDocument>(*openScene);
				openScene->camera.position = camera->getPosition();
				openScene->camera.target = orbitTarget;
				openScene->camera.fov = camera->getFov();
				openScene->camera.nearPlane = camera->getNearClipDistance();
				openScene->camera.farPlane = camera->getFarClipDistance();
				auto after = std::make_shared<SceneDocument>(*openScene);
				documentChangedSincePreview = true;
				sceneCommands.execute(
				    std::make_unique<SceneSnapshotCommand>("Save Current View", &openScene, before, after));
				lastEditScene = true;
				sceneDirty = scenePath.empty() || sceneCommands.dirty();
			}
			if (inspectionControlsAvailable)
			{
				ImGui::SameLine();
				ImGui::SetNextItemWidth(120);
				ImGui::SliderInt("Version", &inspectedVersion, 0, std::max(0, inspectedVersions - 1));
				ImGui::SameLine();
				ImGui::SetNextItemWidth(120);
				ImGui::SliderInt("Mip", &inspectedMip, 0, std::max(0, inspectedMipLevels - 1));
				ImGui::SameLine();
				if (inspectedDepth)
				{
					ImGui::SetNextItemWidth(192);
					ImGui::BeginDisabled();
					int depthMode = 0;
					ImGui::Combo("Visualisation", &depthMode, "Depth\0");
					ImGui::EndDisabled();
				}
				else
				{
					int modes[] = {0, 1, 2, 3, 4, 5, 7, 8}, selectedMode = 0;
					for (int index = 0; index < 8; ++index)
						if (modes[index] == textureDiagnosticMode) selectedMode = index;
					ImGui::SetNextItemWidth(192);
					if (ImGui::Combo("Visualisation", &selectedMode,
					                 "Colour\0Red\0Green\0Blue\0Alpha\0Luminance\0HDR tone map\0HDR heat map\0"))
						textureDiagnosticMode = modes[selectedMode];
					if (textureDiagnosticMode == 7 || textureDiagnosticMode == 8)
					{
						ImGui::SameLine();
						ImGui::SetNextItemWidth(140);
						ImGui::SliderFloat("Exposure", &diagnosticExposure, 0.01f, 16.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
					}
				}
			}
			if (inspectionControlsAvailable)
			{
				auto imageInfo = activePreviewDocument->graph->getImageInfo(
				    {(uint32_t)selectedImage, (uint32_t)inspectedVersion});
				ImGui::TextDisabled(
				    inspectedDepth ? "%s / maps [near, near + scale] to black-to-white" : "%s%s",
				    graphImageFormatName(imageInfo.desc.format),
				    hasGraphImageUsage(imageInfo.desc.usage, GraphImageUsage::Sampled) ? "" : " / diagnostic resolve");
				if (!inspectedDepth && textureDiagnosticMode == (int)RenderSystem::TextureDiagnosticMode::Depth)
					textureDiagnosticMode = (int)RenderSystem::TextureDiagnosticMode::Colour;
				if (!textureDiagnosticFailure.empty())
				{
					ImGui::SameLine();
					ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
					                   "Inspection failed: %s", textureDiagnosticFailure.c_str());
				}
			}
			ImGui::TextDisabled("MMB / Alt+left: orbit | Shift: pan | Ctrl: dolly | Wheel: dolly");
			auto viewportSize = ImGui::GetContentRegionAvail();
			auto requestedWidth = (uint32_t)std::max(1.0f, viewportSize.x),
			     requestedHeight = (uint32_t)std::max(1.0f, viewportSize.y);
			if (activePreviewTarget && (requestedWidth != viewportWidth || requestedHeight != viewportHeight))
			{
				try
				{
					pipelineRuntime.resize(requestedWidth, requestedHeight);
					if (activePreviewDocument)
					{
						std::map<std::string, RenderTargetPtr> resizedDestinations;
						for (auto const& output : activePreviewDocument->outputs)
							for (uint32_t image = 0; image < activePreviewDocument->graph->getImageCount(); ++image)
							{
								auto info = activePreviewDocument->graph->getImageInfo({image, 0});
								if (info.name != output.image || !info.desc.external)
									continue;
								auto destination = pipelineRuntime.getImports().find(info.importName);
								if (destination != pipelineRuntime.getImports().end())
									resizedDestinations.emplace(output.name, destination->second);
								break;
							}
						if (resizedDestinations.size() == activePreviewDocument->outputs.size())
							renderSystem.getRenderPipeline(activePipeline)
							    ->prepareOutputs(*activePreviewDocument->graph, resizedDestinations);
					}
					if (activePreviewTexture)
						provider->unregisterTexture(activePreviewTexture);
					viewportWidth = requestedWidth;
					viewportHeight = requestedHeight;
					if (diagnosticTexture)
						provider->unregisterTexture(diagnosticTexture);
					diagnosticTarget->resize(viewportWidth, viewportHeight);
					diagnosticTexture = provider->registerTexture(diagnosticTarget);
					scene->setViewport(0, 0, viewportWidth, viewportHeight);
					camera->setAspectRatio(float(viewportWidth) / float(viewportHeight));
					if (auto texture = std::dynamic_pointer_cast<RenderTexture>(activePreviewTarget))
						activePreviewTexture = provider->registerTexture(texture);
				}
				catch (std::exception const& error)
				{
					try
					{
						pipelineRuntime.resize(viewportWidth, viewportHeight);
					}
					catch (...)
					{
					}
					previewFailure =
					    std::string("Preview resize failed; prior resources were retained: ") + error.what();
					previewStale = true;
				}
			}
			ImTextureID displayTexture = activePreviewTexture;
			if (inspectSelectedImage && activePreviewDocument && activePreviewDocument->graph && selectedImage >= 0 &&
			    (uint32_t)selectedImage < activePreviewDocument->graph->getImageCount() && !activeGraphResource.empty())
			{
				inspectedTarget =
				    renderSystem.getRenderPipeline(activePipeline)
				        ->getGraphImageRenderTarget({(uint32_t)selectedImage, (uint32_t)inspectedVersion});
				if (inspectedTarget)
					displayTexture = diagnosticTexture;
				else
					displayTexture = 0;
			}
			else if (inspectSelectedImage)
				displayTexture = 0;
			auto viewportCanvasSize = ImVec2((float)requestedWidth, (float)requestedHeight);
			auto viewportOrigin = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton("##ViewportCanvas",
			                       viewportCanvasSize,
			                       ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);

			bool viewportHovered = ImGui::IsItemHovered();
			bool viewportActive = ImGui::IsItemActive();
			auto& io = ImGui::GetIO();
			bool middleDrag = viewportActive && ImGui::IsMouseDown(ImGuiMouseButton_Middle);
			bool trackpadDrag = viewportActive && io.KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left);
			bool cameraChanged = false;

			if ((middleDrag && !io.KeyShift && !io.KeyCtrl) || (trackpadDrag && !io.KeyShift && !io.KeyCtrl))
			{
				orbitYaw -= io.MouseDelta.x * 0.008f;
				orbitPitch = std::clamp(orbitPitch - io.MouseDelta.y * 0.008f, -1.5f, 1.5f);
				cameraChanged = io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f;
			}
			else if ((middleDrag && io.KeyShift) || (trackpadDrag && io.KeyShift))
			{
				auto right = glm::normalize(glm::cross(camera->getDirection(), camera->getUp()));
				auto scale = orbitDistance * 0.002f;
				orbitTarget += right * (-io.MouseDelta.x * scale) + camera->getUp() * (io.MouseDelta.y * scale);
				cameraChanged = io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f;
			}
			else if ((middleDrag && io.KeyCtrl) || (trackpadDrag && io.KeyCtrl))
			{
				orbitDistance = std::clamp(orbitDistance * std::exp(io.MouseDelta.y * 0.01f), 0.05f, 100000.0f);
				cameraChanged = io.MouseDelta.y != 0.0f;
			}

			if (viewportHovered && io.MouseWheel != 0.0f)
			{
				orbitDistance = std::clamp(orbitDistance * std::pow(0.85f, io.MouseWheel), 0.05f, 100000.0f);
				cameraChanged = true;
			}

			if (cameraChanged)
			{
				updateOrbitCamera();
			}

			auto viewportMaximum =
			    ImVec2(viewportOrigin.x + viewportCanvasSize.x, viewportOrigin.y + viewportCanvasSize.y);
			if (displayTexture)
			{
				ImGui::GetWindowDrawList()->AddImage(
				    displayTexture, viewportOrigin, viewportMaximum, ImVec2(0, 1), ImVec2(1, 0));
			}
			else
			{
				ImGui::GetWindowDrawList()->AddText(viewportOrigin,
				                                    ImGui::GetColorU32(ImGuiCol_Text),
				                                    inspectSelectedImage
				                                        ? "Selected image is not allocated in the current generation."
				                                        : "No presentation import is available.");
			}
			ImGui::End();
			ImGui::SetNextWindowPos(ImVec2(0, ImGui::GetIO().DisplaySize.y - 24));
			ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x, 24));
			ImGui::Begin("##Status",
			             nullptr,
			             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
			auto statusWork = backgroundJobs.progress();
			std::string gpuFrameTime = "GPU unavailable";
			if (activeGraphResource.empty())
			{
				gpuFrameTimeSamples.clear();
				gpuFrameTimeSum = 0;
				gpuFrameTimePipeline.clear();
			}
			else
			{
				if (gpuFrameTimePipeline != activePipeline)
				{
					gpuFrameTimeSamples.clear();
					gpuFrameTimeSum = 0;
					gpuFrameTimePipeline = activePipeline;
				}
				auto const& gpuStats = renderSystem.getRenderPipeline(activePipeline)->getLastGraphExecutionStats();
				bool supported =
				    !gpuStats.empty() && std::all_of(gpuStats.begin(),
				                                     gpuStats.end(),
				                                     [](auto const& pass) { return pass.gpuTimingSupported; });
				bool available =
				    !gpuStats.empty() && std::all_of(gpuStats.begin(),
				                                     gpuStats.end(),
				                                     [](auto const& pass) { return pass.gpuTimingAvailable; });
				if (available)
				{
					double total = 0;
					for (auto const& pass : gpuStats)
						total += pass.gpuMilliseconds;
					gpuFrameTimeSamples.push_back(total);
					gpuFrameTimeSum += total;
					if (gpuFrameTimeSamples.size() > 30)
					{
						gpuFrameTimeSum -= gpuFrameTimeSamples.front();
						gpuFrameTimeSamples.pop_front();
					}
				}
				if (!gpuFrameTimeSamples.empty())
				{
					std::ostringstream text;
					text << std::fixed << std::setprecision(3) << (gpuFrameTimeSum / gpuFrameTimeSamples.size())
					     << " ms GPU avg";
					gpuFrameTime = text.str();
				}
				else if (supported)
					gpuFrameTime = "GPU pending";
			}
			ImGui::Text(
			    "%s%s | %.1f FPS | %s | %d submitted triangles | %llu known unique | Preview: %s%s",
			    openDocument ? "Loaded" : "Ready",
			    (pipelineDirty || sceneDirty) ? " *" : "",
			    fps,
			    gpuFrameTime.c_str(),
			    renderSystem.getCurrentRenderInfo().trianglesRendered,
			    (unsigned long long)(sceneRuntime.getScene() ? sceneRuntime.getUniqueTriangleCount()
			                                                 : (openScene ? openScene->getKnownTriangleCount() : 0)),
			    openDocument ? (previewStale ? "stale last-valid generation" : "current generation") : "no document",
			    (statusWork.queued || statusWork.running) ? (" | " + statusWork.stage).c_str()
			    : gpuInstallationPending                  ? " | installing GPU generation"
			                                              : "");
			if (openScene && openScene->getUnknownTriangleModelCount() && ImGui::IsItemHovered())
				ImGui::SetTooltip("%zu visible .mppmodel source(s) are not included until model metadata is loaded.",
				                  openScene->getUnknownTriangleModelCount());
			ImGui::End();
			if (!ImGui::IsAnyItemActive())
			{
				pipelineCommands.endCoalescing();
				sceneCommands.endCoalescing();
			}
			ImGui::Render();
			provider->setDrawData(ImGui::GetDrawData());
			renderSystem.startStatsCollection();
			renderDocCapture.discardUnexpectedCapture();
			bool captureThisFrame = false;
			bool openCompletedCapture = false;
			if (captureRequested)
			{
				captureRequested = false;
				openCompletedCapture = captureAndOpen;
				auto currentCaptureWork = backgroundJobs.progress();
				bool captureStillEnabled =
				    !currentCaptureWork.queued && !currentCaptureWork.running && !gpuInstallationPending;
				try
				{
					if (captureStillEnabled && ensureRenderDocSettings())
					{
						renderDocCapture.initialise(editorSettings.renderDocExecutable);
						renderDocCapture.begin(editorSettings.captureDirectory);
						captureThisFrame = true;
					}
				}
				catch (std::exception const& error)
				{
					reportOperationError("RenderDoc Capture Failed",
					                     "Could not start the viewport capture.",
					                     editorSettings.captureDirectory.string(),
					                     error);
				}
			}

			try
			{
				renderSystem.renderScene(scene, camera, glm::vec2(0), activePipeline);
			}
			catch (...)
			{
				if (captureThisFrame)
					try
					{
						renderDocCapture.end();
					}
					catch (...)
					{
					}
				throw;
			}

			if (!captureThisFrame)
				renderDocCapture.discardUnexpectedCapture();
			if (captureThisFrame)
			{
				try
				{
					auto capturePath = renderDocCapture.end();
					if (openCompletedCapture)
						launchRenderDoc(editorSettings.renderDocExecutable, capturePath);
					operationMessageIsSuccess = true;
					operationErrorTitle = openCompletedCapture ? "Capture Opened in RenderDoc" : "Viewport Captured";
					operationErrorMessage = "Created viewport-only capture:\n" + capturePath.string();
					openOperationError = true;
				}
				catch (std::exception const& error)
				{
					reportOperationError("RenderDoc Capture Failed",
					                     "Could not complete the viewport capture.",
					                     editorSettings.captureDirectory.string(),
					                     error);
				}
			}

			if (inspectSelectedImage && inspectedTarget && activePreviewDocument && activePreviewDocument->graph &&
			    selectedImage >= 0 && (uint32_t)selectedImage < activePreviewDocument->graph->getImageCount())
			{
				try
				{
					auto source = std::dynamic_pointer_cast<RenderTexture>(inspectedTarget);
					if (!source)
						throw std::runtime_error("The selected import is not texture-backed and cannot be inspected.");
					auto info = activePreviewDocument->graph->getImageInfo(
					    {(uint32_t)selectedImage, (uint32_t)inspectedVersion});
					bool depth = info.desc.format >= GraphImageFormat::Depth16;
					if (source->isMultisampled())
					{
						auto existing = std::dynamic_pointer_cast<RenderTexture>(diagnosticResolveTarget);
						if (!existing || existing->getWidth() != source->getWidth() ||
						    existing->getHeight() != source->getHeight() || diagnosticResolveDepth != depth)
						{
							RenderTextureOptions options;
							if (depth)
							{
								options.numAttachments = 0;
								options.depthAttachment = RenderTextureDepthAttachment::DepthTexture;
								options.depthFormat = RenderTextureDepthFormat::Depth32f;
							}
							else
							{
								options.colourType = TextureInternalType::Float;
								options.colourNormalised = false;
								options.colourBitSize = 16;
								options.colourChannels = 4;
							}
							diagnosticResolveTarget =
							    renderSystem.createRenderTexture("PipelineEditor.ImageDiagnosticResolve",
							                                     source->getWidth(),
							                                     source->getHeight(),
							                                     options);
							diagnosticResolveDepth = depth;
							existing = std::dynamic_pointer_cast<RenderTexture>(diagnosticResolveTarget);
						}
						source->resolveTo(existing.get(), !depth, depth);
						source = existing;
					}
					RenderSystem::TextureDiagnosticOptions options;
					options.mode = (RenderSystem::TextureDiagnosticMode)textureDiagnosticMode;
					options.mipLevel = (uint32_t)inspectedMip;
					options.exposure = diagnosticExposure;
					options.depthNear = diagnosticDepthNear;
					options.depthFar = diagnosticDepthFar;
					renderSystem.renderTextureDiagnostic(source.get(), diagnosticTarget, options);
					textureDiagnosticFailure.clear();
				}
				catch (std::exception const& error)
				{
					textureDiagnosticFailure = error.what();
				}
			}
			renderer.render(&renderSystem);
			renderSystem.finishStatsCollection();
			window.show();
			if (smokeTest)
			{
				if (!activeGraphResource.empty())
				{
					if (++smokeStableFrames >= 30)
					{
						auto pipeline = renderSystem.getRenderPipeline(activePipeline);
						auto snapshot = pipeline->getLastFlowSnapshot();
						// A pipeline may legitimately author no passes; the Empty
						// starting template does. Telemetry must still be published
						// and internally coherent, but pass- and batch-derived
						// invariants are vacuous for such a graph and asserting them
						// would reject a valid pipeline. Derive the expectations from
						// what the document actually declares instead.
						auto const& smokeGraph = *activePreviewDocument->graph;
						size_t expectedPasses = 0;
						bool expectsBatches = false;
						for (uint32_t pass = 0; pass < smokeGraph.getPassCount(); ++pass)
						{
							auto const info = smokeGraph.getPassInfo({pass});
							if (!info.enabled) continue;
							++expectedPasses;
							if (info.type == GraphPassType::Scene) expectsBatches = true;
						}
						expectsBatches = expectsBatches && openScene && !openScene->models.empty();
						if (!snapshot)
							throw std::runtime_error("Process-flow phase-one snapshot was not published.");
						// Every enabled pass must have executed, which is a stricter
						// check than merely requiring a non-empty order.
						if (snapshot->actualPassOrder.size() != expectedPasses ||
						    snapshot->actualPassOrder.size() != pipeline->getLastGraphExecutionOrder().size())
							throw std::runtime_error("Process-flow snapshot pass count differs from the enabled graph passes.");
						for (size_t pass = 0; pass < snapshot->actualPassOrder.size(); ++pass)
							if (snapshot->actualPassOrder[pass].id != pipeline->getLastGraphExecutionOrder()[pass].id)
								throw std::runtime_error("Process-flow snapshot order differs from actual execution.");
						if (expectsBatches && snapshot->batches.empty())
							throw std::runtime_error("Process-flow batch telemetry was not published.");
						if (!expectsBatches && !snapshot->batches.empty())
							throw std::runtime_error("Process-flow reported batches for a pipeline that draws no geometry.");
						if (snapshot->outputPlans.empty())
							throw std::runtime_error("Process-flow output telemetry was not published.");
						for (auto const& batch : snapshot->batches)
							if (!batch.parentPass.isValid() || !batch.sceneObject || batch.meshName.empty() ||
							    batch.materialName.empty() || batch.programName.empty() || batch.count == 0)
								throw std::runtime_error("Process-flow batch descriptor is incomplete.");
						GraphPassHandle executingPass;
						size_t submittedBatch = 0;
						for (size_t event = 0; event < snapshot->physicalEvents.size(); ++event)
						{
							auto const& flowEvent = snapshot->physicalEvents[event];
							if (event && flowEvent.sequence <= snapshot->physicalEvents[event - 1].sequence)
								throw std::runtime_error("Process-flow event sequence is not strictly ordered.");
							for (auto const* resources : {&flowEvent.inputs, &flowEvent.outputs})
								for (auto const& resource : *resources)
									if (resource.name.empty() || resource.size.x == 0 || resource.size.y == 0 || resource.samples == 0)
										throw std::runtime_error("Process-flow physical resource descriptor is invalid.");
							if (flowEvent.enabled &&
							    (flowEvent.kind == RenderFlowEventKind::MsaaResolve ||
							     flowEvent.kind == RenderFlowEventKind::Taa ||
							     flowEvent.kind == RenderFlowEventKind::SsaaHorizontal ||
							     flowEvent.kind == RenderFlowEventKind::SsaaVertical ||
							     flowEvent.kind == RenderFlowEventKind::Fxaa ||
							     flowEvent.kind == RenderFlowEventKind::Presentation) &&
							    (flowEvent.inputs.empty() || flowEvent.outputs.empty()))
								throw std::runtime_error("Process-flow physical resource descriptor is incomplete.");
							if (flowEvent.kind == RenderFlowEventKind::MsaaResolve && flowEvent.enabled &&
							    (flowEvent.inputs.front().samples <= 1 || flowEvent.outputs.front().samples != 1))
								throw std::runtime_error("Process-flow MSAA resource samples are invalid.");
							if (flowEvent.kind == RenderFlowEventKind::PassBegin) executingPass = flowEvent.pass;
							else if (flowEvent.kind == RenderFlowEventKind::BatchSubmission)
							{
								if (submittedBatch >= snapshot->batches.size() || !executingPass.isValid() ||
								    flowEvent.sequence != snapshot->batches[submittedBatch].sequence ||
								    flowEvent.pass.id != executingPass.id ||
								    snapshot->batches[submittedBatch].parentPass.id != executingPass.id)
									throw std::runtime_error("Process-flow batch order/parent association is invalid.");
								++submittedBatch;
							}
							else if (flowEvent.kind == RenderFlowEventKind::PassEnd)
							{
								if (!executingPass.isValid() || flowEvent.pass.id != executingPass.id)
									throw std::runtime_error("Process-flow pass boundaries are invalid.");
								executingPass = {};
							}
						}
						if (executingPass.isValid() || submittedBatch != snapshot->batches.size())
							throw std::runtime_error("Process-flow event stream is incomplete.");
						for (auto const& plan : snapshot->outputPlans)
						{
							bool presentation = false, taa = false, ssaaHorizontal = false,
							     ssaaVertical = false, fxaa = false, msaa = false;
							for (auto const& event : snapshot->physicalEvents)
							{
								if (event.kind == RenderFlowEventKind::MsaaResolve &&
								    (event.outputName == plan.name || event.outputName.empty())) msaa |= event.enabled;
								if (event.outputName != plan.name) continue;
								presentation |= event.kind == RenderFlowEventKind::Presentation && event.enabled;
								taa |= event.kind == RenderFlowEventKind::Taa && event.enabled;
								ssaaHorizontal |= event.kind == RenderFlowEventKind::SsaaHorizontal && event.enabled;
								ssaaVertical |= event.kind == RenderFlowEventKind::SsaaVertical && event.enabled;
								fxaa |= event.kind == RenderFlowEventKind::Fxaa && event.enabled;
							}
							if (!presentation || taa != plan.antiAliasing.taa ||
							    ssaaHorizontal != (plan.antiAliasing.ssaa != AntiAliasingSamples::Off) ||
							    ssaaVertical != (plan.antiAliasing.ssaa != AntiAliasingSamples::Off) ||
							    fxaa != plan.antiAliasing.fxaa ||
							    msaa != (plan.antiAliasing.msaa != AntiAliasingSamples::Off))
								throw std::runtime_error("Process-flow physical output stages differ from their plan.");
						}
						ProcessFlowBuildInput smokeFlowInput;
						smokeFlowInput.graph = activePreviewDocument->graph.get();
						smokeFlowInput.snapshot = snapshot;
						smokeFlowInput.sceneGeneration = sceneRuntime.getGeneration();
						for (auto const& batch : snapshot->batches)
						{
							auto id = sceneRuntime.getModelId(batch.sceneObject);
							int modelIndex = -1;
							for (size_t model = 0; openScene && model < openScene->models.size(); ++model)
								if (openScene->models[model].id == id) { modelIndex = (int)model; break; }
							if (id.empty() || modelIndex < 0)
								throw std::runtime_error("Process-flow scene identity could not be resolved in the active generation.");
							smokeFlowInput.sceneObjects[batch.sceneObject] = {modelIndex, id};
						}
						smokeFlowInput.filters.resources =
						    (uint32_t)ProcessFlowResourceCategory::AuthoredImages |
						    (uint32_t)ProcessFlowResourceCategory::Imports |
						    (uint32_t)ProcessFlowResourceCategory::NamedOutputs |
						    (uint32_t)ProcessFlowResourceCategory::MsaaResources |
						    (uint32_t)ProcessFlowResourceCategory::TaaHistories |
						    (uint32_t)ProcessFlowResourceCategory::SsaaTargets |
						    (uint32_t)ProcessFlowResourceCategory::FxaaTargets;
						auto smokeFlowModel = processFlowBuilder.build(smokeFlowInput);
						processFlowLayout.apply(smokeFlowModel);
						// A pass-less graph still produces import/output/presentation
						// nodes, but nothing has to connect them, so edges are only
						// required once the graph actually declares work.
						if (!smokeFlowModel.diagnostics.empty() || smokeFlowModel.nodes.empty() ||
						    (expectedPasses > 0 && smokeFlowModel.edges.empty()) ||
						    std::accumulate(smokeFlowModel.nodes.begin(), smokeFlowModel.nodes.end(), size_t{0}, [](size_t count, auto const& node)
						                    { return count + ((node.kind == ProcessFlowNodeKind::BatchSubmission ||
						                                       node.kind == ProcessFlowNodeKind::BatchGroup) ? node.submissionCount : 0); }) != snapshot->batches.size() ||
						    std::none_of(smokeFlowModel.nodes.begin(), smokeFlowModel.nodes.end(), [](auto const& node)
						                 { return node.kind == ProcessFlowNodeKind::Presentation; }) ||
						    std::any_of(smokeFlowModel.nodes.begin(), smokeFlowModel.nodes.end(), [](auto const& node)
						                { return (node.kind == ProcessFlowNodeKind::BatchSubmission || node.kind == ProcessFlowNodeKind::BatchGroup) &&
						                         node.sceneObjectNames.empty(); }) ||
						    std::any_of(smokeFlowModel.nodes.begin(), smokeFlowModel.nodes.end(), [](auto const& node)
						                { return node.mainSpine && ((node.enabled && node.renderDocLabels.empty()) ||
						                         node.renderDocLabels.size() != node.renderDocLabelSummaries.size()); }))
							throw std::runtime_error("Process-flow editor model/layout smoke validation failed.");
						running = false;
					}
				}
				else
					smokeStableFrames = 0;
			}
		}
		if (activePreviewTexture)
			provider->unregisterTexture(activePreviewTexture);
		if (diagnosticTexture)
			provider->unregisterTexture(diagnosticTexture);
		inspectedTarget.reset();
		diagnosticResolveTarget.reset();
		diagnosticTarget.reset();
		activePreviewDocument.reset();
		activePreviewTarget.reset();
		if (!activeGraphResource.empty())
		{
			renderSystem.removeRenderPipeline(activePipeline);
			resources.deleteResource(activeGraphResource);
		}
		scene->unload();
		scene.reset();
		sceneRuntime.clear();
		pipelineRuntime.clear();
		imGuiShutdown(&backend);
		provider->clearRegisteredTextures();
		font.reset();
		if (resources.getResource("__ImGui_Font__", true))
			resources.deleteResource("__ImGui_Font__");
		renderSystem.destroyCoreResources();
		return 0;
	}
	catch (std::exception const& error)
	{
		fprintf(stderr, "PipelineEditor fatal error: %s\n", error.what());
		MessageBoxA(nullptr, error.what(), "PipelineEditor Error", MB_ICONERROR);
		return 1;
	}
}
