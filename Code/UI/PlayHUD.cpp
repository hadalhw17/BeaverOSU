#include "UICommon.hpp"
#include <Events/SystemEvents.hpp>

namespace OSU::UI {
using namespace Raven;
using namespace Raven::UI;

struct HUDRoot{};
struct ComboText{};
struct KeyVisual{EKey key;};
struct AccuracyText{};

void SpawnHUD(CWorld& world, const Appearance& appearance, App& app, SAssetManager& mgr, const Assets<CBeatmap>& beatmaps, const Query<With<CBeatmapController>>& gameControllers) {
	const CBeatmapController& controller = gameControllers.GetSingle();

	auto* pBeatmap = beatmaps.Get(controller.Beatmap);
	const auto hBGImage = LoadBackgroundImage(app, mgr, *pBeatmap);
	// Root vbox
	auto hRoot = Widgets::UINode(world, "GameHUD", Style { 
		.colour  = hBGImage ? SColourF::Grey(appearance.BGDim) : SColourF::Black(),
		.size    = Size::All(100_pc),
		.eDirection = EFlexDirection::Column,
		.eJustifyContent = EFlexJustifyContent::SpaceBetween,
	});
	UIBuilder root{hRoot, world};
	root.WithComponent<Root, HUDRoot>();

	if (hBGImage) {
		world.AddComponent<Raven::UI::Image>(
			hRoot, Raven::UI::Image{.hImg = hBGImage});
	}

	// Top hbox
	root.Create("Top", Style{.colour     = SColourF::Red(0.f),
							 .size       = Size::Height(10_pc),
							 .eDirection = EFlexDirection::RowReverse})
		.Text("Accuracy", appearance.Font, SColourF::White())
		.WithComponent<AccuracyText>().Style().FlexBasis(10_pc);

	// Center vbox
	root.Create("Center",
				Style{
					.colour     = SColourF::Green(0.f),
					.minSize    = Size::Height(Dimension::Pc(0.5f)),
					.eDirection = EFlexDirection::Row,
				})
		.Create("KeyMap", Style::VBox(Size{.width = 3_pc, .height = 10_pc})
							  .AlignSelf(EAlignSelf::Center)
							  .Colour(SColourF::Grey(0.1f, 0.5f))
							  .MaxSize(Size{.width = 3_pc, .height = 10_pc})
							  .MinSize(Size{.width = 3_pc, .height = 10_pc})
							  .JustifyContent(EFlexJustifyContent::SpaceEvenly))
		.WithChildren(2, [&](UIBuilder keyVis, int i) {
			constexpr static std::array Keys = {
				std::tuple{EKey::D, SColourF::Red(0.7f)},
				std::tuple{EKey::F, SColourF::Blue(0.7f)},
			};
			const auto& key = Keys[i];
			keyVis.WithComponent<KeyVisual>({KeyVisual{std::get<EKey>(key)}})
				.Style()
				.Colour(std::get<SColourF>(key))
				.Margin(Rect::Horizontal(3_pc))
				.Flex(1.f, 1.f);
			keyVis.Text(KeyToString(std::get<EKey>(key)), appearance.Font);
		});


	//Bottom hbox
	root.Create("Bottom",
				Style{
					.colour = SColourF::Blue(0.f),
					.size   = Size::Height(10_pc),
				})
		.Text("Combo", appearance.Font, SColourF::White())
		.WithComponent<ComboText>();
}

void UpdateCombo(const Query<With<GameScores>>&      scores,
				 const Query<With<ComboText, Text>>& texts) {
	const GameScores& score = scores.GetSingle();
	for(auto hText : texts) {
		auto& text = texts.get<Text>(hText);
		text.text = std::to_string(score.Combo);
	}
}

void UpdateAccuracy(const Query<With<GameScores>>&         scores,
					const Query<With<AccuracyText, Text>>& texts) {
	const GameScores& score = scores.GetSingle();
	for (auto hText : texts) {
		auto&      text = texts.get<Text>(hText);
		const auto potential =
			(score.Hit300 + score.Hit100 + score.Hit50 + score.HitMiss) * 300;
		text.text =
			fmt::format("{}%/100%\n{}", (static_cast<float>(score.ScoreRaw) /
								std::max(static_cast<float>(potential), 1.f)) *
								   100.f, score.Score);
	}
}

void HightlightKeys(const Events<Event::System::SKeyPress>& events,
					Query<With<KeyVisual, Style>>&          query) {
	for (const auto& e : events) {
		for (auto hVis : query) {
			auto& vis = query.get<Style>(hVis);
			if(query.get<KeyVisual>(hVis).key != e.ePressedKey)
				continue;

			switch (e.eKeyAction) {
			case EKeyAction::Press:
				vis.colour.m_colour.a += 0.2f;
				break;
			case EKeyAction::Release:
				vis.colour.m_colour.a -= 0.2f;
				break;
			default:
				break;
			}
		}
	}
}

void RemoveHUD(CWorld& world, Query<With<HUDRoot>>& roots) {
	for (auto hHub : roots) {
		world.RemoveEntity(hHub);
	}
}

template <typename T> SystemDesc HUDEnterSystem(T&& sys) {
	return GameStartSystem(std::forward<T>(sys));
}

template <typename T> SystemDesc HUDLeaveSystem(T&& sys) {
	return GameExitSystem(std::forward<T>(sys));
}

void BuildPlayerHUD(App& app) {
	app
		.AddComponent<HUDRoot>()
		.AddSystem(OSU::StateStage, HUDEnterSystem(&SpawnHUD))
		.AddSystem(OSU::StateStage, HUDLeaveSystem(&RemoveHUD))
		.AddSystem(DefaultStages::POST_UPDATE, &UpdateCombo)
		.AddSystem(DefaultStages::POST_UPDATE, &UpdateAccuracy)
		.AddSystem(DefaultStages::UPDATE, &HightlightKeys)
		;
}
} // namespace OSU::UI