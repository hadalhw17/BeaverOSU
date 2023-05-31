#pragma once

namespace Raven {
class CImage;
}

namespace OSU {
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
	int HPDrainRate       = 1;
	int CircleSize        = 1;
	int OverallDifficulty = 1;
	int ApproachRate      = 1;
	int SliderMultiplier  = 1;
	int SliderTickRate    = 1;
};

struct General {
	std::string AudioFilename;
	int         AudioLeadIn = 0;
	std::string AudioHash;
	int         PreviewTime = 0;
	int         Countdown   = 0;
};

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
} // namespace OSU