#pragma once
#include <RavenWorld/WorldDefs.hpp>
#include <RavenApp/RavenApp.hpp>

namespace Raven {
class CImage;
}

namespace OSU {
enum class EGameState{
	None = 0,
	Menu,
	Playing,
	Paused,
};
constexpr inline auto StateStage = Raven::DefaultStages::FIRST;

struct HitCurve {
	enum Type {
		Bezier = 0,
		Centerpetal,
		Linear,
		PerfectCircle,

		Count,
	};

	Type              Type;
	std::vector<int2> CurvePoints;
	int               Slides;
	float             Length;
};
struct HitObject {
	enum Type {
		Circle  = 0,
		Slider  = 1,
		Spinner = 3,
	};
	int                                    X, Y;
	int                                    Time;
	int                                    Type;
	std::variant<std::monostate, HitCurve> ObjectParams{std::monostate{}};
};
struct Difficulty {
	float HPDrainRate       = 1.f;
	float CircleSize        = 1.f;
	float OverallDifficulty = 1.f;
	float ApproachRate      = 1.f;
	float SliderMultiplier  = 1.f;
	float SliderTickRate    = 1.f;
};

struct General {
	std::string AudioFilename;
	int         AudioLeadIn = 0;
	std::string AudioHash;
	int         PreviewTime = 0;
	int         Countdown   = 0;
};

struct DifficultyProperties {
	float Radius;
	float Preempt;
	float FadeIn;
	float DurationSingle = 0.f;
	float DurationTotal = 0.f;
};

struct VisibilityProperties {
	float TimeSinceSpawn;
	float ApproachAmount; // 0 to 1 where 1 is fully apprached
	float SliderT;
};

struct Hovered{};
struct ActiveMousePos{ uint2 Pos; float2 ToOsuScale; float2 FromOSUScale; Raven::TEntity CursorEntity; };

struct Skin {
	using TImageMap =
		std::unordered_map<Raven::HashedString, Raven::Handle<Raven::CImage>>;

	TImageMap Images;
};

class CBeatmapLoader;
class CBeatmap {
  public:
	CBeatmap()  = default;
	~CBeatmap() = default;

	std::span<HitObject const> GetHitObjects() const { return m_hitObjects; }
	std::string_view  GetSongPath() const { return m_general.AudioFilename; }
	const Difficulty& GetDifficulty() const { return m_difficulty; }
	const General&    GetGeneral() const { return m_general; }

  private:
	friend class CBeatmapLoader;
	General                m_general{};
	Difficulty             m_difficulty{};
	std::vector<HitObject> m_hitObjects;
};

struct CBeatmapController {
	Difficulty              Difficulty{};
	Raven::Handle<CBeatmap> Beatmap;
	int64                   CurrentTime = 0;
	int64                   MaxTime     = 0;
};

struct GameScores {
	int32 Score = 0;
	int32 Combo = 0;
	int32 MaxCombo = 0;
	int32 Hit300 = 0;
	int32 Hit100 = 0;
	int32 Hit50 = 0;
	int32 HitMiss = 0;
	int32 ScoreRaw = 0;
};

template <typename T> Raven::SystemDesc GameStartSystem(T&& sys) {
	return Raven::SystemDesc{std::forward<T>(sys)}.WithCondition(
		Raven::State<EGameState>::OnEnter(EGameState::Playing));
}

template <typename T> Raven::SystemDesc GameExitSystem(T&& sys) {
	return Raven::SystemDesc{std::forward<T>(sys)}.WithCondition(
		Raven::State<EGameState>::OnExit(EGameState::Playing));
}

} // namespace OSU