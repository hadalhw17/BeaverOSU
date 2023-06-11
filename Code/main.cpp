#include <startup.hpp>

#include <RavenSprite/Sprite.hpp>
#include <RavenUI/RavenUI.hpp>
#include <RavenVFX/ParticleSystem.hpp>
#include <RavenAudio/RavenAudio.hpp>
#include <Events/SystemEvents.hpp>
#include "IInput.h"

#include "RavenOSU.hpp"
#include <RavenWorld/DefaultComponents.hpp>
#include <RavenRenderer/RenderOutput.hpp>
#include <CVar.hpp>

namespace OSU {
using namespace Raven;
namespace Detail {
	std::string_view Trim(const std::string_view& str,
						  const std::string_view  whitespace = " \t\r\n") {
		const auto strBegin = str.find_first_not_of(whitespace);
		if (strBegin == std::string::npos) {
			return "";
		}
		const auto strEnd   = str.find_last_not_of(whitespace);
		const auto strRange = strEnd - strBegin + 1;
		return str.substr(strBegin, strRange);
	}

	std::vector<std::string> GetSubstrings(std::string_view string,
										   char             delim = ',') {
		std::vector<std::string> ret;
		size_t                   begin = 0;
		size_t                   it    = string.find_first_of(delim);
		while (begin != it) {
			ret.emplace_back(Detail::Trim(string.substr(begin, it - begin)));
			if (it == std::string::npos)
				break;
			begin = it = it + 1;
			it         = string.find_first_of(delim, it);
		}

		return ret;
	}

} // namespace Detail

struct ScoreDriver {
	int Score = 0;
	float TimeSinceSpawn = 0;
	float TotalTime      = 0;
	TEntity EffectEntity{};
};

float4 GetScoreSpriteCol(const int score) {
	return score == 300 ? float4{0.f, 1.f, 0.f, 1.f}
		 : score == 100 ? float4{0.f, 0.f, 1.f, 1.f}
		 : score == 50  ? float4{1.f, 0.f, 0.f, 1.f}
						: float4{0.f, 0.f, 0.f, 1.f};
}

Handle<CImage> GetScoreSpriteTexture(const int score, const Skin& skin) {
	return score == 300 ? skin.Images.find("hit300")->second
		 : score == 100 ? skin.Images.find("hit100")->second
		 : score == 50  ? skin.Images.find("hit50")->second
						: skin.Images.find("hit0")->second;
}

void CreateScoreDriver(CWorld& world, const Skin& skin, const Raven::TEntity& dst, const int score, float2 pos, float extraDuration) {
	constexpr int64 ScoreLifetime = 500;
	auto hScoreHitEnt = world.CreateEntity("HitEffect");
	world.AddComponent<ScoreDriver>(dst, score, 0.f, ScoreLifetime + extraDuration, hScoreHitEnt);

	world.AddComponent<STransformComponent>(hScoreHitEnt).m_translation = float3{pos, 10.f};
	world.AddComponent<Sprite::SSprite>(hScoreHitEnt).hImg =
		GetScoreSpriteTexture(score, skin);
}

class CBeatmapLoader : public Raven::IAssetLoader {
  public:
	using AssetT = CBeatmap;

	void LoadHitObjects(std::istringstream&     data,
						std::vector<HitObject>& hitObjects) {
		hitObjects.clear();
		std::string    line;
		OSU::HitObject hitObject{};
		while (std::getline(data, line)) {
			auto substrings = Detail::GetSubstrings(line);
			if (substrings.empty() || line[0] == '\r')
				break;
			hitObject.X    = std::stoi(substrings[0]);
			hitObject.Y    = std::stoi(substrings[1]);
			hitObject.Time = std::stoi(substrings[2]);
			const int type = std::stoi(substrings[3]);
			hitObject.Type =
				IsBitSet(type, HitObject::Circle)   ? HitObject::Circle
				: IsBitSet(type, HitObject::Slider) ? HitObject::Slider
													: HitObject::Spinner;
			hitObject.ObjectParams = std::monostate{};
			if (hitObject.Type == HitObject::Slider) {
				// Get all object params
				RavenAssert(substrings.size() >= 6, "Invalid curve data!");
				int      i = 0;
				HitCurve curve;
				{
					const auto& curveParams = substrings[5];
					const auto  params =
						Detail::GetSubstrings(curveParams.substr(2), '|');
					const char curveType = curveParams[0];
					curve.Type           = curveType == 'B' ? HitCurve::Bezier
										 : curveType == 'C' ? HitCurve::Centerpetal
										 : curveType == 'L' ? HitCurve::Linear
															: HitCurve::PerfectCircle;
					// Iterate all params that are subdivided by :
					while (i < params.size()) {
						const auto points = Detail::GetSubstrings(params[i++], ':');
						if (points.size() != 2)
							break;
						curve.CurvePoints.emplace_back(std::stoi(points[0]),
													   std::stoi(points[1]));
					}
					i = 2;
				}
				curve.Slides           = std::stoi(substrings[6]);
				curve.Length           = std::stof(substrings[7]);

				hitObject.ObjectParams = std::move(curve);
			}

			hitObjects.emplace_back(hitObject);
		}
	}

	void LoadEvents(CBeatmap& map, std::istringstream& data) {

		std::string line;
		while(std::getline(data, line)) {
			auto substrings = Detail::GetSubstrings(line, ',');
			if(line.empty() || line[0] == '\r')
				break;
			[[maybe_unused]] const bool isEvent = substrings.size() == 3;
			[[maybe_unused]] const bool isBG    = substrings.size() == 5 && substrings[0][0] == '0' && substrings[1][0] == '0';
			[[maybe_unused]] const bool isVideo = substrings.size() == 5 && (substrings[0] == "Video" || substrings[0][0] == '1');
			[[maybe_unused]] const bool isBreak = substrings.size() == 3 && substrings[0] == "2";

			if(isBG) {
				map.m_backgroundPath = fmt::format("{}/{}", map.m_path, Detail::Trim(substrings[2], " \"\r"));
			}

		}
	}

	void LoadGeneralInfo(CBeatmap& map, std::istringstream& data) {
		#define READ_GENERAL(name)                                                      \
			if (substrings[0] == #name) {                                              \
				Parse(substrings[1], map.m_general.name);                              \
			}

		std::string line;
		while(std::getline(data, line)) {
			auto substrings = Detail::GetSubstrings(line, ':');
			if(line.empty() || substrings.size() != 2 || line[0] == '\r')
				break;

			if(substrings[0] == "AudioFilename") {
				map.m_general.AudioFilename = fmt::format("{}/{}", map.m_path, substrings[1]);
			}
			READ_GENERAL(AudioLeadIn);
			READ_GENERAL(AudioHash);
			READ_GENERAL(PreviewTime);
			READ_GENERAL(Countdown);
		}
		#undef READ_GENERAL
	}

	void LoadDifficulty(CBeatmap& map, std::istringstream& data) {
		#define READ_DIFFICULTY(name)                                                  \
			if (substrings[0] == #name) {                                              \
				Parse(substrings[1], map.m_difficulty.name);                           \
			}
		std::string line;
		while(std::getline(data, line)) {
			auto substrings = Detail::GetSubstrings(line, ':');
			if(line.empty() || substrings.size() != 2 || line[0] == '\r')
				break;

			READ_DIFFICULTY(HPDrainRate);
			READ_DIFFICULTY(CircleSize);
			READ_DIFFICULTY(OverallDifficulty);
			READ_DIFFICULTY(ApproachRate);
			READ_DIFFICULTY(SliderMultiplier);
			READ_DIFFICULTY(SliderTickRate);
		}
		#undef READ_DIFFICULTY
	}

	void ParseFile(CBeatmap& map, const Context& ctx) {
		std::istringstream osuFileStream(
			std::string(reinterpret_cast<const char*>(ctx.bytes.data()),
						ctx.bytes.size()));

		if (osuFileStream.fail()) {
			return;
		}

		auto& hitObjects = map.m_hitObjects;
		hitObjects.clear();

		std::string line;
		while (std::getline(osuFileStream, line)) {
			if (line.starts_with("[HitObjects]")) {
				LoadHitObjects(osuFileStream, hitObjects);
			} else if(line.starts_with("[General]")) {
				LoadGeneralInfo(map, osuFileStream);
			} else if(line.starts_with("[Difficulty]")) {
				LoadDifficulty(map, osuFileStream);
			} else if(line.starts_with("[Events]")) {
				LoadEvents(map, osuFileStream);
			}
		}
	}

	Result Load(Raven::App& app, Context ctx) final {
		CBeatmap map{};
		std::filesystem::path path{ctx.absolutePath};
		map.m_path = path.parent_path().string();
		ParseFile(map, ctx);
		return Result::Success(app.GetResource<Raven::Assets<CBeatmap>>()
								   ->Create(std::move(map))
								   .Untyped());
	}

	void GetSupportedFormats(std::vector<std::string_view>& out) const final {
		out.emplace_back("osu");
	}

  private:

	template<typename T>
	void Parse(const std::string& string, T& out);

	template<>
	void Parse(const std::string& string, int& out) {
		out = std::stoi(string);
	}
	template<>
	void Parse(const std::string& string, float& out) {
		out = std::stof(string);
	}

	template<>
	void Parse(const std::string& string, std::string& out) {
		out = string;
	}
};

void InitialiseHitObjects(
	CWorld& world, App& app, SAssetManager& mgr,
	const Raven::Assets<CBeatmap>& beatmaps,
	const Raven::Query<Raven::With<CBeatmapController,
								   Raven::Initialised<CBeatmapController>>>&
		components) {

	for (auto hBmap : components) {
		auto& comp     = components.get<CBeatmapController>(hBmap);
		auto* pBeatmap = beatmaps.Get(comp.Beatmap);
		if (!pBeatmap)
			continue;
		comp.Difficulty = pBeatmap->GetDifficulty();

		// Offset hit objects to not be clipped by screen
		world.AddOrReplace<STransformComponent>(hBmap).m_translation.xy = float2{100, 100};
		world.AddComponent<GameScores>(hBmap);
		auto hSong      = mgr.Load(app, pBeatmap->GetSongPath())
						 .OnSuccess()
						 .Typed<Audio::Sound>();
		world.AddOrReplace<Audio::Player>(hBmap,
										  Audio::Player{.Sound         = hSong,
														.PlaybackSpeed = 1.f,
														.Volume        = 1.f,
														.IsLooping     = false,
														.IsPlaying     = true});

		comp.CurrentTime = 0;
		comp.MaxTime     = pBeatmap->GetHitObjects().back().Time;
		for (const auto& hit : pBeatmap->GetHitObjects()) {
			auto hHit = world.CreateChild(hBmap);
			world.AddComponent<Tags::NoSerialise>(hHit);
			world.AddComponent<Tags::NoCopy>(hHit);
			world.AddComponent<S2DTransformTag>(hHit);
			world.AddComponent<HitObject>(hHit, hit);
			world.AddComponent<STransformComponent>(hHit).m_translation = {
				hit.X, hit.Y, 0.f};
		}
	}
}

void AdvanceSimulation(
	const Query<With<CBeatmapController, SParentComponent, Audio::Player>>&
					   controllers,
	State<EGameState>& stateMachine) {

	for (const auto& hController : controllers) {
		const auto& player = controllers.get<Audio::Player>(hController);
		if (!player.IsPlaying) {
			stateMachine.Set(EGameState::Menu);
			break;
		}
		controllers.get<CBeatmapController>(hController).CurrentTime =
			player.PlayingTime;
	}
}

void RemoveAllMaps(CWorld& world, const Query<With<CBeatmapController>>& beatmaps) {
	for (const auto& hController : beatmaps) {
		world.RemoveEntity(hController);
	}
}

void EndSimulation(
	CWorld& world,
	const Query<With<CBeatmapController, SParentComponent, Audio::Player>>& controllers,
	State<EGameState>& stateMachine) {

	bool isDone = true;
	for (const auto& hController : controllers) {
		isDone = isDone && !controllers.get<Audio::Player>(hController).IsPlaying;
	}
	if(isDone) {
		stateMachine.Set(EGameState::Menu);
	}

}

Skin LoadSkin(App& app, std::string_view skinDir) {
	auto&                mgr = *app.GetResource<SAssetManager>();
	Skin                 skin{};
	constexpr std::array ImagesToLoad = {
		"approachcircle",
		"hitcircle",
		"hitcircleoverlay",
		"sliderb0",
		"hit0",
		"hit50",
		"hit100",
		"hit300",
		"cursor",
	};
	for (const auto* pImg : ImagesToLoad) {
		std::string path = std::string{skinDir} + pImg + ".png";
		auto        hRes = mgr.Load(app, path);
		if (hRes.IsSuccess()) {
			skin.Images[pImg] = hRes.OnSuccess().Typed<CImage>();
		} else {
			RavenLogWarning("Failed to load image {} from {}", pImg, skinDir);
			RavenLogWarning("Reason: {}", hRes.OnFailed());
		}
	}

	app.GetResource<Window::Cursors>()->Set(Window::ECursorType::Arrow, skin.Images["cursor"]);
	return skin;
}

void ComputeDifficultyProps(
	CWorld& world,
	const Query<With<CBeatmapController, SParentComponent>>& controllers,
	const Query<With<HitObject>, WithOut<DifficultyProperties>>& toExtract) {
	auto computeRadius = [](const float cs) {
		constexpr float BaseRad      = 54.4f;
		constexpr float ShrinkFactor = 4.48f;
		return BaseRad - ShrinkFactor * cs;
	};
	auto computeFadeIn = [](const float ar) {
		if (ar < 5) {
			return 800.f + 400.f * (5.f - ar) / 5.f;
		} else if (ar == 5) {
			return 800.f;
		} else {
			return 800.f - 500.f * (ar - 5.f) / 5.f;
		}
	};

	auto computePreempt = [](const float ar) {
		if (ar < 5) {
			return 1200.f + 600.f * (5.f - ar) / 5.f;
		} else if (ar == 5) {
			return 1200.f;
		} else {
			return 1200.f - 750.f * (ar - 5.f) / 5.f;
		}
	};

	auto computeSliderLength = [](const int   sliderRepeat,
								  const float sliderPixelLength,
								  float sliderMul, float velocity,
								  float beatLen = 300) {
		return (sliderPixelLength /
				(std::max(sliderMul, 0.01f) * 100.f * velocity) * beatLen) *
			   (std::max(sliderRepeat, 1));
	};

	for(const auto& hController: controllers) {
		auto next = controllers.get<SParentComponent>(hController).first;
		const auto& controller = controllers.get<CBeatmapController>(hController);
		const auto fadein  = computeFadeIn(controller.Difficulty.ApproachRate);
		const auto preempt = computePreempt(controller.Difficulty.ApproachRate);

		while(next) {
			const auto& hitObj = toExtract.get<HitObject>(next);
			const std::pair<float, float> duration = [&] {
				if(hitObj.Type == HitObject::Slider) {
					const auto& slider = std::get<HitCurve>(hitObj.ObjectParams);
					const float len = computeSliderLength(slider.Slides, slider.Length,
											  controller.Difficulty.SliderMultiplier,
											  1.f/* TODO: Slider velocity multiplier comes from timing points*/);
					return std::pair<float, float>{len, len * slider.Slides};
				} else {
					return std::pair{0.f, 0.f};
				}
			}();
			world.AddOrReplace<DifficultyProperties>(
				next,
				DifficultyProperties{
					.Radius  = computeRadius(controller.Difficulty.CircleSize),
					.Preempt = preempt,
					.FadeIn  = fadein,
					.DurationSingle = duration.first,
					.DurationTotal  = duration.second,
				});

			next = world.GetComponent<SHierarchyComponent>(next).next;
		}
	}
}

void ComputeVisibleProps(
	CWorld&                                                  world,
	const Query<With<CBeatmapController, SParentComponent>>& controllers,
	const Query<With<DifficultyProperties, HitObject>>&      toExtract) {
	for (const auto& hController : controllers) {
		auto        next = controllers.get<SParentComponent>(hController).first;
		const auto& controller =
			controllers.get<CBeatmapController>(hController);
		const auto currentTime = controller.CurrentTime;

		while (next) {
			const auto& hitObj = toExtract.get<HitObject>(next);
			const auto& props  = toExtract.get<DifficultyProperties>(next);
			const float dt     = static_cast<float>(currentTime - hitObj.Time);
			const bool  isVisible =
				dt > 0 && dt < props.Preempt + props.DurationTotal;

			if (isVisible) {
				const auto durationFac = dt - props.Preempt;
				const auto iteration   = static_cast<int>(
                    std::floor(durationFac / props.DurationSingle));
				const auto slideTime =
					durationFac - iteration * props.DurationSingle;
				const auto t = iteration % 2 == 0
								 ? slideTime / props.DurationSingle
								 : 1.f - (slideTime / props.DurationSingle);

				world.AddOrReplace<VisibilityProperties>(
					next,
					VisibilityProperties{
						.TimeSinceSpawn = dt,
						.ApproachAmount = glm::lerp(
							1.f, 0.5f,
							std::clamp(dt, 0.f, props.Preempt) / props.Preempt),
						.SliderT = t * static_cast<float>(dt >= props.Preempt),
					});
			} else {
				if (world.Has<VisibilityProperties>(next)) {
					world.RemoveComponent<VisibilityProperties>(next);
				}
			}

			next = world.GetComponent<SHierarchyComponent>(next).next;
		}
	}
}

void GetMousePos(CWorld&                                  world,
				 const Events<Event::System::SMouseMove>& mouseMove,
				 const Query<With<SRenderInfo>>&          renderInfos) {
	const auto hInfo = renderInfos.front();
	const auto& RI = renderInfos.get<SRenderInfo>(hInfo);
	for (const auto& e : mouseMove) {
		const uint2 cursorPos = uint2{static_cast<uint32>(e.newPosX),
									  static_cast<uint32>(e.newPosY)} -
								RI.CursorPositionOffset;

		constexpr float2 OSUResolution     = {640, 480};
		constexpr float2 TextureResolution = {1024, 768};

		const float2     renderRes         = float2{RI.m_resolution} * 0.9f;
		const float2     resScale          = renderRes / OSUResolution;
		const float2     texScale          = renderRes / TextureResolution;

		if(!world.Has<ActiveMousePos>(hInfo)) {
			auto hCursor = world.CreateEntity("CursorParticles");
			world.AddComponent<Tags::NoCopy>(hCursor);
			world.AddComponent<Tags::NoSerialise>(hCursor);
			world.AddComponent<STransformComponent>(hCursor);
			world.AddOrReplace<Particles::Emitter>(
				hCursor,
				Particles::Emitter{
					.ToSpawn = 100,
				});

			world.AddComponent<ResolutionConversion>(hInfo);
			world.AddOrReplace<ActiveMousePos>(hInfo,
											   ActiveMousePos{
												   .Pos          = cursorPos,
												   .CursorEntity = hCursor,
											   });
		}

		auto& pos             = world.GetComponent<ActiveMousePos>(hInfo);
		pos.Pos               = cursorPos;

		auto& conv            = world.GetComponent<ResolutionConversion>(hInfo);
		conv.AspectRatio      = renderRes.x / renderRes.y;
		conv.FromOSUScale     = resScale;
		conv.ToOsuScale       = 1.f / resScale;
		conv.FromTextureScale = texScale;
		conv.ToTextureScale   = 1.f / texScale;
	}
}

bool AreKeysDown() {
	return CInput::IsKeyDown(EKey::D) || CInput::IsKeyDown(EKey::F);
}

void UpdateHovered(CWorld& world, const Skin& skin,
				   const Query<With<VisibilityProperties, WorldSpaceTransform,
									DifficultyProperties, HitObject>>& objs,
				   const Query<With<ActiveMousePos, ResolutionConversion>>& activeMouse) {
	const auto& [mouse, conv] = activeMouse.GetSingle();
	for(auto hObj: objs) {
		const auto& pos = objs.get<WorldSpaceTransform>(hObj).m_translation.xy();
		const auto& dif = objs.get<DifficultyProperties>(hObj);
		const auto& vis = objs.get<VisibilityProperties>(hObj);

		const bool  isHovered = glm::distance(pos, float2{float2{mouse.Pos} * conv.ToOsuScale}) < dif.Radius;
		if(isHovered) {
			world.AddOrReplace<Hovered>(hObj);
			if(AreKeysDown() && !world.Has<ScoreDriver>(hObj)) {
				int score = 0;
				if(vis.ApproachAmount >= 0.8) {
					score = 300;
				} else if(vis.ApproachAmount >= 0.5) {
					score = 100;
				} else {
					score = 50;
				}
				CreateScoreDriver(world, skin, hObj, score, pos * conv.FromOSUScale, dif.DurationTotal);
			}
		} else if (world.Has<Hovered>(hObj)) {
			world.RemoveComponent<Hovered>(hObj);
		}
	}
}

void CleanUpInteractions(CWorld& world,
				   const Query<With<Hovered, WithOut<VisibilityProperties>>>& objs,
				   const Query<With<ActiveMousePos>>& activeMouse) {
	world.GetRegistry().remove<Hovered>(objs.begin(), objs.end());
}

void MarkMissedNotes(CWorld& world, const Skin& skin,
					 const Query<With<Removed<VisibilityProperties>, HitObject,
									  WorldSpaceTransform>,
								 WithOut<ScoreDriver>>& missed,
					 const Query<With<ResolutionConversion>>& activeMouse) {
	const ResolutionConversion& res = activeMouse.GetSingle();
	for(const auto hMissed: missed) {
		const auto pos = missed.get<WorldSpaceTransform>(hMissed).m_translation.xy();
		CreateScoreDriver(world, skin, hMissed, 0, pos * res.FromOSUScale, 0);
	}
}

void UpdateScore(CWorld& world, const Query<With<ScoreDriver>>& scores,
				 const Query<With<STransformComponent, Sprite::SSprite>>& effects,
				 const CTimestep&                        ts) {
	for(auto& hScore: scores) {
		auto& score = scores.get<ScoreDriver>(hScore);
		score.TimeSinceSpawn += ts.GetMilliseconds();
		const float scoreAlpha = 1.f / score.TotalTime;
		auto& effectXForm  = effects.get<STransformComponent>(score.EffectEntity);
		auto& effectSprite = effects.get<Sprite::SSprite>(score.EffectEntity);
		effectXForm.m_translation.y -= scoreAlpha * 100.f;
		effectSprite.customSize += scoreAlpha * 100.f;

		if(score.TimeSinceSpawn >= score.TotalTime) {
			world.RemoveEntity(score.EffectEntity);
			world.RemoveComponent<ScoreDriver>(hScore);
			if(world.Has<Sprite::SSprite>(hScore))
				world.RemoveComponent<Sprite::SSprite>(hScore);
		}
	}
}

void CollectScores(
	const Query<With<GameScores>>&                            scores,
	const Query<With<Initialised<ScoreDriver>, ScoreDriver>>& newScores) {
	GameScores& collector = scores.GetSingle();
	for (auto hScore : newScores) {
		auto& score = newScores.get<ScoreDriver>(hScore);
		switch (score.Score) {
		case 0: {
			collector.Combo = 0;
			collector.HitMiss++;
			break;
		}
		case 50: {
			collector.Combo++;
			collector.Hit50++;
			break;
		}
		case 100: {
			collector.Combo++;
			collector.Hit100++;
			break;
		}
		case 300: {
			collector.Combo++;
			collector.Hit300++;
			break;
		}
		default:
			RavenLogWarning("Undefined score value: {}!", score.Score);
			break;
		}
		collector.ScoreRaw += score.Score;
		collector.Score += score.Score * std::max(1, collector.Combo);
		collector.MaxCombo = std::max(collector.Combo, collector.MaxCombo);
	}
}

void UpdateCursorParcile(
	CWorld& world, const Query<With<SRenderInfo, ActiveMousePos>>& cursor,
	const Query<With<Particles::Emitter, STransformComponent>>& particles) {

	for (const auto& hCursor : cursor) {
		const auto& RI = cursor.get<SRenderInfo>(hCursor);
		const auto& pos = cursor.get<ActiveMousePos>(hCursor);
		auto& emitter = particles.get<Particles::Emitter>(pos.CursorEntity);
		float2 particlePos = pos.Pos;
		particles.get<STransformComponent>(pos.CursorEntity).m_translation =
			RI.m_mainCamera.ScreenToWorld(
				float2{particlePos} / float2{RI.m_resolution}, 0.1f);
		emitter.Burst(100);
	}
}

struct GameState {
};

void ToggleSimulation(Raven::App& app, GameState& state, Raven::CWorld& world) {
	auto* pState = app.GetResource<State<EGameState>>();
	if(!world.GetSimulationEnabled() && pState) {
		app.RemoveResource<State<EGameState>>();
	} else if(world.GetSimulationEnabled() && !pState) {
		app.CreateResource<State<EGameState>>();
		app.GetResource<State<EGameState>>()->Push(EGameState::Menu);
	}


	// Create game settings if they were not created manually
	//if(world.Query<GameSettings>().empty()) {
	//	world.AddComponent<GameSettings>(world.CreateEntity("GameSettings"));
	//}
}

void CreateGameWorld(Raven::App& app, Assets<CWorld>& worlds) {
	if(app.GetResource<IRavenEditor>() || app.GetResource<GameWorld>())
		return;
	auto hWorld = worlds.Create();
	app.CreateResource<GameWorld>(hWorld);

	auto* pWorld = worlds.GetMut(hWorld);
	pWorld->SetSimulationEnabled(true);
	pWorld->SetIsPaused(false);

	auto hCam = pWorld->CreateEntity("Camera");
	auto& cam = pWorld->AddComponent<SCameraComponent>(hCam);
	cam.isActive = true;
	pWorld->AddComponent<STransformComponent>(hCam).m_translation.y = 10.f;
	pWorld->AddOrReplace<SRenderInfo>(hCam);
	const auto& hWnd = *app.GetResource<Raven::Window::MainWindow>();
	app.GetResource<Window::Windows>()->Get(hWnd)->Name("Raven OSU");
	pWorld->AddComponent<Raven::RenderTargetT>(hCam, hWnd.m_hId);
}

void BuildRenderingPlugin(Raven::App& app);
namespace UI {
	void BuildUIPlugin(Raven::App& app);
	void BuildPlayerHUD(Raven::App& app);
	void BuildSplashScreen(Raven::App& app);
}

template <typename T> SystemDesc GameSystem(T&& sys) {
	return SystemDesc{std::forward<T>(sys)}.WithCondition(
		State<EGameState>::OnUpdate(EGameState::Playing));
}

template <typename T> SystemDesc PauseSystem(T&& sys) {
	return SystemDesc{std::forward<T>(sys)}.WithCondition(
		State<EGameState>::OnUpdate(EGameState::Paused));
}

template <typename T> SystemDesc MenuSystem(T&& sys) {
	return SystemDesc{std::forward<T>(sys)}.WithCondition(
		State<EGameState>::OnUpdate(EGameState::Menu));
}

struct Plugin {
	void Build(Raven::App& app) {
		using State = State<EGameState>;
		app
			.CreateResource<GameState>()
			.AddPlugin<Audio::Plugin>()
			.AddState<OSU::EGameState>(OSU::StateStage)
			.AddPlugin<Raven::UI::Plugin>()
			.AddPlugin<Particles::Plugin>()
			.AddAsset<CBeatmap>()
			.AddLoader<CBeatmapLoader>()
			.AddComponent<CBeatmapController>()
			.AddComponent<HitObject>()
			.AddComponent<VisibilityProperties>() // To dispatch signals
			.AddComponent<ScoreDriver>() // To dispatch signals
			.AddSystem(OSU::StateStage, MenuEnterSystem(&OSU::CreateGameWorld))
			.AddSystem(DefaultStages::FIRST, &ToggleSimulation)
			.AddSystem(OSU::StateStage, GameStartSystem(&OSU::InitialiseHitObjects))
			.AddSystem(DefaultStages::PRE_UPDATE, &OSU::ComputeDifficultyProps)
			.AddSystem(DefaultStages::PRE_UPDATE, &OSU::GetMousePos)
			.AddSystem(DefaultStages::UPDATE, &OSU::ComputeVisibleProps)
			.AddSystem(DefaultStages::UPDATE, &OSU::MarkMissedNotes)
			.AddSystem(DefaultStages::UPDATE, &OSU::UpdateHovered)
			.AddSystem(DefaultStages::UPDATE, &OSU::UpdateScore)
			.AddSystem(DefaultStages::POST_UPDATE, &OSU::CollectScores)
			//.AddSystem(Raven::DefaultStages::UPDATE, &OSU::UpdateCursorParcile)
			.AddSystem(OSU::StateStage, GameSystem(&AdvanceSimulation))
			//.AddSystem(OSU::StateStage, GameSystem(&EndSimulation))
			.AddSystem(OSU::StateStage, GameExitSystem(&RemoveAllMaps))
			.AddSystem(DefaultStages::LAST, &CleanUpInteractions)
			.CreateResource<OSU::Skin>(
				LoadSkin(app, "project://Assets/Skins/- YUGEN -/"));
		BuildRenderingPlugin(app);
		OSU::UI::BuildUIPlugin(app);
		OSU::UI::BuildPlayerHUD(app);
	}
};

} // namespace OSU

int main(int argc, char* argv[]) {
	using namespace Raven;

	SEngineStartupParams params{};

	params.m_windowWidth       = -1;
	params.m_windowHeight      = -1;
	params.m_bInitialiseEditor = false;
	params.m_argc              = argc;
	params.m_pArgv             = argv;

	InitialiseEngine(params);

	Raven::App& app = Raven::App::Get();
	Raven::Handle<Raven::CWorld> hGameWorld{};

	OSU::UI::BuildSplashScreen(app);
	app.AddPlugin<OSU::Plugin>();

	if(!params.m_bInitialiseEditor) {
#if 0
		auto hRes = app.GetResource<Raven::SAssetManager>()->Load(
			app, "project://Assets/MyLove.rlevel");
		hGameWorld = hRes.OnSuccess().Typed<Raven::CWorld>();
#else
		//hGameWorld = app.GetResource<Assets<CWorld>>()->Create();
#endif
		Raven::ICVarManager::Get()->SetBoolCVar("r.VolumetricFog", false);

	}

	app.Run();

	return 0;
}

RAVEN_REFLECTION_BLOCK {
	using namespace Raven::Meta;
	using namespace OSU;
	TypeRegistry::Class_<CBeatmap>();

	TypeRegistry::Class_<CBeatmapController>()
		.Property(&CBeatmapController::Beatmap, "Beatmap")
		.Property(&CBeatmapController::CurrentTime, "Current Time")
		.Property(&CBeatmapController::MaxTime, "Max Time")
		.Property(&CBeatmapController::Difficulty, "Difficulty");

	TypeRegistry::Class_<General>()
		.Property(&General::AudioFilename, "Audio Filename")
		.Property(&General::AudioLeadIn, "Audio Lead In")
		.Property(&General::PreviewTime, "Preview Time")
		.Property(&General::Countdown, "Countdown");
	TypeRegistry::Class_<Difficulty>()
		.Property(&Difficulty::HPDrainRate, "HP Drain Rate")
		.Property(&Difficulty::CircleSize, "Circle Size")
		.Property(&Difficulty::OverallDifficulty, "Overall Difficulty")
		.Property(&Difficulty::ApproachRate, "Approach Rate")
		.Property(&Difficulty::SliderMultiplier, "Slider Multiplier")
		.Property(&Difficulty::SliderTickRate, "Slider Tick Rate");

	TypeRegistry::Class_<HitObject>();
}