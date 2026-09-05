#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cctype>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include "mpp/data/StructuredData.h"
#include "mpp/resource-parsers/ParticleEffectParser.h"
#include "StructuredDataAdapter.h"

namespace mpp::resource_parsers
{
	namespace
	{
		using Data = mpp::data::StructuredData;

		struct Reader
		{
			ParticleEffectParseResult result;
			std::string source;

			void error(std::string code, std::string message, std::string path = {})
			{
				result.diagnostics.error(std::move(code), std::move(message), { source, std::move(path) });
			}

			void fields(Data const& node, std::initializer_list<char const*> accepted, std::string const& path)
			{
				std::unordered_set<std::string> names;
				for (auto name : accepted) names.emplace(name);
				for (auto const& entry : node)
					if (!names.contains(entry.first)) error("MPP-PARTICLE-002", "Unknown particle effect field '" + entry.first + "'.", path + "/" + entry.first);
			}

			std::string value(Data const& node, char const* name, std::string const& path, bool required = false)
			{
				if (!node.hasEntry(name))
				{
					if (required) error("MPP-PARTICLE-003", "Required particle effect field '" + std::string(name) + "' is missing.", path);
					return {};
				}
				auto const& child = node.getEntry(name);
				if (!child.isValue()) { error("MPP-PARTICLE-004", "Particle effect field '" + std::string(name) + "' must be a scalar.", path + "/" + name); return {}; }
				return child.getValue();
			}

			float number(Data const& node, char const* name, std::string const& path, float fallback, bool required = false)
			{
				auto text = value(node, name, path, required);
				if (text.empty()) return fallback;
				try
				{
					size_t used = 0; float parsed = std::stof(text, &used);
					if (used != text.size() || !std::isfinite(parsed)) throw std::invalid_argument("number");
					return parsed;
				}
				catch (...) { error("MPP-PARTICLE-005", "Particle effect field '" + std::string(name) + "' must be a finite number.", path + "/" + name); return fallback; }
			}

			uint32_t integer(Data const& node, char const* name, std::string const& path, uint32_t fallback, bool required = false)
			{
				auto text = value(node, name, path, required);
				if (text.empty()) return fallback;
				uint32_t parsed{}; auto conversion = std::from_chars(text.data(), text.data() + text.size(), parsed);
				if (conversion.ec != std::errc{} || conversion.ptr != text.data() + text.size())
				{
					error("MPP-PARTICLE-006", "Particle effect field '" + std::string(name) + "' must be an unsigned integer.", path + "/" + name);
					return fallback;
				}
				return parsed;
			}

			bool boolean(Data const& node, char const* name, std::string const& path, bool fallback)
			{
				auto text = value(node, name, path);
				std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
				if (text.empty()) return fallback;
				if (text == "true" || text == "1") return true;
				if (text == "false" || text == "0") return false;
				error("MPP-PARTICLE-007", "Particle effect field '" + std::string(name) + "' must be true or false.", path + "/" + name);
				return fallback;
			}

			template<size_t N>
			std::array<float, N> vector(Data const& node, char const* name, std::string const& path, std::array<float, N> fallback, bool required = false)
			{
				auto text = value(node, name, path, required);
				if (text.empty()) return fallback;
				std::replace(text.begin(), text.end(), ',', ' '); std::istringstream input(text); std::array<float, N> parsed{};
				for (auto& item : parsed) if (!(input >> item) || !std::isfinite(item)) { error("MPP-PARTICLE-008", "Particle effect field '" + std::string(name) + "' requires " + std::to_string(N) + " finite values.", path + "/" + name); return fallback; }
				std::string extra; if (input >> extra) { error("MPP-PARTICLE-008", "Particle effect field '" + std::string(name) + "' requires " + std::to_string(N) + " finite values.", path + "/" + name); return fallback; }
				return parsed;
			}

			template<typename T>
			T enumeration(Data const& node, char const* name, std::string const& path, T fallback, std::initializer_list<std::pair<char const*, T>> choices)
			{
				auto text = value(node, name, path);
				std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
				if (text.empty()) return fallback;
				for (auto const& choice : choices) if (text == choice.first) return choice.second;
				error("MPP-PARTICLE-009", "Unknown value '" + text + "' for particle effect field '" + name + "'.", path + "/" + name);
				return fallback;
			}

			void parseCurve(Data const& node, ParticleCurve& curve, std::string const& path)
			{
				fields(node, { "default", "Keys" }, path);
				curve.defaultValue = number(node, "default", path, curve.defaultValue);
				if (!node.hasEntry("Keys")) return;
				float previous = -1.0f;
				for (auto const& entry : node.getEntry("Keys"))
				{
					if (entry.first != "Key") { error("MPP-PARTICLE-002", "Curves contain only Key entries.", path + "/Keys"); continue; }
					fields(entry.second, { "time", "value" }, path + "/Keys/Key");
					ParticleCurveKey key{ number(entry.second, "time", path + "/Keys/Key", 0.0f, true), number(entry.second, "value", path + "/Keys/Key", 1.0f, true) };
					if (key.time < 0.0f || key.time > 1.0f || key.time < previous) error("MPP-PARTICLE-010", "Curve key times must be ordered in [0, 1].", path + "/Keys/Key/time");
					previous = key.time; curve.keys.push_back(key);
				}
			}

			void parseGradient(Data const& node, ParticleGradient& gradient, std::string const& path)
			{
				fields(node, { "default", "Keys" }, path);
				gradient.defaultColour = vector<3>(node, "default", path, gradient.defaultColour);
				if (!node.hasEntry("Keys")) return;
				float previous = -1.0f;
				for (auto const& entry : node.getEntry("Keys"))
				{
					if (entry.first != "Key") { error("MPP-PARTICLE-002", "A colour gradient contains only Key entries.", path + "/Keys"); continue; }
					fields(entry.second, { "time", "colour" }, path + "/Keys/Key");
					ParticleGradientKey key{ number(entry.second, "time", path + "/Keys/Key", 0.0f, true), vector<3>(entry.second, "colour", path + "/Keys/Key", { 1, 1, 1 }, true) };
					if (key.time < 0.0f || key.time > 1.0f || key.time < previous) error("MPP-PARTICLE-010", "Gradient key times must be ordered in [0, 1].", path + "/Keys/Key/time");
					previous = key.time; gradient.keys.push_back(key);
				}
			}

			void parseEmitter(Data const& node, size_t index)
			{
				auto path = "/ParticleEffect/Emitters/Emitter[" + std::to_string(index) + "]";
				fields(node, { "name", "maximumParticleCount", "transform", "Spawn", "Behaviours", "Curves", "Appearance" }, path);
				ParticleEffectSpecification::EmitterTemplate authored;
				authored.name = value(node, "name", path, true);
				auto& emitter = authored.value; auto& sim = emitter.simulation; auto& appearance = emitter.appearance;
				sim.shapeSeedModulesBudget[3] = integer(node, "maximumParticleCount", path, 0, true);
				auto transform = vector<16>(node, "transform", path, sim.transform);
				sim.transform = transform;
				for (int column = 0; column < 4; ++column) for (int row = 0; row < 4; ++row) emitter.localTransform[column][row] = transform[size_t(column * 4 + row)];

				if (!node.hasEntry("Spawn")) error("MPP-PARTICLE-003", "Emitter template requires a Spawn block.", path);
				else
				{
					auto const& spawn = node.getEntry("Spawn"); auto spawnPath = path + "/Spawn";
					fields(spawn, { "shape", "shapeParameters", "seed", "mode", "enabled", "rate", "burstCount", "initialVelocityMin", "initialVelocityMax", "colourMin", "colourMax", "lifetime", "size", "rotation", "angularVelocity" }, spawnPath);
					sim.shapeSeedModulesBudget[0] = uint32_t(enumeration(spawn, "shape", spawnPath, ParticleSpawnShape::Point, { {"point",ParticleSpawnShape::Point},{"line",ParticleSpawnShape::Line},{"box",ParticleSpawnShape::Box},{"sphere",ParticleSpawnShape::Sphere},{"hemisphere",ParticleSpawnShape::Hemisphere},{"disc",ParticleSpawnShape::Disc},{"cone",ParticleSpawnShape::Cone} }));
					sim.shapeParameters = vector<4>(spawn, "shapeParameters", spawnPath, sim.shapeParameters);
					sim.shapeSeedModulesBudget[1] = integer(spawn, "seed", spawnPath, 0);
					sim.emissionState[0] = uint32_t(enumeration(spawn, "mode", spawnPath, 0u, { {"continuous",0u},{"burst",1u} }));
					sim.emissionState[1] = boolean(spawn, "enabled", spawnPath, true) ? 1u : 0u;
					sim.emissionRateAndPadding[0] = number(spawn, "rate", spawnPath, 0.0f);
					sim.emissionState[2] = integer(spawn, "burstCount", spawnPath, 0);
					sim.initialVelocityMin = vector<4>(spawn, "initialVelocityMin", spawnPath, sim.initialVelocityMin);
					sim.initialVelocityMax = vector<4>(spawn, "initialVelocityMax", spawnPath, sim.initialVelocityMax);
					sim.colourMin = vector<4>(spawn, "colourMin", spawnPath, sim.colourMin);
					sim.colourMax = vector<4>(spawn, "colourMax", spawnPath, sim.colourMax);
					auto lifetime = vector<2>(spawn, "lifetime", spawnPath, { 1, 1 }); auto size = vector<2>(spawn, "size", spawnPath, { 1, 1 });
					auto rotation = vector<2>(spawn, "rotation", spawnPath, { 0, 0 }); auto angular = vector<2>(spawn, "angularVelocity", spawnPath, { 0, 0 });
					sim.lifetimeSizeRanges = { lifetime[0], lifetime[1], size[0], size[1] }; sim.rotationRanges = { rotation[0], rotation[1], angular[0], angular[1] };
					if (sim.emissionRateAndPadding[0] < 0 || lifetime[0] < 0 || lifetime[1] < lifetime[0] || size[0] < 0 || size[1] < size[0]) error("MPP-PARTICLE-011", "Spawn rates and ranges must be non-negative and ordered.", spawnPath);
				}

				if (node.hasEntry("Behaviours"))
				{
					auto const& behaviours = node.getEntry("Behaviours"); auto behaviourPath = path + "/Behaviours";
					fields(behaviours, { "Gravity", "Drag", "Noise" }, behaviourPath);
					if (behaviours.hasEntry("Gravity")) { auto const& block=behaviours.getEntry("Gravity"); fields(block,{"acceleration"},behaviourPath+"/Gravity"); auto acceleration=vector<3>(block,"acceleration",behaviourPath+"/Gravity",{0,-9.81f,0}); sim.gravityAndDrag[0]=acceleration[0];sim.gravityAndDrag[1]=acceleration[1];sim.gravityAndDrag[2]=acceleration[2];sim.shapeSeedModulesBudget[2]|=uint32_t(ParticleBehaviourModule::Gravity); }
					if (behaviours.hasEntry("Drag")) { auto const& block=behaviours.getEntry("Drag"); fields(block,{"coefficient"},behaviourPath+"/Drag"); sim.gravityAndDrag[3]=number(block,"coefficient",behaviourPath+"/Drag",0,true);sim.shapeSeedModulesBudget[2]|=uint32_t(ParticleBehaviourModule::Drag); }
					if (behaviours.hasEntry("Noise")) { auto const& block=behaviours.getEntry("Noise");fields(block,{"frequency","strength","scroll","timeScale"},behaviourPath+"/Noise");auto frequency=vector<3>(block,"frequency",behaviourPath+"/Noise",{1,1,1});auto scroll=vector<3>(block,"scroll",behaviourPath+"/Noise",{0,0,0});sim.noiseFrequencyStrength={frequency[0],frequency[1],frequency[2],number(block,"strength",behaviourPath+"/Noise",0,true)};sim.noiseScrollAndTimeScale={scroll[0],scroll[1],scroll[2],number(block,"timeScale",behaviourPath+"/Noise",1)};sim.shapeSeedModulesBudget[2]|=uint32_t(ParticleBehaviourModule::Noise); }
				}

				if (node.hasEntry("Curves"))
				{
					auto const& curves=node.getEntry("Curves"); auto curvePath=path+"/Curves";
					fields(curves,{"Size","Alpha","VelocityMultiplier","Drag","RotationSpeed","EmissiveIntensity","Colour"},curvePath);
					std::array<std::pair<char const*,ParticleScalarCurve>,6> names{{{"Size",ParticleScalarCurve::Size},{"Alpha",ParticleScalarCurve::Alpha},{"VelocityMultiplier",ParticleScalarCurve::VelocityMultiplier},{"Drag",ParticleScalarCurve::Drag},{"RotationSpeed",ParticleScalarCurve::RotationSpeed},{"EmissiveIntensity",ParticleScalarCurve::EmissiveIntensity}}};
					for(auto const& named:names)if(curves.hasEntry(named.first))parseCurve(curves.getEntry(named.first),emitter.curves[size_t(named.second)],curvePath+"/"+named.first);
					if(curves.hasEntry("Colour"))parseGradient(curves.getEntry("Colour"),emitter.colourGradient,curvePath+"/Colour");
				}

				if (node.hasEntry("Appearance"))
				{
					auto const& block=node.getEntry("Appearance"); auto appearancePath=path+"/Appearance";
					fields(block,{"texture","tint","alpha","emissiveIntensity","softFadeDistance","atlasColumns","atlasRows","frameCount","animation","randomStart","animationRate","billboard","blendClass"},appearancePath);
					authored.albedoTexture=value(block,"texture",appearancePath); auto tint=vector<3>(block,"tint",appearancePath,{1,1,1});appearance.tintAndAlpha={tint[0],tint[1],tint[2],number(block,"alpha",appearancePath,1)};
					appearance.appearance[0]=number(block,"emissiveIntensity",appearancePath,1);appearance.appearance[1]=number(block,"softFadeDistance",appearancePath,0);appearance.appearance[2]=number(block,"animationRate",appearancePath,0);
					appearance.textureAndAtlas[2]=integer(block,"atlasColumns",appearancePath,1);appearance.textureAndAtlas[3]=integer(block,"atlasRows",appearancePath,1);appearance.modes[0]=integer(block,"frameCount",appearancePath,1);
					auto animation=enumeration(block,"animation",appearancePath,ParticleTextureAnimation::None,{{"none",ParticleTextureAnimation::None},{"frameoverlife",ParticleTextureAnimation::FrameOverLife},{"fixedrate",ParticleTextureAnimation::FixedRate}});if(boolean(block,"randomStart",appearancePath,false))animation=animation|ParticleTextureAnimation::RandomStart;appearance.modes[1]=uint32_t(animation);
					appearance.modes[2]=uint32_t(enumeration(block,"billboard",appearancePath,ParticleBillboardMode::CameraFacing,{{"camerafacing",ParticleBillboardMode::CameraFacing},{"screenaligned",ParticleBillboardMode::ScreenAligned},{"cylindrical",ParticleBillboardMode::Cylindrical},{"axislocked",ParticleBillboardMode::AxisLocked},{"velocityaligned",ParticleBillboardMode::VelocityAligned},{"velocitystretched",ParticleBillboardMode::VelocityStretched}}));
					appearance.modes[3]=uint32_t(enumeration(block,"blendClass",appearancePath,ParticleBlendClass::Additive,{{"additive",ParticleBlendClass::Additive},{"alpha",ParticleBlendClass::Alpha},{"weightedoit",ParticleBlendClass::WeightedOit}}));
					if(appearance.textureAndAtlas[2]==0||appearance.textureAndAtlas[3]==0||appearance.modes[0]==0||appearance.modes[0]>appearance.textureAndAtlas[2]*appearance.textureAndAtlas[3])error("MPP-PARTICLE-012","Atlas dimensions must be positive and contain frameCount.",appearancePath);
				}
				result.specification.emitterTemplates.push_back(std::move(authored));
			}

			void parse(Data const& data)
			{
				if (data.getName() != "ParticleEffect") { error("MPP-PARTICLE-001", "Expected ParticleEffect document root.", "/"); return; }
				fields(data,{"version","name","maximumParticleCount","Emitters"},"/ParticleEffect");
				result.specification.version=integer(data,"version","/ParticleEffect",1);
				result.specification.name=value(data,"name","/ParticleEffect",true);
				result.specification.maximumParticleCount=integer(data,"maximumParticleCount","/ParticleEffect",0,true);
				if(result.specification.version!=1)error("MPP-PARTICLE-013","Unsupported particle effect version; expected 1.","/ParticleEffect/version");
				if(!data.hasEntry("Emitters"))error("MPP-PARTICLE-003","Particle effect requires an Emitters list.","/ParticleEffect");
				else { size_t index=0;for(auto const& entry:data.getEntry("Emitters")){if(entry.first!="Emitter"){error("MPP-PARTICLE-002","Emitters contains only Emitter entries.","/ParticleEffect/Emitters");continue;}parseEmitter(entry.second,index++);} }
				uint64_t total=0;for(auto const& emitter:result.specification.emitterTemplates)total+=emitter.value.simulation.shapeSeedModulesBudget[3];
				if(total>std::numeric_limits<uint32_t>::max()||total!=result.specification.maximumParticleCount)error("MPP-PARTICLE-014","maximumParticleCount must equal the sum of emitter-template budgets ("+std::to_string(total)+").","/ParticleEffect/maximumParticleCount");
			}
		};
	}

	ParticleEffectParseResult ParticleEffectParser::fromData(mpp::data::StructuredData const& data, std::string const& sourceName) noexcept
	{
		Reader reader; reader.source=std::move(sourceName);
		try { reader.parse(data); }
		catch(std::exception const& exception) { reader.error("MPP-PARTICLE-000", "Could not parse particle effect: " + std::string(exception.what()), "/"); }
		catch(...) { reader.error("MPP-PARTICLE-000", "Could not parse particle effect.", "/"); }
		return std::move(reader.result);
	}

	ParticleEffectParseResult ParticleEffectParser::fromFile(std::string const& filepath) noexcept
	{
		try { return fromData(detail::readDocumentRoot(filepath), filepath); }
		catch(std::exception const& exception) { ParticleEffectParseResult result;result.diagnostics.error("MPP-PARTICLE-000","Could not read particle effect: "+std::string(exception.what()),{filepath,"/"});return result; }
		catch(...) { ParticleEffectParseResult result;result.diagnostics.error("MPP-PARTICLE-000","Could not read particle effect.",{filepath,"/"});return result; }
	}
}
