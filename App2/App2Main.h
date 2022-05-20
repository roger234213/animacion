#pragma once

#include "Common\StepTimer.h"
#include "Common\DeviceResources.h"
#include "Content\Sample3DSceneRenderer.h"
#include "Content\SampleFpsTextRenderer.h"

// Presenta contenido Direct2D y 3D en la pantalla.
namespace App2
{
	class App2Main : public DX::IDeviceNotify
	{
	public:
		App2Main(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		~App2Main();
		void CreateWindowSizeDependentResources();
		void Update();
		bool Render();
		void Nose();

		// IDeviceNotify
		virtual void OnDeviceLost();
		virtual void OnDeviceRestored();

	private:
		// Puntero almacenado en caché para los recursos del dispositivo.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// TODO: Sustituir con sus propios representadores de contenido.
		std::unique_ptr<Sample3DSceneRenderer> m_sceneRenderer;
		std::unique_ptr<SampleFpsTextRenderer> m_fpsTextRenderer;

		// Temporizador de bucle de representación.
		DX::StepTimer m_timer;
	};
}