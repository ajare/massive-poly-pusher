#include <array>
#include <sstream>

#include "mpp/resource-parsers/DocumentWriter.h"
#include "mpp/resource-parsers/ParticleEffectSerializer.h"
#include "mpp/resource-parsers/StructuredDataWriteNode.h"

namespace mpp::resource_parsers
{
	namespace
	{
		template<typename Range> std::string values(Range const& range)
		{
			std::ostringstream output; bool first=true;for(auto const& item:range){if(!first)output<<' ';first=false;output<<item;}return output.str();
		}
		char const* shape(uint32_t value){static char const* names[]{"point","line","box","sphere","hemisphere","disc","cone"};return value<7?names[value]:"point";}
		char const* billboard(uint32_t value){static char const* names[]{"cameraFacing","screenAligned","cylindrical","axisLocked","velocityAligned","velocityStretched"};return value<6?names[value]:"cameraFacing";}
		void curve(StructuredDataWriteNode* parent,char const* name,ParticleCurve const& value)
		{
			auto node=parent->createChild(name);node->createChild("default")->setValue(value.defaultValue);auto keys=node->createChild("Keys");for(auto const& key:value.keys){auto item=keys->createChild("Key");item->createChild("time")->setValue(key.time);item->createChild("value")->setValue(key.value);}
		}
	}

	void ParticleEffectSerializer::toFile(ParticleEffectSpecification const& specification, std::string const& filepath)
	{
		StructuredDataWriteNode root("ParticleEffect");root.createChild("version")->setValue(specification.version);root.createChild("name")->setValue(specification.name);root.createChild("maximumParticleCount")->setValue(specification.maximumParticleCount);auto emitters=root.createChild("Emitters");
		for(auto const& authored:specification.emitterTemplates)
		{
			auto node=emitters->createChild("Emitter");auto const& emitter=authored.value;auto const& sim=emitter.simulation;auto const& appearance=emitter.appearance;
			node->createChild("name")->setValue(authored.name);node->createChild("maximumParticleCount")->setValue(sim.shapeSeedModulesBudget[3]);
			std::array<float,16> transform{};for(int column=0;column<4;++column)for(int row=0;row<4;++row)transform[size_t(column*4+row)]=emitter.localTransform[column][row];node->createChild("transform")->setValue(values(transform));
			auto spawn=node->createChild("Spawn");spawn->createChild("shape")->setValue(shape(sim.shapeSeedModulesBudget[0]));spawn->createChild("shapeParameters")->setValue(values(sim.shapeParameters));spawn->createChild("seed")->setValue(sim.shapeSeedModulesBudget[1]);spawn->createChild("mode")->setValue(sim.emissionState[0]?"burst":"continuous");spawn->createChild("enabled")->setValue(sim.emissionState[1]!=0);spawn->createChild("rate")->setValue(sim.emissionRateAndPadding[0]);spawn->createChild("burstCount")->setValue(sim.emissionState[2]);spawn->createChild("initialVelocityMin")->setValue(values(sim.initialVelocityMin));spawn->createChild("initialVelocityMax")->setValue(values(sim.initialVelocityMax));spawn->createChild("colourMin")->setValue(values(sim.colourMin));spawn->createChild("colourMax")->setValue(values(sim.colourMax));spawn->createChild("lifetime")->setValue(values(std::array<float,2>{sim.lifetimeSizeRanges[0],sim.lifetimeSizeRanges[1]}));spawn->createChild("size")->setValue(values(std::array<float,2>{sim.lifetimeSizeRanges[2],sim.lifetimeSizeRanges[3]}));spawn->createChild("rotation")->setValue(values(std::array<float,2>{sim.rotationRanges[0],sim.rotationRanges[1]}));spawn->createChild("angularVelocity")->setValue(values(std::array<float,2>{sim.rotationRanges[2],sim.rotationRanges[3]}));
			auto behaviours=node->createChild("Behaviours");auto modules=sim.shapeSeedModulesBudget[2];if(modules&uint32_t(ParticleBehaviourModule::Gravity)){auto block=behaviours->createChild("Gravity");block->createChild("acceleration")->setValue(values(std::array<float,3>{sim.gravityAndDrag[0],sim.gravityAndDrag[1],sim.gravityAndDrag[2]}));}if(modules&uint32_t(ParticleBehaviourModule::Drag)){auto block=behaviours->createChild("Drag");block->createChild("coefficient")->setValue(sim.gravityAndDrag[3]);}if(modules&uint32_t(ParticleBehaviourModule::Noise)){auto block=behaviours->createChild("Noise");block->createChild("frequency")->setValue(values(std::array<float,3>{sim.noiseFrequencyStrength[0],sim.noiseFrequencyStrength[1],sim.noiseFrequencyStrength[2]}));block->createChild("strength")->setValue(sim.noiseFrequencyStrength[3]);block->createChild("scroll")->setValue(values(std::array<float,3>{sim.noiseScrollAndTimeScale[0],sim.noiseScrollAndTimeScale[1],sim.noiseScrollAndTimeScale[2]}));block->createChild("timeScale")->setValue(sim.noiseScrollAndTimeScale[3]);}
			auto curves=node->createChild("Curves");curve(curves,"Size",emitter.curves[size_t(ParticleScalarCurve::Size)]);curve(curves,"Alpha",emitter.curves[size_t(ParticleScalarCurve::Alpha)]);curve(curves,"VelocityMultiplier",emitter.curves[size_t(ParticleScalarCurve::VelocityMultiplier)]);curve(curves,"Drag",emitter.curves[size_t(ParticleScalarCurve::Drag)]);curve(curves,"RotationSpeed",emitter.curves[size_t(ParticleScalarCurve::RotationSpeed)]);curve(curves,"EmissiveIntensity",emitter.curves[size_t(ParticleScalarCurve::EmissiveIntensity)]);auto colour=curves->createChild("Colour");colour->createChild("default")->setValue(values(emitter.colourGradient.defaultColour));auto colourKeys=colour->createChild("Keys");for(auto const& key:emitter.colourGradient.keys){auto item=colourKeys->createChild("Key");item->createChild("time")->setValue(key.time);item->createChild("colour")->setValue(values(key.colour));}
			auto block=node->createChild("Appearance");if(!authored.albedoTexture.empty())block->createChild("texture")->setValue(authored.albedoTexture);block->createChild("tint")->setValue(values(std::array<float,3>{appearance.tintAndAlpha[0],appearance.tintAndAlpha[1],appearance.tintAndAlpha[2]}));block->createChild("alpha")->setValue(appearance.tintAndAlpha[3]);block->createChild("emissiveIntensity")->setValue(appearance.appearance[0]);block->createChild("softFadeDistance")->setValue(appearance.appearance[1]);block->createChild("atlasColumns")->setValue(appearance.textureAndAtlas[2]);block->createChild("atlasRows")->setValue(appearance.textureAndAtlas[3]);block->createChild("frameCount")->setValue(appearance.modes[0]);auto playback=appearance.modes[1]&ParticleTexturePlaybackMask;block->createChild("animation")->setValue(playback==uint32_t(ParticleTextureAnimation::FrameOverLife)?"frameOverLife":playback==uint32_t(ParticleTextureAnimation::FixedRate)?"fixedRate":"none");block->createChild("randomStart")->setValue((appearance.modes[1]&ParticleTextureRandomStartBit)!=0);block->createChild("animationRate")->setValue(appearance.appearance[2]);block->createChild("billboard")->setValue(billboard(appearance.modes[2]));block->createChild("blendClass")->setValue(appearance.modes[3]==uint32_t(ParticleBlendClass::Alpha)?"alpha":"additive");
		}
		detail::writeDocument(root.toStructuredData(),filepath);
	}
}
