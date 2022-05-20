#include "pch.h"
#include "App2Main.h"
#include "Common\DirectXHelper.h"
#include <iostream>

using namespace App2;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;

// Carga e inicializa los activos de la aplicación cuando se carga la aplicación.
App2Main::App2Main(const std::shared_ptr<DX::DeviceResources>& deviceResources) :
	m_deviceResources(deviceResources)
{
	// Registrarse para recibir notificación si el dispositivo se pierde o se vuelve a crear
	m_deviceResources->RegisterDeviceNotify(this);

	// TODO: Reemplácelo por la inicialización del contenido de su aplicación.
	m_sceneRenderer = std::unique_ptr<Sample3DSceneRenderer>(new Sample3DSceneRenderer(m_deviceResources));

	m_fpsTextRenderer = std::unique_ptr<SampleFpsTextRenderer>(new SampleFpsTextRenderer(m_deviceResources));

	// TODO: Cambie la configuración del temporizador si desea usar un modo distinto al modo de timestep variable predeterminado.
	// p. ej. para una lógica de actualización de timestep fijo de 60 FPS, llame a:
	/*
	m_timer.SetFixedTimeStep(true);
	m_timer.SetTargetElapsedSeconds(1.0 / 60);
	*/
}

App2Main::~App2Main()
{
	// Anular el registro de notificación del dispositivo
	m_deviceResources->RegisterDeviceNotify(nullptr);
}

// Actualiza el estado de la aplicación cuando cambia el tamaño de la ventana (p. ej., un cambio de orientación del dispositivo)
void App2Main::CreateWindowSizeDependentResources() 
{
	// TODO: Reemplácelo por la inicialización dependiente del tamaño del contenido de su aplicación.
	m_sceneRenderer->CreateWindowSizeDependentResources();
}

// Actualiza el estado de la aplicación una vez por marco.
void App2Main::Update() 
{
	// Actualizar los objetos de la escena.
	m_timer.Tick([&]()
	{
		// TODO: Reemplácelo por las funciones de actualización de contenido de su aplicación.
		m_sceneRenderer->Update(m_timer);
		m_fpsTextRenderer->Update(m_timer);
	});
}

// Presenta el marco actual de acuerdo con el estado actual de la aplicación.
// Devuelve true si se ha presentado el marco y está listo para ser mostrado.
bool App2Main::Render() 
{
	// No intente presentar nada antes de la primera actualización.
	if (m_timer.GetFrameCount() == 0)
	{
		return false;
	}

	auto context = m_deviceResources->GetD3DDeviceContext();

	// Restablecer la ventanilla para que afecte a toda la pantalla.
	auto viewport = m_deviceResources->GetScreenViewport();
	context->RSSetViewports(1, &viewport);

	// Restablecer los valores de destino de presentación en la pantalla.
	ID3D11RenderTargetView *const targets[1] = { m_deviceResources->GetBackBufferRenderTargetView() };
	context->OMSetRenderTargets(1, targets, m_deviceResources->GetDepthStencilView());

	// Borrar el búfer de reserva y la vista de galería de símbolos de profundidad.
	context->ClearRenderTargetView(m_deviceResources->GetBackBufferRenderTargetView(), DirectX::Colors::CornflowerBlue);
	context->ClearDepthStencilView(m_deviceResources->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// Presentar los objetos de la escena.
	// TODO: Reemplácelo por las funciones de representación de contenido de su aplicación.
	m_sceneRenderer->Render();
	m_fpsTextRenderer->Render();

	return true;
}

// Notifica a los representadores que deben liberarse recursos del dispositivo.
void App2Main::OnDeviceLost()
{
	m_sceneRenderer->ReleaseDeviceDependentResources();
	m_fpsTextRenderer->ReleaseDeviceDependentResources();
}

// Notifica a los representadores que los recursos del dispositivo pueden volver a crearse.
void App2Main::OnDeviceRestored()
{
	m_sceneRenderer->CreateDeviceDependentResources();
	m_fpsTextRenderer->CreateDeviceDependentResources();
	CreateWindowSizeDependentResources();
}


void App2Main::Nose() {
	std::cout << "hola";
}