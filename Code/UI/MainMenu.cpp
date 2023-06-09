#include "UICommon.hpp"

#include "RavenOSU.hpp"
#include <Events/SystemEvents.hpp>
#include <RavenFont/Font.hpp>
#include <RavenAudio/RavenAudio.hpp>
#include <IInput.h>

#include <filesystem>

namespace OSU::UI {
using namespace Raven;
using namespace Raven::UI;
struct MenuRoot {};
struct SongSelect { std::string Path; std::string BGImage; };

struct PreviewImage {
	Handle<CImage> Image;
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

TEntity SpawnSlider(CWorld& world, const TEntity& parent, const char* name) {
	constexpr float greyIntensity = 21.f / 256.f;
	auto hSliderRoot = Widgets::UINode(world, name, Style {
		.colour  = SColourF::Grey(greyIntensity, 0.8f),
		.padding = Rect::All(Dimension::Pc(0.01f)),
		.size    = Size{ 90_pc, 20_pc},
		.minSize = Size{ 90_pc, 20_pc},
		.maxSize = Size{ 90_pc, 20_pc},
		.eDirection = EFlexDirection::Row,
		.eAlignContent = EAlignContent::Center,
	});
	world.AddChild(parent, hSliderRoot);

	auto hSlider = Widgets::UINode(world, "Slider", Style {
		.colour  = SColourF::White(0.99f),
		.size    = Size{20_pc, 90_pc},
		.minSize = Size{20_pc, 90_pc},
		.maxSize = Size{20_pc, 90_pc},
	});
	world.AddComponent<Button>(hSlider);
	world.AddComponent<Slider>(hSlider);
	world.AddChild(hSliderRoot, hSlider);

	return hSliderRoot;
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
			style.padding.Top = Dimension::Px(list.ScrollOffset);
		}
	}
}

void RemoveDrags(
	const Events<Event::System::SMouseBtnPress>&                      events,
	const Query<With<Slider, DragInteraction>, WithOut<Interaction>>& sliders) {
	for (const auto& e : events) {
		switch(e.eKeyAction) {
			case EKeyAction::Release:
				sliders.storage<DragInteraction>().erase(std::begin(sliders), std::end(sliders));
				break;
			default:
				break;
		}
	}
}

void UpdateDrag(CWorld&                                      world,
				const Events<Event::System::SMouseBtnPress>& events,
				const Query<With<Slider, Interaction>>&      sliders) {
	for (const auto& e : events) {
		sliders.each([&](TEntity hSlider, Slider& s) {
			switch (e.eKeyAction) {
			case EKeyAction::Press:
				world.AddComponent<DragInteraction>(
					hSlider,
					DragInteraction::FromPos(
						{CInput::GetMousePosX(), CInput::GetMousePosY()}));
				break;
			case EKeyAction::Release:
				if(world.Has<DragInteraction>(hSlider))
					world.RemoveComponent<DragInteraction>(hSlider);
				break;
			default:
				break;
			}
		});
	}
}

void UpdateSliderValue(const Events<Event::System::SMouseMove>&       events,
	const Query<With<DragInteraction, Slider, Style, SComputedLayout, SHierarchyComponent>>& sliders,
	const Query<With<SComputedLayout>>& layouts) {
	for(const auto& e: events) {
		sliders.each([&](const TEntity& ent, DragInteraction& drag, Slider& s,
						 Style& style, const SComputedLayout& handleLayout,
						 const SHierarchyComponent& hierarcy) {
			sliders.storage<DragInteraction>().patch(
				ent, [&](DragInteraction& drag) {
					const float2 pos{e.newPosX, e.newPosY};
					drag.Delta   = pos - drag.PrevPos;
					drag.PrevPos = pos;
				});
			sliders.storage<Slider>().patch(ent, [&](Slider& s) {
				const auto& sliderLayout =
					layouts.get<SComputedLayout>(hierarcy.parentId);
				const float2 availableSpace =
					sliderLayout.size - handleLayout.size;

				s.Value += drag.Delta.x / (availableSpace.x);
				s.Value = std::clamp(s.Value, s.From, s.To);
				if (s.OnChange)
					s.OnChange(s.Value);
			});

		});
	}
}

void UpdateSliderHandle(
	const Query<With<Slider, Style, SComputedLayout,
					 SHierarchyComponent /*,Updated<Slider>*/>>& sliders,
	const Query<With<SComputedLayout>>& layouts) {
	sliders.each([&](Slider& slider, Style& style, SComputedLayout& handleLayout,
					 const SHierarchyComponent& hierarchy) {
		const auto& sliderLayout =
			layouts.get<SComputedLayout>(hierarchy.parentId);
		const float2 availableSpace = sliderLayout.size - handleLayout.size;
		style.position.Left         = Dimension::Pc(
					std::min((slider.Value - slider.From) / (slider.To - slider.From),
							 availableSpace.x / sliderLayout.size.x));
	});
}

void SpawnListItems(
	CWorld& world, const Appearance& appearance,
	const Query<With<Initialised<ScrollList>, ScrollList>>& lists) {
	for(auto hList: lists) {
		Detail::ForEachSong([&](std::filesystem::path path) {
			Widgets::SpawnButton(world, hList, SColourF::Black(0.1f),
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
		auto hList   = Widgets::UINode(world, "SongList", Style {
			.colour  = {0.8f, 0.1f, 0.1f, 0.8f},
			.margin  = Rect{.Right = 2_pc},
			.size    = Size::Width(40_pc),
			.maxSize = Size::Width(40_pc),
			.eDirection = EFlexDirection::Column,
		});
		world.AddChild(hMenu, hList);
		world.AddComponent<ScrollList>(hList);
		world.AddComponent<Button>(hList); // For interaction
	}
}

void SpawnSettings(CWorld& world, Appearance& appearance,
				   const Query<With<Initialised<MenuRoot>>>& menus) {
	for (auto hMenu : menus) {
		auto settingWnd =
			UIBuilder(hMenu, world)
				.Create("SettingWnd", Style::VBox(Size::Width(40_pc))
										  .Colour(SColourF::Grey(0.2f, 0.8f))
										  .AlignItems(EAlignItems::Center)
										  .AlignSelf(EAlignSelf::FlexEnd)
										  .MinSize(Size::Width(40_pc))
										  .MaxSize(Size::Width(40_pc))
										  .Margin(Rect{.Left   = 3_pc,
													   .Right  = 20_pc,
													   .Top    = 3_pc,
													   .Bottom = 3_pc})
										  .Border(Rect::All(1_pc)));

		settingWnd.Text("SETTINGS", appearance.Font);

		std::array Sliders = {
			std::tuple{"BG Fade: ", &appearance.BGDim, 0.f, 1.f},
			std::tuple{"Audio Volume: ", &appearance.AudioVolume, 0.f, 1.f},
		};

		settingWnd
			.Create("SettingsRoot", Style::VBox(Size::Width(90_pc))
										.Colour(SColourF::Grey(0.3f, 0.1f))
										.AlignItems(EAlignItems::FlexStart))
			.WithChildren(static_cast<uint32>(Sliders.size()), [&](UIBuilder builder, int idx) {
				builder.Style()
					.Colour(SColourF::Grey(0.9f, 0.1f))
					.MinSize(Size(90_pc, 5_pc))
					.MaxSize(Size(100_pc, 10_pc))
					.Margin(Rect::All(1_pc))
					.FlexShrink(1.f)
					.AlignItems(EAlignItems::Center);

				auto& slider = Sliders[idx];
				constexpr float greyIntensity = 21.f / 256.f;
				builder.SliderFloat(
					std::get<0>(slider), SColourF::Grey(greyIntensity, 0.8f),
					SColourF::Red(0.99f), appearance.Font, *std::get<1>(slider),
					std::get<2>(slider), std::get<3>(slider),
					[pVal = std::get<1>(slider)](float val) { *pVal = val; });
			});
	}
}

void AddPreview(CWorld& world, App& app, SAssetManager& mgr,
				const Query<With<Initialised<Interaction>, SongSelect>,
							WithOut<Audio::Player>>& selected,
				const Assets<CBeatmap>&              beatmaps) {
	for (auto& hSel : selected) {
		const auto& song     = selected.get<SongSelect>(hSel);

		const auto* pBeatmap = beatmaps.Get(
			mgr.Load(app, song.Path).OnSuccess().Typed<CBeatmap>());

		if (!pBeatmap)
			continue;

		auto hSong = mgr.Load(app, pBeatmap->GetSongPath())
						 .OnSuccess()
						 .Typed<Audio::Sound>();

		world.AddComponent<Audio::Player>(hSel,
										  Audio::Player{.Sound         = hSong,
														.PlaybackSpeed = 1.f,
														.Volume        = 1.f,
														.IsLooping     = true,
														.IsPlaying     = true});
		const auto bgImage = LoadBackgroundImage(app, mgr, *pBeatmap);
		if (bgImage) {
			world.AddComponent<PreviewImage>(hSel, bgImage);
		}
	}
}

void RemovePreview(
	CWorld& world, const Query<With<MenuRoot, Audio::Player>>& bgPlayer,
	const Query<With<Removed<Interaction>, SongSelect, Audio::Player>>&
		selected) {
	for (auto& hSel : selected) {
		world.RemoveComponent<Audio::Player>(hSel);
		if (world.Has<PreviewImage>(hSel)) {
			world.RemoveComponent<PreviewImage>(hSel);
		}
	}
}

void EnsureBGMusic(
	CWorld&                                            world,
	const Appearance&                                  appearance,
	const Query<With<MenuRoot, Style, Audio::Player>>& bgPlayer) {
	for (auto hBg : bgPlayer) {
		auto&          bg               = bgPlayer.get<Audio::Player>(hBg);
		auto&          style            = bgPlayer.get<Style>(hBg);
		bool           areSongsSelected = false;
		Handle<CImage> previewImage{};
		const auto selectedPlayers = world.Query<SongSelect, Audio::Player>();
		for (auto hSelected : selectedPlayers) {
			areSongsSelected = true;
			selectedPlayers.get<Audio::Player>(hSelected).Volume =
				appearance.AudioVolume;
			if (world.Has<PreviewImage>(hSelected)) {
				previewImage =
					world.GetComponent<PreviewImage>(hSelected).Image;
			}
			break;
		}

		if (areSongsSelected) {
			bg.Pause();
		} else {
			bg.Play();
			bgPlayer.storage<Audio::Player>().patch(
				hBg, [&appearance](Audio::Player& player) {
					player.Volume = appearance.AudioVolume;
				});
		}

		if (previewImage) {
			world.AddOrReplace<Raven::UI::Image>(hBg, previewImage);
			style.colour = SColourF::Grey(appearance.BGDim, 1.f);
		} else if (!previewImage && world.Has<Raven::UI::Image>(hBg)) {
			world.RemoveComponent<Raven::UI::Image>(hBg);
			style.colour = SColourF::White(0.8f);
		}
	}
}

void SpawnSong(CWorld& world, App& app, SAssetManager& mgr,
			   const Query<With<Interaction, SongSelect>>&  selected,
			   const Events<Event::System::SMouseBtnPress>& events,
			   State<EGameState>&                           stateMachine) {
	for (const auto& e : events) {
		if (e.ePressedBtn != EMouseBtn::Left &&
			e.eKeyAction != EKeyAction::Press)
			continue;
		SongSelect song{};
		for (auto hSelect : selected) {
			song = selected.get<SongSelect>(hSelect);
			break;
		}
		if (song.Path.empty())
			continue;

		RavenLogInfo("Playing: {}", song.Path);
		world.AddComponent<CBeatmapController>(world.CreateEntity("Song"))
			.Beatmap = mgr.Load(app, song.Path).OnSuccess().Typed<CBeatmap>();
		stateMachine.Set(EGameState::Playing);
		break;
	}
}

void OpenMenu(CWorld& world, App& app, SAssetManager& mgr) {
	auto hMenu = Widgets::UINode(world, "Menu Root",
		Style {
			.colour     = SColourF::White(0.8f),
			.size       = Size::All(100_pc),
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
		.AddComponent<Slider>()
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
		.AddSystem(DefaultStages::PRE_UPDATE, &RemoveDrags)
		.AddSystem(DefaultStages::PRE_UPDATE, &UpdateDrag)
		.AddSystem(DefaultStages::PRE_UPDATE, &UpdateSliderValue)
		.AddSystem(DefaultStages::PRE_UPDATE, &UpdateSliderHandle)
		.AddSystem(DefaultStages::PRE_UPDATE, &SpawnSongList)
		.AddSystem(DefaultStages::PRE_UPDATE, &SpawnSettings)
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
	Meta::TypeRegistry::Class_<Slider>()
		.Property(&Slider::From, "From")
		.Property(&Slider::To, "To");
}
