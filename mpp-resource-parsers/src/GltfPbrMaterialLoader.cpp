#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "mpp/resource-parsers/GltfPbrMaterialLoader.h"

namespace mpp::resource_parsers
{
	namespace
	{
		struct Json
		{
			enum class Type { Null, Boolean, Number, String, Array, Object } type{Type::Null};
			bool boolean{}; double number{}; std::string string; std::vector<Json> array; std::map<std::string, Json> object;
			Json const* get(std::string const& key) const { auto found = object.find(key); return found == object.end() ? nullptr : &found->second; }
		};
		class Parser
		{
			char const* current; char const* end;
			void space() { while (current != end && std::isspace((unsigned char)*current)) ++current; }
			void expect(char character) { space(); if (current == end || *current++ != character) throw std::runtime_error("Invalid glTF JSON."); }
			std::string string()
			{
				expect('\"'); std::string result;
				while (current != end && *current != '\"') { if (*current == '\\') { ++current; if (current == end) throw std::runtime_error("Invalid glTF JSON escape."); char value = *current++; result += value == 'n' ? '\n' : value == 'r' ? '\r' : value == 't' ? '\t' : value; } else result += *current++; }
				expect('\"'); return result;
			}
			Json value()
			{
				space(); if (current == end) throw std::runtime_error("Unexpected end of glTF JSON.");
				if (*current == '{') { ++current; Json result; result.type = Json::Type::Object; space(); while (current != end && *current != '}') { auto key = string(); expect(':'); result.object.emplace(std::move(key), value()); space(); if (*current != '}') expect(','); space(); } expect('}'); return result; }
				if (*current == '[') { ++current; Json result; result.type = Json::Type::Array; space(); while (current != end && *current != ']') { result.array.push_back(value()); space(); if (*current != ']') expect(','); space(); } expect(']'); return result; }
				if (*current == '\"') { Json result; result.type = Json::Type::String; result.string = string(); return result; }
				if (end - current >= 4 && std::string_view(current, 4) == "true") { current += 4; Json result; result.type = Json::Type::Boolean; result.boolean = true; return result; }
				if (end - current >= 5 && std::string_view(current, 5) == "false") { current += 5; Json result; result.type = Json::Type::Boolean; return result; }
				if (end - current >= 4 && std::string_view(current, 4) == "null") { current += 4; return {}; }
				char* next{}; Json result; result.type = Json::Type::Number; result.number = std::strtod(current, &next); if (next == current) throw std::runtime_error("Invalid glTF JSON value."); current = next; return result;
			}
		public:
			explicit Parser(std::string const& source) : current(source.data()), end(source.data() + source.size()) {}
			Json parse() { auto result = value(); space(); if (current != end) throw std::runtime_error("Trailing glTF JSON data."); return result; }
		};
	}

	GltfPbrMaterialLoadResult GltfPbrMaterialLoader::loadFirstMaterial(std::filesystem::path const& filepath)
	{
		if (filepath.empty()) throw std::invalid_argument("glTF material loader requires a file path.");
		auto extension = filepath.extension().string(); for (auto& character : extension) character = (char)std::tolower((unsigned char)character);
		if (extension == ".glb") throw std::runtime_error("Binary .glb material loading is not supported yet; use JSON .gltf.");
		if (extension != ".gltf") throw std::runtime_error("glTF material loader requires a .gltf file: " + filepath.string());
		std::ifstream file(filepath, std::ios::binary); if (!file) throw std::runtime_error("Could not open glTF file: " + filepath.string());
		std::string source((std::istreambuf_iterator<char>(file)), {}); auto root = Parser(source).parse();
		if (root.type != Json::Type::Object) throw std::runtime_error("glTF root must be a JSON object.");
		auto asset = root.get("asset"); auto version = asset && asset->type == Json::Type::Object ? asset->get("version") : nullptr;
		if (!version || version->type != Json::Type::String || version->string.empty() || version->string.front() != '2') throw std::runtime_error("glTF asset.version must identify glTF 2.x.");
		auto materials = root.get("materials");
		if (!materials || materials->type != Json::Type::Array || materials->array.empty()) throw std::runtime_error("glTF file contains no materials.");
		auto const& material = materials->array.front();
		if (material.type != Json::Type::Object) throw std::runtime_error("glTF material 0 must be an object.");
		GltfPbrMaterialLoadResult result;
		result.materialIndex = 0;
		auto name = material.get("name"); result.materialName = name && name->type == Json::Type::String && !name->string.empty() ? name->string : filepath.stem().string();
		if (materials->array.size() > 1) result.warnings.push_back("glTF defines " + std::to_string(materials->array.size()) + " materials; MPP loaded material 0 ('" + result.materialName + "') and ignored the remaining materials.");
		result.definition = utils::StructuredData("PbrMaterial"); result.definition.addEntry("name", result.materialName);
		utils::StructuredData surface("Surface");
		auto scalar = [](Json const* value, float fallback) { return value && value->type == Json::Type::Number ? (float)value->number : fallback; };
		auto vector = [&](Json const* value, std::initializer_list<float> defaults) { std::vector<float> result(defaults); if (value && value->type == Json::Type::Array) for (size_t i = 0; i < result.size() && i < value->array.size(); ++i) result[i] = scalar(&value->array[i], result[i]); return result; };
		auto pbr = material.get("pbrMetallicRoughness");
		auto base = pbr && pbr->type == Json::Type::Object ? vector(pbr->get("baseColorFactor"), {1, 1, 1, 1}) : std::vector<float>{1, 1, 1, 1};
		surface.addEntry("baseColourFactor", std::to_string(base[0])+" "+std::to_string(base[1])+" "+std::to_string(base[2])+" "+std::to_string(base[3]));
		surface.addEntry("metallicFactor", std::to_string(pbr && pbr->type == Json::Type::Object ? scalar(pbr->get("metallicFactor"), 1) : 1));
		surface.addEntry("roughnessFactor", std::to_string(pbr && pbr->type == Json::Type::Object ? scalar(pbr->get("roughnessFactor"), 1) : 1));
		auto emissive = vector(material.get("emissiveFactor"), {0, 0, 0}); surface.addEntry("emissiveFactor", std::to_string(emissive[0])+" "+std::to_string(emissive[1])+" "+std::to_string(emissive[2]));
		surface.addEntry("normalScale", "1"); surface.addEntry("occlusionStrength", "1");
		auto alpha = material.get("alphaMode"); surface.addEntry("alphaMode", alpha && alpha->type == Json::Type::String ? alpha->string : "OPAQUE");
		surface.addEntry("alphaCutoff", std::to_string(scalar(material.get("alphaCutoff"), 0.5f)));
		auto sided = material.get("doubleSided"); surface.addEntry("doubleSided", sided && sided->type == Json::Type::Boolean && sided->boolean ? "true" : "false");
		result.definition.addEntry("Surface", surface);
		auto textures = root.get("textures"), images = root.get("images");
		auto texturePath = [&](Json const* texture) -> std::filesystem::path
		{
			if (!texture || texture->type != Json::Type::Object || !textures || !images || textures->type != Json::Type::Array || images->type != Json::Type::Array) return {};
			auto index = texture->get("index"); if (!index || index->type != Json::Type::Number || index->number < 0 || (size_t)index->number >= textures->array.size()) return {};
			auto sourceIndex = textures->array[(size_t)index->number].get("source"); if (!sourceIndex || sourceIndex->type != Json::Type::Number || sourceIndex->number < 0 || (size_t)sourceIndex->number >= images->array.size()) return {};
			auto uri = images->array[(size_t)sourceIndex->number].get("uri"); if (!uri || uri->type != Json::Type::String || uri->string.starts_with("data:")) return {};
			return (filepath.parent_path() / std::filesystem::path(uri->string)).lexically_normal();
		};
		auto addMap = [&](char const* name, Json const* texture, char const* colourSpace)
		{
			auto imagePath = texturePath(texture); if (imagePath.empty()) { if (texture) result.warnings.push_back(std::string("glTF ") + name + " image is embedded or unresolved and was not imported yet."); return; }
			utils::StructuredData image("Texture"); image.addEntry("name", result.materialName + "." + name); image.addEntry("target", "2D"); image.addEntry("filename", imagePath.string()); image.addEntry("colourSpace", colourSpace); image.addEntry("minFilter", "LINEAR"); image.addEntry("magFilter", "LINEAR"); image.addEntry("wrap", "REPEAT");
			utils::StructuredData map(name); map.addEntry("Resource", image); result.definition.addEntry(name, map); result.generatedImages.push_back(imagePath);
		};
		if (pbr && pbr->type == Json::Type::Object) { addMap("BaseColourMap", pbr->get("baseColorTexture"), "SRGB"); addMap("MetallicRoughnessMap", pbr->get("metallicRoughnessTexture"), "LINEAR"); }
		addMap("NormalMap", material.get("normalTexture"), "LINEAR"); if (auto normal = material.get("normalTexture")) result.definition.getEntry("Surface").setEntryValue("normalScale", std::to_string(scalar(normal->get("scale"), 1)));
		addMap("OcclusionMap", material.get("occlusionTexture"), "LINEAR"); if (auto occlusion = material.get("occlusionTexture")) result.definition.getEntry("Surface").setEntryValue("occlusionStrength", std::to_string(scalar(occlusion->get("strength"), 1)));
		addMap("EmissiveMap", material.get("emissiveTexture"), "SRGB");
		return result;
	}
}
