#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <Windows.h>
#endif

#include "mpp/BuiltInShaderResources.h"
#include "mpp/ParticleShaders.h"
#include "mpp/PbrShaders.h"

using namespace std;

namespace mpp
{
	namespace
	{
		constexpr size_t PbrShaderCount = 2;
		constexpr size_t ParticleShaderCount = 24;

		struct ShaderResources
		{
			array<string, PbrShaderCount> pbr;
			array<string, ParticleShaderCount> particle;
		};

		mutex shaderResourcesMutex;
		unique_ptr<ShaderResources> shaderResources;

		filesystem::path executableDirectory()
		{
#if defined(_WIN32)
			vector<wchar_t> path(32768);
			auto length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
			if (length != 0 && length < path.size())
				return filesystem::path(wstring(path.data(), length)).parent_path();
#elif defined(__linux__)
			error_code error;
			auto path = filesystem::read_symlink("/proc/self/exe", error);
			if (!error && !path.empty()) return path.parent_path();
#endif
			return filesystem::current_path();
		}

		optional<filesystem::path> configuredShaderResourceDirectory()
		{
#if defined(_WIN32)
			auto length = GetEnvironmentVariableW(L"MPP_SHADER_RESOURCE_DIR", nullptr, 0);
			if (length == 0) return nullopt;
			vector<wchar_t> value(length);
			auto copied = GetEnvironmentVariableW(
				L"MPP_SHADER_RESOURCE_DIR", value.data(), static_cast<DWORD>(value.size()));
			if (copied == 0 || copied >= value.size()) return nullopt;
			return filesystem::path(wstring(value.data(), copied));
#else
			if (auto value = getenv("MPP_SHADER_RESOURCE_DIR"); value && *value)
				return filesystem::path(value);
			return nullopt;
#endif
		}

		filesystem::path findShaderResourceDirectory()
		{
			vector<filesystem::path> candidates;
			if (auto configured = configuredShaderResourceDirectory())
				candidates.push_back(*configured);
			candidates.push_back(executableDirectory() / "mpp-shaders");
#ifdef MPP_BUILT_IN_SHADER_SOURCE_DIR
			candidates.emplace_back(MPP_BUILT_IN_SHADER_SOURCE_DIR);
#endif
			for (auto const& candidate : candidates)
			{
				error_code error;
				if (filesystem::is_directory(candidate, error)) return candidate;
			}

			string message = "Could not find the Massive Poly Pusher built-in shader resources. Searched:";
			for (auto const& candidate : candidates) message += "\n  " + candidate.string();
			throw runtime_error(message);
		}

		string loadShader(filesystem::path const& directory, char const* filename)
		{
			auto path = directory / filename;
			ifstream stream(path, ios::binary);
			if (!stream) throw runtime_error("Could not open built-in shader resource '" + path.string() + "'.");
			string source((istreambuf_iterator<char>(stream)), istreambuf_iterator<char>());
			if (!stream.good() && !stream.eof())
				throw runtime_error("Could not read built-in shader resource '" + path.string() + "'.");
			if (source.empty()) throw runtime_error("Built-in shader resource '" + path.string() + "' is empty.");
			return source;
		}
	}

	void initialiseBuiltInShaderResources()
	{
		lock_guard lock(shaderResourcesMutex);
		if (shaderResources) return;

		auto directory = findShaderResourceDirectory();
		auto loaded = make_unique<ShaderResources>();
		loaded->pbr = {
			loadShader(directory, "BuiltInPbr.vert"),
			loadShader(directory, "BuiltInPbr.frag")
		};
		loaded->particle = {
			loadShader(directory, "ParticlePoolInitialise.comp"),
			loadShader(directory, "ParticleStatisticsPrepare.comp"),
			loadShader(directory, "ParticleEventPrepare.comp"),
			loadShader(directory, "ParticleSpawn.comp"),
			loadShader(directory, "ParticleSimulationPrepare.comp"),
			loadShader(directory, "ParticleSimulation.comp"),
			loadShader(directory, "ParticleEventProcess.comp"),
			loadShader(directory, "ParticleCompactionPrepare.comp"),
			loadShader(directory, "ParticleCompactionCount.comp"),
			loadShader(directory, "ParticleCompactionPrefix.comp"),
			loadShader(directory, "ParticleCompactionScatter.comp"),
			loadShader(directory, "ParticleMeshCommand.comp"),
			loadShader(directory, "ParticleSortPrepare.comp"),
			loadShader(directory, "ParticleSortKey.comp"),
			loadShader(directory, "ParticleRadixHistogram.comp"),
			loadShader(directory, "ParticleRadixPrefix.comp"),
			loadShader(directory, "ParticleRadixScatter.comp"),
			loadShader(directory, "ParticleSortFinalize.comp"),
			loadShader(directory, "ParticleDraw.vert"),
			loadShader(directory, "ParticleDraw.frag"),
			loadShader(directory, "ParticleVolumetricLighting.vert"),
			loadShader(directory, "ParticleVolumetricLighting.frag"),
			loadShader(directory, "ParticleDistortionComposite.frag"),
			loadShader(directory, "ParticleWeightedOitResolve.frag")
		};
		shaderResources = std::move(loaded);
	}

	string const& pbrShaderSource(PbrShader shader)
	{
		initialiseBuiltInShaderResources();
		auto index = static_cast<size_t>(shader);
		if (index >= PbrShaderCount) throw invalid_argument("Unknown built-in PBR shader resource.");
		return shaderResources->pbr[index];
	}

	string const& particleShaderSource(ParticleShader shader)
	{
		initialiseBuiltInShaderResources();
		auto index = static_cast<size_t>(shader);
		if (index >= ParticleShaderCount) throw invalid_argument("Unknown built-in particle shader resource.");
		return shaderResources->particle[index];
	}
}
