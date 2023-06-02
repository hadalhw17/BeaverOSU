#include "RavenOSU.hpp"

#include <RavenApp/RavenApp.hpp>
#include <RavenCommon/Mesh.hpp>
#include <RavenSprite/Sprite.hpp>
#include <RenderFunction.hpp>
#include <IRavenRenderer.h>

namespace OSU {
using namespace Raven;
class CRenderingCache {
  public:
	std::pair<Handle<CMesh>, CMesh*> GetMesh(Assets<CMesh>& meshes) {
		if(!hMesh) {
			hMesh = meshes.Create(CMesh{"OSUMesh"}.AllowRaytracing(false));
		}
		return {hMesh, meshes.GetMut(hMesh)};
	}

	Handle<Sprite::SpriteMaterial>
	GetSpriteMaterialForTexture(Assets<Sprite::SpriteMaterial>& materials,
								Handle<CImage> hTexture, const bool isFont) {

		auto& hMat = m_materialCache[hTexture.Index()];
		if(hMat && materials.Contains(hMat))
			return hMat;

		Sprite::SpriteMaterial mat{};
		mat.IsFont          = isFont;
		mat.Texture         = hTexture.ToWeak();
		hMat                = materials.Create(mat);
		return hMat;
	}

	std::vector<std::pair<uint32, HandleUntyped>> PreviousMaterials;
  private:
	// Weak handles based on texture+colour lookup
	std::unordered_map<AssetId, Handle<Sprite::SpriteMaterial>> m_materialCache;
	Handle<CMesh>                                               hMesh;
};

namespace Geometry {
	using DefaultIdxT = uint16;
	template <typename IdxT = DefaultIdxT>
	static void AddQuad(CMesh& mesh, float2 tl, float2 tr, float2 bl, float2 br,
						float4 colour, float uStart = 0.f, float uEnd = 1.f,
						float vStart = 0.f, float vEnd = 1.f) {

		const size_t totalVtx = 4;
		const size_t totalIdx = 6;
		auto         posIt    = mesh.AppendAndReturnHead<float3>(
            SVertexStreamMap::POSITION, totalVtx);
		auto tcIt = mesh.AppendAndReturnHead<float2>(SVertexStreamMap::TEXCOORD,
													 totalVtx);
		auto colIt = mesh.AppendAndReturnHead<float4>(SVertexStreamMap::COLOUR,
													  totalVtx);
		auto idxIt = mesh.AppendAndReturnHead<IdxT>(totalIdx);
		const size_t firstVertex = mesh.FindStream(SVertexStreamMap::POSITION)->size() - totalVtx;


		posIt[0]   = float3{bl.xy, 0.f};
		tcIt [0]   = {uStart, vEnd};
		colIt[0]   = colour;

		posIt[1]   = float3{tr.xy, 0.f};
		tcIt [1]   = {uEnd, vStart};
		colIt[1]   = colour;

		posIt[2]   = float3{tl.xy, 0.f};
		tcIt [2]   = {uStart, vStart};
		colIt[2]   = colour;

		posIt[3]   = float3{br.xy, 0.f};
		tcIt [3]   = {uEnd, vEnd};
		colIt[3]   = colour;

		uint32 currIdx  = 0;
		idxIt[currIdx++]    = static_cast<IdxT>(firstVertex + 0);
		idxIt[currIdx++]    = static_cast<IdxT>(firstVertex + 1);
		idxIt[currIdx++]    = static_cast<IdxT>(firstVertex + 2);

		idxIt[currIdx++]    = static_cast<IdxT>(firstVertex + 0);
		idxIt[currIdx++]    = static_cast<IdxT>(firstVertex + 3);
		idxIt[currIdx++]    = static_cast<IdxT>(firstVertex + 1);
	}
} // namespace Geometry

struct ExtractedHitObject {
	float2 Position;
	Handle<CImage>  Image;
	float Radius;
	float ApproachCircleScale;
	float SliderT;
	float Opacity;
	std::vector<int2> SliderPoints;
};
using TExtractedObjects = std::vector<ExtractedHitObject>;

void ExtractActiveObjects(TExtractedObjects& dst, 
	CWorld& world,
	const Query<With<CBeatmapController, SParentComponent>>& controllers,
	const Query<With<VisibilityProperties, HitObject, WorldSpaceTransform, DifficultyProperties>>& visibleObjects
) {
	visibleObjects.each([&](const VisibilityProperties& vis,
							const HitObject&            hitObj,
							const WorldSpaceTransform&  xForm,
							const DifficultyProperties& props) {
		auto& obj = dst.emplace_back(ExtractedHitObject{
			.Position            = xForm.m_translation.xy(),
			.Radius              = props.Radius,
			.ApproachCircleScale = vis.ApproachAmount,
			.SliderT             = vis.SliderT,
			.Opacity = std::clamp(vis.TimeSinceSpawn, 0.f, props.FadeIn) /
					   props.FadeIn,
		});

		if (hitObj.Type == HitObject::Slider) {
			obj.SliderPoints =
				std::get<HitCurve>(hitObj.ObjectParams).CurvePoints;
			obj.SliderPoints.insert(std::begin(obj.SliderPoints), obj.Position);
		}
	});
}
} // namespace OSU

constexpr int BinomialCoefficient(const int n, const int k) {
	if (k == 0 || k == n) {
		return 1;
	} else {
		return BinomialCoefficient(n - 1, k - 1) +
			   BinomialCoefficient(n - 1, k);
	}
}

float2 ComputeBezierPoint(const float                  t,
						  const std::span<int2 const>& controlPoints) {
	float2    result(0.0f);
	const int n = static_cast<int>(controlPoints.size()) - 1;
	for (int i = 0; i <= n; ++i) {
		const float binomialCoef =
			static_cast<float>(BinomialCoefficient(n, i));
		const float powT =
			static_cast<float>(glm::pow(1.f - t, n - i) * glm::pow(t, i));
		result += binomialCoef * powT * float2{controlPoints[i]};
	}
	return result;
}

template<>
struct Raven::TComponentRenderSystem<OSU::HitObject> {
	static void Draw(CWorld& world, IFrameContext& ctx, const OSU::Skin& skin,
					 OSU::TExtractedObjects&         extracted,
					 Assets<Sprite::SpriteMaterial>& materials,
					 Assets<CMesh>& meshes, OSU::CRenderingCache& cache) {

		using namespace Raven;
		constexpr float2 OSUResolution = {640, 480};
		const float2     renderRes     = ctx.GetRenderInfo().m_resolution;
		const float2     resScale      = renderRes / OSUResolution;
		const float ar                 = renderRes.x / renderRes.y;
		auto fromOSUPixels = [resScale](const float2 px) {
			return px * resScale;
		};
		const size_t spriteCount = extracted.size();
		if(spriteCount <= 0)
			return;

		auto [hMesh, pMesh] = cache.GetMesh(meshes);

		if(!cache.PreviousMaterials.empty()) {
			SRenderMesh mesh{};
			mesh.cullMask = ~0u;
			mesh.hMesh    = hMesh;
			ctx.AddMeshPrimitives(mesh, cache.PreviousMaterials, ECoreRenderPhases::HUD);
			cache.PreviousMaterials.clear();
		}
		pMesh->Reset();

		auto addMaterialPrimitive = [lastBatchSprite = 0u, pMesh, &cache, &materials](const Handle<CImage>& img, const uint32 spriteIdx, const bool isFont) mutable {
			const uint32 diff = spriteIdx - lastBatchSprite;
			const auto hMat = cache.GetSpriteMaterialForTexture(materials, img, isFont).Untyped();
			pMesh->AddPrimitive(SMeshPrimitive {
				.hMaterial    = hMat,
				.indexOffset  = lastBatchSprite * 6,
				.indexCount   = diff * 6,
				.vertexOffset = 0,
				.vertexCount  = diff * 4,
			});
			cache.PreviousMaterials.emplace_back(static_cast<uint32>(pMesh->GetRenderPrimitives().size() - 1), hMat);
			lastBatchSprite = spriteIdx;
		};

		int spriteIdx = 0;
		auto queueSprite = [&](const float2 pos, const float2 size,
							   const Handle<CImage>& img,
							   const float           opacity) mutable {

			float uStart = 0.f;
			float uEnd   = 1.f;
			float vStart = 0.f;
			float vEnd   = 1.f;

			const float2 bl = pos + float2(-1.f, 1.f)  * size;
			const float2 tr = pos + float2(1.f, -1.f)  * size;
			const float2 tl = pos + float2(-1.f, -1.f) * size;
			const float2 br = pos + float2(1.f, 1.f)   * size;

			OSU::Geometry::AddQuad(*pMesh, tl, tr, bl, br, float4{1.f, 1.f, 1.f, opacity}, uStart, uEnd, vStart, vEnd);
			addMaterialPrimitive(img, ++spriteIdx, false);
		};

		auto queueCurve = [&](std::span<int2 const> points, const float2 size,
							  const Handle<CImage>& img,
							  const float           opacity) mutable {

			RavenAssert(!points.empty(), "Invalid curve data!");

			constexpr size_t TessLevel = 50;
			const float uStep = 1.f / static_cast<float>(TessLevel);

			constexpr float CurveRoudness = 0.5f;

			float uStart = 0.f;
			float uEnd   = CurveRoudness;
			float vStart = 0.f;
			float vEnd   = 1.f;

			float2 pos = fromOSUPixels(ComputeBezierPoint(0, points));
			float2 end = fromOSUPixels(ComputeBezierPoint(uStep, points));
			float2 dir  = glm::normalize(end - pos);
			float2 perp = float2{-dir.y, dir.x};
			float2 tl = pos - perp * size;
			float2 bl = pos + perp * size;
			float2 tr = end - perp * size;
			float2 br = end + perp * size;

			for(size_t t = 1; t < TessLevel; ++t) {
				OSU::Geometry::AddQuad(*pMesh, tl, tr, bl, br, float4{1.f, 1.f, 1.f, opacity}, uStart, uEnd, vStart, vEnd);
				++spriteIdx;

				pos = end;
				end = fromOSUPixels(ComputeBezierPoint((t + 1) * uStep, points));

				dir  = glm::normalize(end - pos);
				perp = float2{-dir.y, dir.x};

				tl = tr;
				bl = br;
				tr = end - perp * size;
				br = end + perp * size;

				//uStart = uEnd;
				//uEnd   = uEnd + uStep;
				uStart = CurveRoudness;
				uEnd   = CurveRoudness;
			}

			OSU::Geometry::AddQuad(*pMesh, tl, tr, bl, br, float4{1.f, 1.f, 1.f, opacity}, 1.f - CurveRoudness, 1.f, vStart, vEnd);
			++spriteIdx;

			addMaterialPrimitive(img, spriteIdx, false);
		};


		const auto hHitCircle = skin.Images.find("hitcircle");
		RavenAssert(hHitCircle != std::end(skin.Images), "Invalid skin!");
		const auto hApproachCircle = skin.Images.find("approachcircle");
		RavenAssert(hApproachCircle != std::end(skin.Images), "Invalid skin!");
		const auto hHitCircleOverlay = skin.Images.find("hitcircleoverlay");
		RavenAssert(hHitCircleOverlay != std::end(skin.Images), "Invalid skin!");
		const auto hSliderB = skin.Images.find("sliderb0");
		RavenAssert(hSliderB != std::end(skin.Images), "Invalid skin!");

		auto drawHitCircle = [&](const float2 pos, const float2 approachSize,
								 const float2 hitSize,
								 const float  opacity) mutable {
			if(approachSize.x > 0.f && approachSize.y > 0.f) {
				queueSprite(pos, approachSize, hApproachCircle->second, opacity);
			}
			queueSprite(pos, hitSize, hHitCircle->second, opacity);
			queueSprite(pos, hitSize, hHitCircleOverlay->second, opacity);
		};

		for(const auto& ext: extracted) {
			const float2 hitSize      = fromOSUPixels(float2{ext.Radius} / float2{ar, 1.f});
			const float2 approachSize = hitSize * 2.f * ext.ApproachCircleScale;
			if(ext.SliderPoints.empty()) {
				const float2 pos = fromOSUPixels(ext.Position);
				drawHitCircle(pos, approachSize, hitSize, ext.Opacity);
			} else {
				const float2 p0 =
					fromOSUPixels(ComputeBezierPoint(0.f, ext.SliderPoints));
				const float2 p1 =
					fromOSUPixels(ComputeBezierPoint(1.f, ext.SliderPoints));

				queueCurve(ext.SliderPoints, hitSize, hSliderB->second,
						   ext.Opacity);
				drawHitCircle(p0, approachSize, hitSize, ext.Opacity);
				if (ext.SliderT != 0.f) {
					drawHitCircle(fromOSUPixels(ComputeBezierPoint(
									  ext.SliderT, ext.SliderPoints)),
								  approachSize, hitSize, ext.Opacity);
				}
				drawHitCircle(p1, float2{0.f}, hitSize, ext.Opacity);
			}
		}

		pMesh->ComputeBounds();
		extracted.clear();
	}
};

namespace OSU {
void BuildRenderingPlugin(Raven::App& app) {
	app.CreateResource<OSU::CRenderingCache>()
		.CreateResource<OSU::TExtractedObjects>()
		.AddSystem(Raven::Renderer::Stages::EXTRACT, &OSU::ExtractActiveObjects)
		.AddPlugin<Raven::TRenderSystemFor<OSU::HitObject>>();
}
} // namespace OSU
