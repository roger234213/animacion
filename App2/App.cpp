#include "pch.h"
#include "App.h"

#include <ppltasks.h>

using namespace App2;

using namespace concurrency;
using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::Activation;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;

// La función principal solo se utiliza para inicializar nuestra clase IFrameworkView.
[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
	auto direct3DApplicationSource = ref new Direct3DApplicationSource();
	CoreApplication::Run(direct3DApplicationSource);
	return 0;
}

IFrameworkView^ Direct3DApplicationSource::CreateView()
{
	return ref new App();
}

App::App() :
	m_windowClosed(false),
	m_windowVisible(true)
{
}

// El primer método al que se llama cuando se crea IFrameworkView.
void App::Initialize(CoreApplicationView^ applicationView)
{
	// Registrar controladores de eventos del ciclo de vida de la aplicación. Este ejemplo incluye Activated, así que
	// podemos activar CoreWindow e iniciar la representación en la ventana.
	applicationView->Activated +=
		ref new TypedEventHandler<CoreApplicationView^, IActivatedEventArgs^>(this, &App::OnActivated);

	CoreApplication::Suspending +=
		ref new EventHandler<SuspendingEventArgs^>(this, &App::OnSuspending);

	CoreApplication::Resuming +=
		ref new EventHandler<Platform::Object^>(this, &App::OnResuming);

	// En este punto tenemos acceso al dispositivo. 
	// Podemos crear los recursos dependientes del dispositivo.
	m_deviceResources = std::make_shared<DX::DeviceResources>();
}

// Se llama cuando se crea o se vuelve a crear el objeto CoreWindow.
void App::SetWindow(CoreWindow^ window)
{
	window->SizeChanged += 
		ref new TypedEventHandler<CoreWindow^, WindowSizeChangedEventArgs^>(this, &App::OnWindowSizeChanged);

	window->VisibilityChanged +=
		ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &App::OnVisibilityChanged);

	window->Closed += 
		ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &App::OnWindowClosed);

	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	currentDisplayInformation->DpiChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnDpiChanged);

	currentDisplayInformation->OrientationChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnOrientationChanged);

	DisplayInformation::DisplayContentsInvalidated +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &App::OnDisplayContentsInvalidated);

	m_deviceResources->SetWindow(window);
}

// Inicializa los recursos de la escena o carga un estado de aplicación previamente guardado.
void App::Load(Platform::String^ entryPoint)
{
	if (m_main == nullptr)
	{
		m_main = std::unique_ptr<App2Main>(new App2Main(m_deviceResources));
	}
}

// Se llama a este método una vez que se activa la ventana.
void App::Run()
{
	while (!m_windowClosed)
	{
		if (m_windowVisible)
		{
			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

			m_main->Update();

			if (m_main->Render())
			{
				m_deviceResources->Present();
			}
		}
		else
		{
			CoreWindow::GetForCurrentThread()->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
		}
	}
}

// Necesario para IFrameworkView.
// Cuando finalizan los eventos, no se llama a Uninitialize. Se llamará si su clase IFrameworkView
// está desactivada mientras la aplicación está en primer plano.
void App::Uninitialize()
{
}

// Controladores de eventos del ciclo de vida de la aplicación.

void App::OnActivated(CoreApplicationView^ applicationView, IActivatedEventArgs^ args)
{
	// Run() no se iniciará hasta que se active CoreWindow.
	CoreWindow::GetForCurrentThread()->Activate();
}

void App::OnSuspending(Platform::Object^ sender, SuspendingEventArgs^ args)
{
	// Guarde el estado de la aplicación de forma asincrónica tras solicitar un aplazamiento. Cuando se efectúa un aplazamiento,
	// se indica que la aplicación está ocupada realizando operaciones de suspensión. Tenga en cuenta que
	// es posible que los aplazamientos no se realicen de forma indefinida. Transcurridos unos cinco segundos,
	// la aplicación se verá forzada a salir.
	SuspendingDeferral^ deferral = args->SuspendingOperation->GetDeferral();

	create_task([this, deferral]()
	{
        m_deviceResources->Trim();

		// Inserte el código aquí.

		deferral->Complete();
	});
}

void App::OnResuming(Platform::Object^ sender, Platform::Object^ args)
{
	// Restaure cualquier dato o estado que se haya descargado durante la suspensión. De forma predeterminada, los datos y
	// estados persisten al reanudar tras una suspensión. Tenga en cuenta que este evento
	// no se produce si se ha finalizado previamente la aplicación.

	// Inserte el código aquí.
}

// Controladores de eventos de Windows.

void App::OnWindowSizeChanged(CoreWindow^ sender, WindowSizeChangedEventArgs^ args)
{
	m_deviceResources->SetLogicalSize(Size(sender->Bounds.Width, sender->Bounds.Height));
	m_main->CreateWindowSizeDependentResources();
}

void App::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
{
	m_windowVisible = args->Visible;
}

void App::OnWindowClosed(CoreWindow^ sender, CoreWindowEventArgs^ args)
{
	m_windowClosed = true;
}

// Controladores de eventos DisplayInformation.

void App::OnDpiChanged(DisplayInformation^ sender, Object^ args)
{
	// Nota: Puede que el valor que se recupera aquí para LogicalDpi no coincida con el valor PPP real de la aplicación
	// si se va a escalar para dispositivos de alta resolución. Una vez establecido el valor PPP en DeviceResources,
	// use siempre el método GetDpi para recuperarlo.
	// Consulte DeviceResources.cpp para obtener más detalles.
	m_deviceResources->SetDpi(sender->LogicalDpi);
	m_main->CreateWindowSizeDependentResources();
}

void App::OnOrientationChanged(DisplayInformation^ sender, Object^ args)
{
	m_deviceResources->SetCurrentOrientation(sender->CurrentOrientation);
	m_main->CreateWindowSizeDependentResources();
}

void App::OnDisplayContentsInvalidated(DisplayInformation^ sender, Object^ args)
{
	m_deviceResources->ValidateDevice();
}