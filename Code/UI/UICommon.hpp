#pragma once

#include <RavenApp/RavenApp.hpp>
#include <RavenUI/RavenUI.hpp>
#include "RavenOSU.hpp"

namespace OSU::UI {
//! General style variables of the game
struct Appearance {
	Raven::Handle<Raven::Font> Font{};
	float BGDim       = 1.f;
	float AudioVolume = 1.f;
};

struct ScrollList {
	float ScrollOffset = 0.f;
};
struct Slider {
	float Value = 0.f;
	float From = 0.f;
	float To   = 1.f;
	std::function<void(float)> OnChange;
};
struct DragInteraction {
	float2 From{};
	float2 PrevPos{};
	float2 Delta{};

	[[nodiscard]] static constexpr DragInteraction FromPos(float2 pos) {
		return DragInteraction{
			.From    = pos,
			.PrevPos = pos,
			.Delta   = {0.f, 0.f},
		};
	}
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
				   .size     = Size::Height(10_pc),
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
				 Raven::UI::Style style) {
	auto hNode = world.CreateEntity(name);
	world.AddComponent<STransformComponent>(hNode);
	world.AddComponent<Raven::Tags::NoSerialise>(hNode);
	world.AddComponent<Raven::Tags::NoCopy>(hNode);
	world.AddComponent<Raven::UI::Style>(hNode, std::move(style));
	return hNode;
}

} // namespace Widgets

struct UIBuilder {
	UIBuilder Create(const char* pName, Raven::UI::Style style) {
		auto hEnt = Widgets::UINode(World, "", std::move(style));
		World.AddChild(CurrentEnt, hEnt);
		return UIBuilder{hEnt, World};
	}

	template<typename T>
	UIBuilder& WithChildren(uint32 count, T fn) {
		for(uint32 i = 0; i < count; ++i) {
			auto hEnt = Widgets::UINode(World, "", Raven::UI::Style{});
			World.AddChild(CurrentEnt, hEnt);
			fn(UIBuilder{hEnt, World}, i);
		}
		return *this;
	}

	UIBuilder Text(std::string_view                  text,
				   const Raven::Handle<Raven::Font>& font,
				   const SColourF& colour = SColourF::Black()) {
		return Create("Text",
					  Raven::UI::Style{
						  .colour = colour,
					  })
			.WithText(text, font);
	}

	UIBuilder SliderFloat(std::string_view label, const SColourF& bgColour,
						  const SColourF&                   handleColour,
						  const Raven::Handle<Raven::Font>& font, float defVal,
						  const float min, const float max,
						  std::function<void(float)> fn = {}) {
		using namespace Raven::UI;
		if (!label.empty()) {
			Text(label, font)
				.Style()
				.Size(Size::Height(10_pc))
				.Padding(Rect::Horizontal(1_px))
				.FlexShrink(1.f);
		}

		return Create("SliderBG",
					  Raven::UI::Style{
						  .colour     = bgColour,
						  .border     = Rect::Horizontal(1_px),
						  .flexGrow   = 1.f,
						  .flexShrink = 1.f,
						  .eAlignSelf = EAlignSelf::Stretch,
					  })
			.Create("DragHandle",
					Raven::UI::Style{
						.colour      = handleColour,
						.maxSize     = Size::Width(10_pc),
						.aspectRatio = 0.5_pc,
					})
			.WithComponent(std::tuple<Slider, Raven::UI::Button>{
				Slider{.Value    = defVal,
					   .From     = min,
					   .To       = max,
					   .OnChange = std::move(fn)},
				{}});
	}

	UIBuilder& WithText(std::string_view text, const Raven::Handle<Raven::Font>& font) {
		return WithComponent<Raven::UI::Text>({Raven::UI::Text{
			.text = std::string{text},
			.Font = font,
		}});
	}

	template <typename... T>
	UIBuilder& WithComponent(std::tuple<T...> components = {}) {
		(World.AddComponent<T>(CurrentEnt, std::move(std::get<T>(components))),
		 ...);
		return *this;
	}

	Raven::UI::Style& Style() {
		return World.GetComponent<Raven::UI::Style>(CurrentEnt);
	}

	template <typename CompT, typename T> UIBuilder& Patch(const T& fn) {
		World.GetRegistry().patch<CompT>(CurrentEnt, fn);
	}

	Raven::TEntity CurrentEnt;
	Raven::CWorld& World;
};


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