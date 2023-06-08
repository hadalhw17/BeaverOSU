#pragma once

#include <RavenApp/RavenApp.hpp>
#include <RavenUI/RavenUI.hpp>
#include "RavenOSU.hpp"

namespace OSU::UI {
//! General style variables of the game
struct Appearance {
	Raven::Handle<Raven::Font> Font{};
};
namespace Widgets {
	static Raven::TEntity SpawnButton(Raven::CWorld& world, Raven::TEntity hRoot,
							   SColourF                          colour,
							   const Raven::Handle<Raven::Font>& hFont,
							   std::string_view                  text) {
	using namespace Raven::UI;
	auto hBtn = world.CreateChild(hRoot);
	world.AddComponent<STransformComponent>(hBtn);
	world.AddComponent<Button>(hBtn);
	world.AddComponent<Style>(hBtn, Style {
		.colour = colour,
		.size = {.width = Dimension::Auto(), .height = Dimension::Px(100.f)},
		.maxSize = {.width = Dimension::Auto(), .height = Dimension::Px(100.f)},
		.eAlignItems = EAlignItems::Center,
	});

	auto hText = world.CreateChild(hBtn);
	world.AddComponent<STransformComponent>(hText);
	world.AddComponent<Style>(
		hText, Style{
				   .colour   = {0.f, 0.f, 0.f, 1.f},
				   .position = {.Left = Dimension::Percent(0.2f)},
				   .size     = Size::Auto(),
			   });
	world.AddComponent<Text>(hText,
							 Text{.text = std::string{text}, .Font = hFont});

	return hBtn;
}

static Raven::TEntity SpawnText(Raven::CWorld& world, Raven::TEntity hRoot,
						 SColourF                          colour,
						 const Raven::Handle<Raven::Font>& hFont,
						 std::string_view                  text) {
	using namespace Raven::UI;
	auto hText = world.CreateChild(hRoot);
	world.AddComponent<STransformComponent>(hText);
	world.AddComponent<Style>(
		hText, Style{
				   .colour   = colour,
				   .size     = Size::Auto(),
			   });
	world.AddComponent<Text>(hText,
							 Text{.text = std::string{text}, .Font = hFont});
	return hText;
}

template <typename... T>
Raven::TEntity SpawnButton(Raven::CWorld& world, Raven::TEntity hRoot,
						   SColourF                          colour,
						   const Raven::Handle<Raven::Font>& hFont,
						   std::string_view text, T&&... components) {
	auto hBtn = SpawnButton(world, hRoot, colour, hFont, text);
	(world.AddComponent<T>(hBtn, std::forward<T>(components)), ...);
	return hBtn;
}

template <typename... T>
Raven::TEntity SpawnText(Raven::CWorld& world, Raven::TEntity hRoot,
						 SColourF                          colour,
						 const Raven::Handle<Raven::Font>& hFont,
						 std::string_view text, T&&... components) {
	auto hBtn = SpawnText(world, hRoot, colour, hFont, text);
	(world.AddComponent<T>(hBtn, std::forward<T>(components)), ...);
	return hBtn;
}

static Raven::TEntity UINode(Raven::CWorld& world, const char* name,
				 Raven::UI::Style&& style) {
	auto hNode = world.CreateEntity(name);
	world.AddComponent<STransformComponent>(hNode);
	world.AddComponent<Raven::Tags::NoSerialise>(hNode);
	world.AddComponent<Raven::Tags::NoCopy>(hNode);
	world.AddComponent<Raven::UI::Style>(hNode, std::move(style));
	return hNode;
}
} // namespace Widgets

static Raven::Handle<Raven::CImage>
LoadBackgroundImage(Raven::App& app, Raven::SAssetManager& mgr,
					const CBeatmap& beatmap) {
	if (!beatmap.GetBackground().empty()) {
		auto bg = mgr.Load(app, beatmap.GetBackground());
		if (bg.IsSuccess()) {
			return bg.OnSuccess().Typed<Raven::CImage>();
		} else {
			RavenLogWarning("Failed to load bg: {}", bg.OnFailed());
		}
	}
	return Raven::Handle<Raven::CImage>{};
}
} // namespace OSU::UI