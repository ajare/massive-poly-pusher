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
		throw std::runtime_error("glTF PBR material conversion is not implemented yet (phase 2 envelope validated): " + filepath.string());
	}
}
