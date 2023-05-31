#include <startup.hpp>

#include <RavenSprite/Sprite.hpp>
#include <RavenUI/RavenUI.hpp>
#include <RavenVFX/ParticleSystem.hpp>
#include <RavenAudio/RavenAudio.hpp>

#include "RavenOSU.hpp"

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
} // namespace Detail

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

class CBeatmapLoader : public Raven::IAssetLoader {
  public:
	using AssetT = CBeatmap;

	void LoadHitObjects(std::istringstream&     data,
						std::vector<HitObject>& hitObjects) {
		hitObjects.clear();
		std::string    line;
		OSU::HitObject hitObject{};
		while (std::getline(data, line)) {
			auto substrings = GetSubstrings(line);
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
						GetSubstrings(curveParams.substr(2), '|');
					const char curveType = curveParams[0];
					curve.Type           = curveType == 'B' ? HitCurve::Bezier
										 : curveType == 'C' ? HitCurve::Centerpetal
										 : curveType == 'L' ? HitCurve::Linear
															: HitCurve::PerfectCircle;
					// Iterate all params that are subdivided by :
					while (i < params.size()) {
						const auto points = GetSubstrings(params[i++], ':');
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

	void LoadGeneralInfo(CBeatmap& map, std::istringstream& data, const Context& ctx) {
		#define READ_INT(name) \
			if(substrings[0] == #name) { \
				map.m_general.name = std::stoi(substrings[1]); \
			}

		#define READ_STRING(name) \
			if(substrings[0] == #name) { \
				map.m_general.name = substrings[1]; \
			}

		std::filesystem::path path{ctx.absolutePath};
		path = path.parent_path();

		std::string line;
		while(std::getline(data, line)) {
			auto substrings = GetSubstrings(line, ':');
			if(line.empty() || substrings.size() != 2 || line[0] == '\r')
				break;

			if(substrings[0] == "AudioFilename") {
				map.m_general.AudioFilename =
					std::filesystem::path{path / substrings[1]}.string();
			}
			READ_INT(AudioLeadIn);
			READ_STRING(AudioHash);
			READ_INT(PreviewTime);
			READ_INT(Countdown);
		}
		#undef READ_INT
		#undef READ_STRING
	}

	void LoadDifficulty(CBeatmap& map, std::istringstream& data) {
		#define READ_DIFFICULTY(name) \
			if(substrings[0] == #name) { \
				map.m_difficulty.name = std::stoi(substrings[1]); \
			}
		std::string line;
		while(std::getline(data, line)) {
			auto substrings = GetSubstrings(line, ':');
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
				LoadGeneralInfo(map, osuFileStream, ctx);
			} else if(line.starts_with("[Difficulty]")) {
				LoadDifficulty(map, osuFileStream);
			}
		}
	}

	Result Load(Raven::App& app, Context ctx) final {
		CBeatmap map{};
		ParseFile(map, ctx);
		return Result::Success(app.GetResource<Raven::Assets<CBeatmap>>()
								   ->Create(std::move(map))
								   .Untyped());
	}

	void GetSupportedFormats(std::vector<std::string_view>& out) const final {
		out.emplace_back("osu");
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
		auto hSong      = mgr.Load(app, pBeatmap->GetSongPath())
						 .OnSuccess()
						 .Typed<Audio::Sound>();
		world.AddOrReplace<Audio::Player>(hBmap,
										  Audio::Player{.Sound         = hSong,
														.PlaybackSpeed = 1.f,
														.Volume        = 1.f,
														.IsLooping     = true,
														.IsPlaying     = true});

		comp.CurrentTime = 0;
		comp.MaxTime     = pBeatmap->GetHitObjects().back().Time;
		for (const auto& hit : pBeatmap->GetHitObjects()) {
			auto hHit = world.CreateChild(hBmap);
			world.AddComponent<Raven::Tags::NoSerialise>(hHit);
			world.AddComponent<Raven::Tags::NoCopy>(hHit);
			world.AddComponent<HitObject>(hHit, hit);
			world.AddComponent<STransformComponent>(hHit).m_translation = {
				hit.X, hit.Y, 0.f};
		}
	}
}

void AdvanceSimulation(
	const Query<With<CBeatmapController, SParentComponent>>& controllers,
	const CTimestep&                                         ts) {

	for (const auto& hController : controllers) {
		controllers.get<CBeatmapController>(hController).CurrentTime +=
			static_cast<int64>(ts.GetMilliseconds());
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
	return skin;
}

void BuildRenderingPlugin(Raven::App& app);
struct Plugin {
	void Build(Raven::App& app) {
		using namespace Raven;
		app.AddPlugin<Audio::Plugin>()
			.AddPlugin<UI::Plugin>()
			.AddPlugin<Particles::Plugin>()
			.AddAsset<CBeatmap>()
			.AddLoader<CBeatmapLoader>()
			.AddComponent<CBeatmapController>()
			.AddComponent<HitObject>()
			.AddSystem(DefaultStages::PRE_UPDATE, &InitialiseHitObjects)
			.AddSystem(DefaultStages::LAST, &AdvanceSimulation)
			.CreateResource<OSU::Skin>(
				LoadSkin(app, "project://Assets/Skins/- YUGEN -/"));
		BuildRenderingPlugin(app);
	}
};

} // namespace OSU

int main(int argc, char* argv[]) {
	SEngineStartupParams params{};

	params.m_windowWidth       = -1;
	params.m_windowHeight      = -1;
	params.m_bInitialiseEditor = true;
	params.m_argc              = argc;
	params.m_pArgv             = argv;

	InitialiseEngine(params);

	Raven::App& app = Raven::App::Get();
	app.AddPlugin<OSU::Plugin>().Run();

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