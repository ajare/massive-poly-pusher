#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iterator>
#include <stdexcept>
#include <fstream>
#include <vector>

#include "mpp/ProgrammaticBasicMaterialStream.h"
#include "mpp/ProgrammaticPbrMaterialStream.h"
#include "mpp/ResourceStreamSerializer.h"
#include "mpp/resource-parsers/FileBasicMaterialStream.h"
#include "mpp/resource-parsers/FileMaterialStream.h"
#include "mpp/resource-parsers/FilePbrMaterialStream.h"
#include "mpp/resource-parsers/MaterialResourceTests.h"

namespace mpp::resource_parsers
{
	namespace
	{
		std::vector<char> readBytes(std::filesystem::path const& path)
		{
			std::ifstream input(path, std::ios::binary);
			return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
		}

		void makeLegacy(std::filesystem::path const& path)
		{
			auto bytes = readBytes(path);
			char const typed[] = "BasicMaterial";
			auto found = std::search(bytes.begin(), bytes.end(), typed, typed + sizeof(typed) - 1);
			if (found == bytes.end()) throw std::runtime_error("BasicMaterial tag not found in test stream");
			auto const offset = static_cast<size_t>(found - bytes.begin());
			uint32_t legacyLength = 8;
			std::memcpy(bytes.data() + offset - sizeof(uint32_t), &legacyLength, sizeof(legacyLength));
			std::memcpy(bytes.data() + offset, "Material", legacyLength);
			bytes.erase(bytes.begin() + offset + legacyLength, bytes.begin() + offset + sizeof(typed) - 1);
			std::memcpy(bytes.data(), "RSER", 4);
			std::ofstream output(path, std::ios::binary | std::ios::trunc);
			output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
		}
	}

	bool runMaterialResourceTests(ResourceManager* resourceMgr, std::string* failure)
	{
		auto fail = [&](std::string const& message) { if (failure) *failure = message; return false; };
		auto const root = std::filesystem::temp_directory_path() / "mpp_material_resource_tests";
		auto const basicXml = root.string() + "_basic.xml";
		auto const pbrXml = root.string() + "_pbr.xml";
		auto const invalidPbrXml = root.string() + "_invalid_pbr.xml";
		auto const basicBin = root.string() + "_basic.bin";
		auto const pbrBin = root.string() + "_pbr.bin";
		auto const legacyBasicBin = root.string() + "_legacy_basic.bin";
		auto const legacyPbrBin = root.string() + "_legacy_pbr.bin";
		try
		{
			{ std::ofstream file(basicXml); file << "<BasicMaterial><name>Test.Basic</name></BasicMaterial>"; }
			{ std::ofstream file(pbrXml); file << "<PbrMaterial><name>Test.Pbr</name><Surface><metallicFactor>0.25</metallicFactor><roughnessFactor>0.75</roughnessFactor></Surface></PbrMaterial>"; }
			{ std::ofstream file(invalidPbrXml); file << "<PbrMaterial><name>Test.InvalidPbr</name><Surface><roughnessFactor>1.5</roughnessFactor></Surface></PbrMaterial>"; }
			auto basicFile = FileMaterialStream::fromFile(resourceMgr, basicXml);
			auto pbrFile = FileMaterialStream::fromFile(resourceMgr, pbrXml);
			if (basicFile->getType() != "BasicMaterial" || pbrFile->getType() != "PbrMaterial") return fail("material XML root dispatch selected the wrong stream type");
			bool rejected = false;
			try { FileBasicMaterialStream wrong(resourceMgr, pbrXml); wrong.load(0); } catch (...) { rejected = true; }
			if (!rejected) return fail("BasicMaterial parser accepted a PbrMaterial root");
			rejected = false;
			try { FilePbrMaterialStream wrong(resourceMgr, basicXml); wrong.load(0); } catch (...) { rejected = true; }
			if (!rejected) return fail("PbrMaterial parser accepted a BasicMaterial root");
			rejected = false;
			try { FilePbrMaterialStream invalid(resourceMgr, invalidPbrXml); invalid.load(0); } catch (...) { rejected = true; }
			if (!rejected) return fail("PbrMaterial parser accepted an out-of-range surface factor");

			ResourceStreamSerializer serializer(resourceMgr);
			auto basic = std::make_shared<ProgrammaticBasicMaterialStream>(resourceMgr);
			basic->setUniform("CUSTOM_VALUE", 4.0f);
			serializer.serialize(basic, basicBin);
			auto restoredBasic = serializer.deserialize(basicBin);
			if (restoredBasic->getType() != "BasicMaterial") return fail("BasicMaterial binary round trip changed its concrete type");
			restoredBasic->load(0);
			auto restoredBasicStream = dynamic_cast<BasicMaterialStream*>(restoredBasic.get());
			if (!restoredBasicStream || !restoredBasicStream->getUniforms().getUniformData().contains("CUSTOM_VALUE")) return fail("BasicMaterial generic uniform was lost during binary round trip");

			auto pbr = std::make_shared<ProgrammaticPbrMaterialStream>(resourceMgr);
			PbrMaterialSpecification::PbrSurface low;
			low.metallicFactor = 0.2f; low.roughnessFactor = 0.8f;
			pbr->setSurface(low);
			pbr->setExtensionUniform("PBR_EXT_TEST", glm::vec2(2.0f, 3.0f));
			auto highQuality = pbr->createQualitySetting("High");
			auto high = low; high.metallicFactor = 0.9f; high.roughnessFactor = 0.1f;
			pbr->setSurface(high, highQuality);
			serializer.serialize(pbr, pbrBin);
			auto bytes = readBytes(pbrBin);
			if (bytes.size() < 4 || std::memcmp(bytes.data(), "RSE2", 4) != 0) return fail("new resource stream is not versioned RSE2");
			if (std::search(bytes.begin(), bytes.end(), "PbrMaterial", "PbrMaterial" + 11) == bytes.end()) return fail("PbrMaterial binary tag is missing");
			if (std::search(bytes.begin(), bytes.end(), "\x08\x00\x00\x00Material", "\x08\x00\x00\x00Material" + 12) != bytes.end()) return fail("new serializer emitted a legacy Material tag");
			auto restoredPbrBase = serializer.deserialize(pbrBin);
			auto restoredPbr = dynamic_cast<PbrMaterialStream*>(restoredPbrBase.get());
			if (!restoredPbr || restoredPbrBase->getType() != "PbrMaterial") return fail("PbrMaterial binary round trip changed its concrete type");
			if (restoredPbr->usesLegacyFullContract()) return fail("new PbrMaterial binary round trip acquired legacy full-contract state");
			restoredPbr->load(restoredPbr->getQualityNames().at("High"));
			if (restoredPbr->getPbrSurface().metallicFactor != 0.9f || restoredPbr->getPbrSurface().roughnessFactor != 0.1f) return fail("PBR quality surface did not survive binary round trip");

			std::filesystem::copy_file(basicBin, legacyBasicBin, std::filesystem::copy_options::overwrite_existing);
			makeLegacy(legacyBasicBin);
			if (serializer.deserialize(legacyBasicBin)->getType() != "BasicMaterial") return fail("legacy basic Material did not convert to BasicMaterial");
			auto legacyPbr = std::make_shared<ProgrammaticBasicMaterialStream>(resourceMgr);
			legacyPbr->setUniform("PBR_ENABLED", int32_t(1));
			legacyPbr->setUniform("PBR_METALLIC_FACTOR", 0.65f);
			legacyPbr->setUniform("PBR_ROUGHNESS_FACTOR", 0.35f);
			serializer.serialize(legacyPbr, legacyPbrBin);
			makeLegacy(legacyPbrBin);
			auto converted = serializer.deserialize(legacyPbrBin);
			auto convertedPbr = dynamic_cast<PbrMaterialStream*>(converted.get());
			if (!convertedPbr || convertedPbr->getPbrSurface().metallicFactor != 0.65f || convertedPbr->getPbrSurface().roughnessFactor != 0.35f) return fail("legacy PBR Material surface conversion failed");
			if (!convertedPbr->usesLegacyFullContract()) return fail("legacy PBR Material did not retain its temporary full-contract marker");
		}
		catch (std::exception const& exception) { return fail(exception.what()); }
		std::filesystem::remove(basicXml); std::filesystem::remove(pbrXml); std::filesystem::remove(invalidPbrXml);
		std::filesystem::remove(basicBin); std::filesystem::remove(pbrBin);
		std::filesystem::remove(legacyBasicBin); std::filesystem::remove(legacyPbrBin);
		return true;
	}
}
