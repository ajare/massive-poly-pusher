#pragma once

namespace particle_editor
{
	class ParticleDocument;

	class ParticleInspector
	{
		int mSelectedEmitter{ 0 };

	public:
		void draw(ParticleDocument& document);
	};
}
