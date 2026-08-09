#include "mpp/RenderPipelineFlow.h"

#include <stdexcept>

namespace mpp
{
	char const* renderFlowEventKindName(RenderFlowEventKind kind)
	{
		switch (kind)
		{
		case RenderFlowEventKind::PassBegin: return "pass begin";
		case RenderFlowEventKind::PassEnd: return "pass end";
		case RenderFlowEventKind::BatchSubmission: return "batch submission";
		case RenderFlowEventKind::MsaaResolve: return "MSAA resolve";
		case RenderFlowEventKind::Taa: return "TAA";
		case RenderFlowEventKind::SsaaHorizontal: return "SSAA horizontal";
		case RenderFlowEventKind::SsaaVertical: return "SSAA vertical";
		case RenderFlowEventKind::Fxaa: return "FXAA";
		case RenderFlowEventKind::Presentation: return "presentation";
		}
		throw std::invalid_argument("Unknown render-flow event kind.");
	}
}
