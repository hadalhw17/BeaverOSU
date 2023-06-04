#include "UICommon.hpp"

namespace OSU::UI {
using namespace Raven;
using namespace Raven::UI;

struct HUDRoot{};
struct ComboText{};
struct AccuracyText{};

void SpawnHUD(CWorld& world, const Appearance& appearance) {
	auto hHUD = world.CreateEntity("GameHUD");
	world.AddComponent<STransformComponent>(hHUD);
	// Root vbox
	auto hRoot = Widgets::UINode(world, "GameHUD", Style { 
		.colour  = {1.f, 1.f, 1.f, 0.0f},
		.size    = Size::All(Dimension::Pc(1.f)),
		.minSize = Size::All(Dimension::Pc(1.f)),
		.eDirection = EFlexDirection::Column,
	});
	world.AddComponent<Root>(hRoot);
	world.AddComponent<HUDRoot>(hRoot);

	// Top hbox
	auto hTop = Widgets::UINode(world, "Top", Style {
		.colour     = {1.f, 0.f, 0.f, 0.f},
		.size       = Size::Height(Dimension::Pc(0.1f)),
		.minSize    = Size::Height(Dimension::Pc(0.1f)),
		.eDirection = EFlexDirection::RowReverse,
	});
	auto& accuracyStyle = world.GetComponent<Style>(
		Widgets::SpawnText(world, hTop, {1.f, 1.f, 1.f, 1.f}, appearance.Font,
						   "Accuracy", AccuracyText{}));
	accuracyStyle.maxSize.width = Dimension::Pc(0.5f);

	// Center vbox
	auto hCenter = Widgets::UINode(world, "Center", Style {
		.colour     = {0.f, 1.f, 0.f, 0.f},
		.size       = Size::Height(Dimension::Pc(0.8f)),
		.minSize    = Size::Height(Dimension::Pc(0.8f)),
		.eDirection = EFlexDirection::Row,
		});

	//Bottom hbox
	auto hBottom = Widgets::UINode(world, "Bottom", Style {
		.colour     = {0.f, 0.f, 1.f, 0.f},
		.size       = Size::Height(Dimension::Pc(0.1f)),
		.minSize    = Size::Height(Dimension::Pc(0.1f)),
		.eDirection = EFlexDirection::Row,
	});
	Widgets::SpawnText(world, hBottom, {1.f, 1.f, 1.f, 1.f}, appearance.Font, "Combo", ComboText{});

	world.AddChild(hRoot, hTop);
	world.AddChild(hRoot, hCenter);
	world.AddChild(hRoot, hBottom);
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
		;
}
} // namespace OSU::UI