#include "UICommon.hpp"
#include <Timer.h>
#include <DefaultComponents.hpp>
#include <RenderSystem.hpp>
#include <RavenWindow/Window.hpp>

namespace OSU::UI {
using namespace Raven;
using namespace Raven::UI;

struct SplashTimer {
	CTimer Timer{false};
	int32 CurrentItem = -1;
};

struct SplashItem {
	std::string ImagePath;
	float Duration = 1.f;
	float FadeInDuration = 2.f;
	float FadeOutDuration = 2.f;
};

struct SplashSequence {
	std::vector<SplashItem> ItemSequence{};
	Window::Id              WindowId{};
	Handle<CWorld>          World{};
};

template <typename T> Raven::SystemDesc EnterSplashSystem(T&& sys) {
	return Raven::SystemDesc{std::forward<T>(sys)}.WithCondition(
		Raven::State<EGameState>::OnEnter(EGameState::SplashScreen));
}

template <typename T> Raven::SystemDesc SplashSystem(T&& sys) {
	return Raven::SystemDesc{std::forward<T>(sys)}.WithCondition(
		Raven::State<EGameState>::OnUpdate(EGameState::SplashScreen));
}

template <typename T> Raven::SystemDesc LeaveSplashSystem(T&& sys) {
	return Raven::SystemDesc{std::forward<T>(sys)}.WithCondition(
		Raven::State<EGameState>::OnExit(EGameState::SplashScreen));
}

std::vector<SplashItem> CreateSplashSequence() {
	return {SplashItem{
		.ImagePath = "project://Assets/Textures/SplashSeq/beaver.png",
		.Duration  = 2.f,
		.FadeInDuration = 3.f,
		.FadeOutDuration = 3.f,
	}, SplashItem{
		.ImagePath = "project://Assets/Textures/SplashSeq/RavenLogo.jpg",
		.Duration  = 3.f,
		.FadeInDuration = 3.f,
		.FadeOutDuration = 3.f,
	}};
}

void CreateSplashWindow(App& app, Assets<CWorld>& worlds,
						Window::Windows&          windows,
						const Window::MainWindow& mainWindow,
						const Window::Monitors&   monitors) {
		auto hGameWorld = worlds.Create();

		auto* pWorld = worlds.GetMut(hGameWorld);
		pWorld->SetSimulationEnabled(true);
		pWorld->SetIsPaused(false);
		auto hCam = pWorld->CreateEntity("SplashCamera");
		auto& cam = pWorld->AddComponent<SCameraComponent>(hCam);
		cam.isActive = true;
		pWorld->AddComponent<STransformComponent>(hCam).m_translation.y = 10.f;
		pWorld->AddOrReplace<SRenderInfo>(hCam);
		auto hWnd = windows.Create(
			"Splash window", int2{512, 512}, Window::EPresentMode::NoVSync,
			Window::EInitConfig::NoDecorations | Window::EInitConfig::NoInputs);

		//const auto hWnd = mainWindow.m_hId; 

		auto* pWindow = windows.Get(hWnd);
		windows.Get(mainWindow)->SetRequestedDimensions(int2{512, 512});
		pWindow->Position(monitors.m_monitors[0].m_workAreaSize / 2 -
						  pWindow->RequestedDimensions() / 2);

		windows.Get(mainWindow)->SetIsMinimised(true);

		pWorld->AddComponent<Raven::RenderTargetT>(hCam, hWnd);

		app.CreateResource<SplashSequence>(SplashSequence{
			.ItemSequence = CreateSplashSequence(),
			.WindowId     = hWnd,
			.World        = hGameWorld,
		});

		UIBuilder{Widgets::UINode(*pWorld, "SplashRoot",
								  Style().Size(Size::All(100_pc))),
				  *pWorld}
			.WithComponent<Root, Image, SplashTimer>();
}

void UpdateSplash(App& app, SAssetManager& mgr, const SplashSequence& seq,
				  const Query<With<SplashTimer, Image>>& timers, State<EGameState>& stateMachine) {
	auto enqueue = [&](SplashTimer& timer, Image& image, int32 imageIdx) {
		timer.Timer.Start();
		timer.CurrentItem = imageIdx;
		image.hImg = mgr.Load(app, seq.ItemSequence[timer.CurrentItem].ImagePath)
						 .OnSuccess()
						 .Typed<CImage>();
	};
	timers.each([&](SplashTimer& timer, Image& image) {
		int32 toEnqueue = -1;
		if(timer.CurrentItem == -1) {
			toEnqueue = 0;
		}
		const auto& currentItem = seq.ItemSequence[timer.CurrentItem];
		if(timer.Timer.GetEllapsedTime() >= currentItem.Duration + currentItem.FadeInDuration + currentItem.FadeOutDuration) {
			const auto next = timer.CurrentItem + 1;
			if(next >= seq.ItemSequence.size()) {

				stateMachine.Set(EGameState::Menu);
			} else {
				toEnqueue = next;
			}

		}
		if(toEnqueue >= 0) {
			enqueue(timer, image, toEnqueue);
		}

	});
}

void UpdateFade(const Query<With<SplashTimer, Style>>& timers,
				const SplashSequence&                  seq) {
	timers.each([&](SplashTimer& timer, Style& style) {
		auto t = static_cast<float>(timer.Timer.GetEllapsedTime());
		const auto& item = seq.ItemSequence[timer.CurrentItem];
		if(t <= item.FadeInDuration) {
			t = t / item.FadeInDuration;
		} else if(t <= item.FadeInDuration + item.Duration) {
			t = (t - item.FadeInDuration) / item.Duration;
			t = 1.f;
		} else {
			t = 1.f - (t - item.FadeInDuration - item.Duration) / item.FadeOutDuration;
		}
		style.colour = SColourF::Grey(t, 1.f);
	});
}

void ScaleWindow(const Query<With<SplashTimer>>& timers,
				 const SplashSequence& seq, Window::Windows& windows, const Window::Monitors& monitors) {
	timers.each([&](SplashTimer& timer) {

		auto t = static_cast<float>(timer.Timer.GetEllapsedTime());
		const auto& item = seq.ItemSequence[timer.CurrentItem];
		if(t <= item.FadeInDuration) {
		} else if(t <= item.FadeInDuration + item.Duration) {
		} else {
			t = 1.f - (t - item.FadeInDuration - item.Duration) / item.FadeOutDuration;
			if(timer.CurrentItem == seq.ItemSequence.size() - 1) {
				auto* pWindow = windows.Get(seq.WindowId);
				pWindow->SetRequestedDimensions(
					int2(glm::floor(float2{512, 512} * t)));
				pWindow->Position(
					glm::floor(float2(monitors.m_monitors[0].m_workAreaSize -
									  pWindow->RequestedDimensions()) /
							   2.f));
			}
		}
		});
}

void ReleaseSplashSequence(App& app, Assets<CWorld>& worlds, SplashSequence& seq, Window::Windows& windows, const Window::MainWindow& mainWindow) {
	if(seq.WindowId != mainWindow.m_hId) {
		windows.Release(seq.WindowId);
		windows.Get(mainWindow)->SetIsMinimised(false);
	}

	worlds.GetMut(seq.World)->SetIsPaused(true);
	app.RemoveResource<SplashSequence>();
	windows.Get(mainWindow)->SetRequestedDimensions(int2{-1, -1});
	windows.Get(mainWindow)->Position(int2{0});
}

void BuildSplashScreen(App& app) {
	app
		.CreateResource<State<EGameState>>()
		.AddSystem(StateStage, EnterSplashSystem(&CreateSplashWindow))
		.AddSystem(StateStage, LeaveSplashSystem(&ReleaseSplashSequence)
								   .Flags(ESystemFlags::ForceSingleThreaded))
		.AddSystem(StateStage, SplashSystem(&UpdateSplash))
		.AddSystem(StateStage, SplashSystem(&UpdateFade))
		.AddSystem(StateStage, SplashSystem(&ScaleWindow))
		.AddComponent<SplashTimer>()
		;
	app.GetResource<State<EGameState>>()->Set(EGameState::SplashScreen);
}

} // namespace OSU::UI