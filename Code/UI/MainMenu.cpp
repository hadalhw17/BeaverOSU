#include "UICommon.hpp"

#include "RavenOSU.hpp"
#include <Events/SystemEvents.hpp>
#include <RavenFont/Font.hpp>
#include <RavenAudio/RavenAudio.hpp>

#include <filesystem>

namespace OSU::UI {
using namespace Raven;
using namespace Raven::UI;
struct MenuRoot {};
struct SongSelect { std::string Path; };
struct ScrollList {
	float ScrollOffset = 0.f;
};

struct UIState {
	TEntity PreviewPlayer{};
};

namespace Detail {
	template<typename FnT> void ForEachSong(FnT f) {
		const auto songsPath = SAssetManager::ResolvePath(App::Get(), "project://Assets/Songs");
		std::filesystem::recursive_directory_iterator iter{songsPath.m_absolutePath};
		for(auto dir: iter) {
			if(dir.is_directory())
				continue;
			const auto p = dir.path();
			if(!p.has_extension() || p.extension() != ".osu")
				continue;
			f(p);
		}
	}
}

void UpdateScrollList(
	const Events<Event::System::SMouseScroll>& events,
	const Query<With<ScrollList, Style, Interaction>>&
		interactedLists) {
	for(const auto& e: events) {
		for(auto hList: interactedLists) {
			auto& list  = interactedLists.get<ScrollList>(hList);
			auto& style = interactedLists.get<Style>(hList);
			list.ScrollOffset += e.yOffset * 100.f;
			style.padding.crossStart = Dimension::Px(list.ScrollOffset);
		}
	}
}

void SpawnListItems(
	CWorld& world, const Appearance& appearance,
	const Query<With<Initialised<ScrollList>, ScrollList>>& lists) {
	for(auto hList: lists) {
		Detail::ForEachSong([&](std::filesystem::path path) {
			Widgets::SpawnButton(world, hList, SColourF{0.f, 0.f, 0.f, 0.1f},
								 appearance.Font,
								 path.filename().stem().string(),
								 Tags::NoSerialise{}, Tags::NoCopy{},
								 SongSelect{
									 .Path = path.string(),
								 });
		});
	}
}

void SpawnSongList(CWorld& world, const Query<With<Initialised<MenuRoot>>>& menus) {
	for(auto hMenu: menus) {
		auto hList = Widgets::UINode(world, "SongList", Style {
			.colour = {0.8f, 0.1f, 0.1f, 1.f},
			.size    = {.width = Dimension::Percent(0.4f), .height = Dimension::Percent(1.f)},
			.maxSize = {.width = Dimension::Percent(0.4f), .height = Dimension::Percent(1.f)},
			.eDirection = EFlexDirection::Column,
		});
		world.AddChild(hMenu, hList);
		world.AddComponent<ScrollList>(hList);
		world.AddComponent<Button>(hList); // For interaction
	}
}

void AddPreview(CWorld& world, App& app, SAssetManager& mgr,
				const Query<With<Initialised<Interaction>, SongSelect>,
							WithOut<Audio::Player>>& selected,
				const Assets<CBeatmap>&              beatmaps) {
	for (auto& hSel : selected) {
		const auto& song  = selected.get<SongSelect>(hSel);
		auto        hSong = mgr.Load(app, beatmaps
											  .Get(mgr.Load(app, song.Path)
													   .OnSuccess()
													   .Typed<CBeatmap>())
											  ->GetSongPath())
						 .OnSuccess()
						 .Typed<Audio::Sound>();

		world.AddComponent<Audio::Player>(hSel,
										  Audio::Player{.Sound         = hSong,
														.PlaybackSpeed = 1.f,
														.Volume        = 1.f,
														.IsLooping     = true,
														.IsPlaying     = true});
	}
}

void RemovePreview(
	CWorld& world, const Query<With<MenuRoot, Audio::Player>>& bgPlayer,
	const Query<With<Removed<Interaction>, SongSelect, Audio::Player>>&
		selected) {
	for (auto& hSel : selected) {
		world.RemoveComponent<Audio::Player>(hSel);
	}
}

void EnsureBGMusic(CWorld&                                     world,
				   const Query<With<MenuRoot, Audio::Player>>& bgPlayer) {
	for (auto hBg : bgPlayer) {
		auto& bg               = bgPlayer.get<Audio::Player>(hBg);
		bool  areSongsSelected = false;
		for ([[maybe_unused]] auto hSelected :
			 world.Query<SongSelect, Audio::Player>()) {
			areSongsSelected = true;
			break;
		}

		if (areSongsSelected) {
			bg.Pause();
		} else {
			bg.Play();
		}
	}
}

void SpawnSong(CWorld& world, App& app, SAssetManager& mgr,
			   const Query<With<Interaction, SongSelect>>&  selected,
			   const Events<Event::System::SMouseBtnPress>& events,
			   State<EGameState>&                           stateMachine) {
	for(const auto& e: events) {
		if (e.ePressedBtn != EMouseBtn::Left &&
			e.eKeyAction != EKeyAction::Press)
			continue;
		SongSelect song{};
		for(auto hSelect: selected) {
			song = selected.get<SongSelect>(hSelect);
			break;
		}
		if(song.Path.empty())
			continue;

		RavenLogInfo("Playing: {}", song.Path);
		world.AddComponent<CBeatmapController>(world.CreateEntity("Song"))
			.Beatmap = mgr.Load(app, song.Path).OnSuccess().Typed<CBeatmap>();
		stateMachine.Set(EGameState::Playing);
		break;
	}
}

void OpenMenu(CWorld& world, App& app, SAssetManager& mgr) {
	auto hMenu = Widgets::UINode(world, "Menu Root", Style {
		.colour  = {1.f, 1.f, 1.f, 0.8f},
		.size    = {.width = Dimension::Percent(1.f), .height = Dimension::Percent(1.f)},
		.minSize = {.width = Dimension::Percent(1.f), .height = Dimension::Percent(1.f)},
		.eDirection = EFlexDirection::RowReverse,
	});
	world.AddComponent<MenuRoot>(hMenu);
	world.AddComponent<Root>(hMenu);
	world.AddComponent<Audio::Player>(
		hMenu,
		Audio::Player{
			.Sound = mgr.Load(app, "project://Assets/Audio/BackgroundAudio.mp3")
						 .OnSuccess()
						 .Typed<Audio::Sound>(),
			.PlaybackSpeed = 1.f,
			.Volume        = 0.4f,
			.IsLooping     = true,
			.IsPlaying     = true});
}

void CloseMenu(CWorld& world, const Query<With<MenuRoot>>& menus) {
	world.RemoveEntity(menus.front());
}

template <typename T> SystemDesc MenuEnterSystem(T&& sys) {
	return SystemDesc{std::forward<T>(sys)}.WithCondition(
		State<EGameState>::OnEnter(EGameState::Menu));
}

template <typename T> SystemDesc MenuLeaveSystem(T&& sys) {
	return SystemDesc{std::forward<T>(sys)}.WithCondition(
		State<EGameState>::OnExit(EGameState::Menu));
}

template <typename T> SystemDesc MenuPauseSystem(T&& sys) {
	return SystemDesc{std::forward<T>(sys)}.WithCondition(
		State<EGameState>::OnPause(EGameState::Menu));
}

template <typename T> SystemDesc MenuResumeSystem(T&& sys) {
	return SystemDesc{std::forward<T>(sys)}.WithCondition(
		State<EGameState>::OnResume(EGameState::Menu));
}

void BuildUIPlugin(Raven::App& app) {
	auto& mgr = *app.GetResource<SAssetManager>();
	app
		.AddComponent<ScrollList>()
		.AddComponent<MenuRoot>()
		.AddComponent<SongSelect>()
		.CreateResource<Appearance>(Appearance {
			.Font = mgr.Load(app, "engine://Assets/Textures/Fonts/"
								  "BalooBhaijaan2-Regular.ttf")
						.OnSuccess()
						.Typed<Font>(),
		})
		.CreateResource<UIState>()
		.AddSystem(DefaultStages::PRE_UPDATE, &UpdateScrollList)
		.AddSystem(DefaultStages::PRE_UPDATE, &SpawnSongList)
		.AddSystem(DefaultStages::PRE_UPDATE, &SpawnListItems)
		.AddSystem(DefaultStages::PRE_UPDATE, &SpawnSong)
		.AddSystem(DefaultStages::UPDATE, &AddPreview)
		.AddSystem(DefaultStages::UPDATE, &RemovePreview)
		.AddSystem(DefaultStages::POST_UPDATE, &EnsureBGMusic)
		.AddSystem(OSU::StateStage, MenuEnterSystem(&OpenMenu))
		.AddSystem(OSU::StateStage, MenuResumeSystem(&OpenMenu))
		.AddSystem(OSU::StateStage, MenuLeaveSystem(&CloseMenu))
		.AddSystem(OSU::StateStage, MenuPauseSystem(&CloseMenu))
		;
}
}

RAVEN_REFLECTION_BLOCK {
	using namespace Raven;
	using namespace OSU::UI;
	Meta::TypeRegistry::Class_<MenuRoot>();
	Meta::TypeRegistry::Class_<SongSelect>().Property(&SongSelect::Path, "Path");
	Meta::TypeRegistry::Class_<ScrollList>();
}
