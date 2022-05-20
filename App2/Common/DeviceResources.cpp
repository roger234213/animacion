#include "pch.h"
#include "DeviceResources.h"
#include "DirectXHelper.h"

using namespace D2D1;
using namespace DirectX;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Controls;
using namespace Platform;

namespace DisplayMetrics
{
	// Las pantallas de alta resolución pueden requerir una gran cantidad de energía de la GPU y de la batería para realizar presentaciones.
	// Por ejemplo, la batería de los teléfonos de alta resolución puede durar muy poco si
	// se intenta presentar los juegos a 60 fotogramas por segundo con plena fidelidad.
	// La decisión de representar con plena fidelidad en todas las plataformas y factores de forma
	// debe ser deliberada.
	static const bool SupportHighResolutions = false;

	// Los umbrales predeterminados que definen una visualización de "alta resolución". Si estos umbrales
	// se sobrepasan y el valor de SupportHighResolutions es false, las dimensiones se escalarán
	// en un 50%.
	static const float DpiThreshold = 192.0f;		// 200% de la visualización de escritorio estándar.
	static const float WidthThreshold = 1920.0f;	// 1080p de ancho.
	static const float HeightThreshold = 1080.0f;	// 1080p de alto.
};

// Constantes usadas para calcular las rotaciones de pantalla
namespace ScreenRotation
{
	// Rotación Z de 0 grados
	static const XMFLOAT4X4 Rotation0(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// Rotación Z de 90 grados
	static const XMFLOAT4X4 Rotation90(
		0.0f, 1.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// Rotación Z de 180 grados
	static const XMFLOAT4X4 Rotation180(
		-1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, -1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);

	// Rotación Z de 270 grados
	static const XMFLOAT4X4 Rotation270(
		0.0f, -1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f
		);
};

// Constructor para DeviceResources.
DX::DeviceResources::DeviceResources() :
	m_screenViewport(),
	m_d3dFeatureLevel(D3D_FEATURE_LEVEL_9_1),
	m_d3dRenderTargetSize(),
	m_outputSize(),
	m_logicalSize(),
	m_nativeOrientation(DisplayOrientations::None),
	m_currentOrientation(DisplayOrientations::None),
	m_dpi(-1.0f),
	m_effectiveDpi(-1.0f),
	m_deviceNotify(nullptr)
{
	CreateDeviceIndependentResources();
	CreateDeviceResources();
}

// Configura los recursos que no dependen del dispositivo Direct3D.
void DX::DeviceResources::CreateDeviceIndependentResources()
{
	// Inicializar recursos de Direct2D.
	D2D1_FACTORY_OPTIONS options;
	ZeroMemory(&options, sizeof(D2D1_FACTORY_OPTIONS));

#if defined(_DEBUG)
	// Si el proyecto es una compilación de depuración, habilite la depuración de Direct2D a través de capas del SDK.
	options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif

	// Inicializar el generador de Direct2D.
	DX::ThrowIfFailed(
		D2D1CreateFactory(
			D2D1_FACTORY_TYPE_SINGLE_THREADED,
			__uuidof(ID2D1Factory3),
			&options,
			&m_d2dFactory
			)
		);

	// Inicializar el generador de DirectWrite.
	DX::ThrowIfFailed(
		DWriteCreateFactory(
			DWRITE_FACTORY_TYPE_SHARED,
			__uuidof(IDWriteFactory3),
			&m_dwriteFactory
			)
		);

	// Inicializar el generador de Windows Imaging Component (WIC).
	DX::ThrowIfFailed(
		CoCreateInstance(
			CLSID_WICImagingFactory2,
			nullptr,
			CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&m_wicFactory)
			)
		);
}

// Configura el dispositivo Direct3D y almacena identificadores en este y en el contexto de dispositivo.
void DX::DeviceResources::CreateDeviceResources() 
{
	// Esta marca agrega compatibilidad con superficies con una ordenación de canales de color diferente a
	// la de la API predeterminada. Se requiere para disponer de compatibilidad con Direct2D.
	UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

#if defined(_DEBUG)
	if (DX::SdkLayersAvailable())
	{
		// Si el proyecto está en una compilación de depuración, habilite la depuración mediante capas del SDK con esta marca.
		creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
	}
#endif

	// Esta matriz define el conjunto de niveles de funcionalidades de hardware DirectX que admitirá esta aplicación.
	// Tenga en cuenta que debe mantenerse la ordenación.
	// No olvide declarar el nivel de funcionalidades mínimo necesario de la aplicación en la
	// descripción. Se presupone que todas las aplicaciones admiten 9.1 a menos que se indique lo contrario.
	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
		D3D_FEATURE_LEVEL_9_3,
		D3D_FEATURE_LEVEL_9_2,
		D3D_FEATURE_LEVEL_9_1
	};

	// Cree el objeto de dispositivo de la API Direct3D 11 y un contexto correspondiente.
	ComPtr<ID3D11Device> device;
	ComPtr<ID3D11DeviceContext> context;

	HRESULT hr = D3D11CreateDevice(
		nullptr,					// Especifique nullptr para usar el adaptador predeterminado.
		D3D_DRIVER_TYPE_HARDWARE,	// Cree un dispositivo con el controlador de gráficos de hardware.
		0,							// Debe ser 0, a menos que el controlador sea D3D_DRIVER_TYPE_SOFTWARE.
		creationFlags,				// Configure las marcas de depuración y compatibilidad con Direct2D.
		featureLevels,				// Lista de niveles de funcionalidades que admite esta aplicación.
		ARRAYSIZE(featureLevels),	// Tamaño de la lista anterior.
		D3D11_SDK_VERSION,			// Para aplicaciones de Microsoft Store, siempre debe establecerse en D3D11_SDK_VERSION.
		&device,					// Devuelve el dispositivo Direct3D creado.
		&m_d3dFeatureLevel,			// Devuelve el nivel de funcionalidad del dispositivo creado.
		&context					// Devuelve el contexto inmediato del dispositivo.
		);

	if (FAILED(hr))
	{
		// Si no se puede inicializar, recurra al dispositivo WARP.
		// Para obtener más información sobre WARP, vea: 
		// https://go.microsoft.com/fwlink/?LinkId=286690
		DX::ThrowIfFailed(
			D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_WARP, // Cree un dispositivo WARP en lugar de un dispositivo de hardware.
				0,
				creationFlags,
				featureLevels,
				ARRAYSIZE(featureLevels),
				D3D11_SDK_VERSION,
				&device,
				&m_d3dFeatureLevel,
				&context
				)
			);
	}

	// Almacene punteros en el dispositivo de la API Direct3D 11.3 y en el contexto inmediato.
	DX::ThrowIfFailed(
		device.As(&m_d3dDevice)
		);

	DX::ThrowIfFailed(
		context.As(&m_d3dContext)
		);

	// Cree el objeto de dispositivo de Direct2D y un contexto correspondiente.
	ComPtr<IDXGIDevice3> dxgiDevice;
	DX::ThrowIfFailed(
		m_d3dDevice.As(&dxgiDevice)
		);

	DX::ThrowIfFailed(
		m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice)
		);

	DX::ThrowIfFailed(
		m_d2dDevice->CreateDeviceContext(
			D2D1_DEVICE_CONTEXT_OPTIONS_NONE,
			&m_d2dContext
			)
		);
}

// Estos recursos se deben volver a crear cada vez que se cambie el tamaño de la ventana.
void DX::DeviceResources::CreateWindowSizeDependentResources() 
{
	// Borrar el contexto específico del tamaño de ventana anterior.
	ID3D11RenderTargetView* nullViews[] = {nullptr};
	m_d3dContext->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
	m_d3dRenderTargetView = nullptr;
	m_d2dContext->SetTarget(nullptr);
	m_d2dTargetBitmap = nullptr;
	m_d3dDepthStencilView = nullptr;
	m_d3dContext->Flush1(D3D11_CONTEXT_TYPE_ALL, nullptr);

	UpdateRenderTargetSize();

	// El ancho y el alto de la cadena de intercambio se debe basar en el
	// ancho y el alto de la ventana en orientación nativa. Si la ventana no tiene orientación
	// nativa, se deberán invertir las dimensiones.
	DXGI_MODE_ROTATION displayRotation = ComputeDisplayRotation();

	bool swapDimensions = displayRotation == DXGI_MODE_ROTATION_ROTATE90 || displayRotation == DXGI_MODE_ROTATION_ROTATE270;
	m_d3dRenderTargetSize.Width = swapDimensions ? m_outputSize.Height : m_outputSize.Width;
	m_d3dRenderTargetSize.Height = swapDimensions ? m_outputSize.Width : m_outputSize.Height;

	if (m_swapChain != nullptr)
	{
		// Si la cadena de intercambio ya existe, cambie el tamaño.
		HRESULT hr = m_swapChain->ResizeBuffers(
			2, // Cadena de intercambio de doble búfer.
			lround(m_d3dRenderTargetSize.Width),
			lround(m_d3dRenderTargetSize.Height),
			DXGI_FORMAT_B8G8R8A8_UNORM,
			0
			);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			// Si el dispositivo se quitó por cualquier motivo, se deberá crear un dispositivo y una cadena de intercambio nuevos.
			HandleDeviceLost();

			// Ya se ha configurado e instalado todo. No continúe la ejecución de este método. HandleDeviceLost volverá a especificar este método 
			// y configurará correctamente el nuevo dispositivo.
			return;
		}
		else
		{
			DX::ThrowIfFailed(hr);
		}
	}
	else
	{
		// De lo contrario, cree una nueva con el mismo adaptador que el del dispositivo Direct3D existente.
		DXGI_SCALING scaling = DisplayMetrics::SupportHighResolutions ? DXGI_SCALING_NONE : DXGI_SCALING_STRETCH;
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};

		swapChainDesc.Width = lround(m_d3dRenderTargetSize.Width);		// Haga coincidir el tamaño de la ventana.
		swapChainDesc.Height = lround(m_d3dRenderTargetSize.Height);
		swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;				// Este es el formato de cadena de intercambio más frecuente.
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1;								// No usar varios ejemplos.
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = 2;									// Use el almacenamiento de doble búfer para minimizar la latencia.
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;	// Todas las aplicaciones de Microsoft Store deben usar este SwapEffect.
		swapChainDesc.Flags = 0;
		swapChainDesc.Scaling = scaling;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

		// Esta secuencia obtiene el generador de DXGI que se usó para crear el dispositivo Direct3D anterior.
		ComPtr<IDXGIDevice3> dxgiDevice;
		DX::ThrowIfFailed(
			m_d3dDevice.As(&dxgiDevice)
			);

		ComPtr<IDXGIAdapter> dxgiAdapter;
		DX::ThrowIfFailed(
			dxgiDevice->GetAdapter(&dxgiAdapter)
			);

		ComPtr<IDXGIFactory4> dxgiFactory;
		DX::ThrowIfFailed(
			dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory))
			);

		ComPtr<IDXGISwapChain1> swapChain;
		DX::ThrowIfFailed(
			dxgiFactory->CreateSwapChainForCoreWindow(
				m_d3dDevice.Get(),
				reinterpret_cast<IUnknown*>(m_window.Get()),
				&swapChainDesc,
				nullptr,
				&swapChain
				)
			);
		DX::ThrowIfFailed(
			swapChain.As(&m_swapChain)
			);

		// Asegúrese de que DXGI no ponga en cola más de un fotograma cada vez. Esto reduce la latencia y
		// garantiza que la aplicación solo se presente tras cada VSync, lo que minimiza el consumo eléctrico.
		DX::ThrowIfFailed(
			dxgiDevice->SetMaximumFrameLatency(1)
			);
	}

	// Establezca la orientación correcta para la cadena de intercambio y genere transformaciones de matriz en 2D y
	// 3D para la representación en la cadena de intercambio girada.
	// Tenga en cuenta que el ángulo de giro de las transformaciones en 2D y en 3D es distinto.
	// Esto se debe a la diferencia en los espacios de coordenadas. Además,
	// la matriz en 3D se especifica explícitamente para evitar errores de redondeo.

	switch (displayRotation)
	{
	case DXGI_MODE_ROTATION_IDENTITY:
		m_orientationTransform2D = Matrix3x2F::Identity();
		m_orientationTransform3D = ScreenRotation::Rotation0;
		break;

	case DXGI_MODE_ROTATION_ROTATE90:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(90.0f) *
			Matrix3x2F::Translation(m_logicalSize.Height, 0.0f);
		m_orientationTransform3D = ScreenRotation::Rotation270;
		break;

	case DXGI_MODE_ROTATION_ROTATE180:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(180.0f) *
			Matrix3x2F::Translation(m_logicalSize.Width, m_logicalSize.Height);
		m_orientationTransform3D = ScreenRotation::Rotation180;
		break;

	case DXGI_MODE_ROTATION_ROTATE270:
		m_orientationTransform2D = 
			Matrix3x2F::Rotation(270.0f) *
			Matrix3x2F::Translation(0.0f, m_logicalSize.Width);
		m_orientationTransform3D = ScreenRotation::Rotation90;
		break;

	default:
		throw ref new FailureException();
	}

	DX::ThrowIfFailed(
		m_swapChain->SetRotation(displayRotation)
		);

	// Cree una nueva vista de destino de presentación del búfer de reserva de la cadena de intercambio.
	ComPtr<ID3D11Texture2D1> backBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))
		);

	DX::ThrowIfFailed(
		m_d3dDevice->CreateRenderTargetView1(
			backBuffer.Get(),
			nullptr,
			&m_d3dRenderTargetView
			)
		);

	// En caso necesario, cree una vista de galería de símbolos de profundidad para su uso con representación en 3D.
	CD3D11_TEXTURE2D_DESC1 depthStencilDesc(
		DXGI_FORMAT_D24_UNORM_S8_UINT, 
		lround(m_d3dRenderTargetSize.Width),
		lround(m_d3dRenderTargetSize.Height),
		1, // Esta vista de galería de símbolos de profundidad solo tiene una textura.
		1, // Use un nivel de asignación de MIP único.
		D3D11_BIND_DEPTH_STENCIL
		);

	ComPtr<ID3D11Texture2D1> depthStencil;
	DX::ThrowIfFailed(
		m_d3dDevice->CreateTexture2D1(
			&depthStencilDesc,
			nullptr,
			&depthStencil
			)
		);

	CD3D11_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc(D3D11_DSV_DIMENSION_TEXTURE2D);
	DX::ThrowIfFailed(
		m_d3dDevice->CreateDepthStencilView(
			depthStencil.Get(),
			&depthStencilViewDesc,
			&m_d3dDepthStencilView
			)
		);
	
	// Establezca la ventanilla de representación en 3D para que afecte a toda la ventana.
	m_screenViewport = CD3D11_VIEWPORT(
		0.0f,
		0.0f,
		m_d3dRenderTargetSize.Width,
		m_d3dRenderTargetSize.Height
		);

	m_d3dContext->RSSetViewports(1, &m_screenViewport);

	// Cree un mapa de bits de destino de Direct2D asociado al
	// búfer de reserva de la cadena de intercambio y establézcalo como el destino actual.
	D2D1_BITMAP_PROPERTIES1 bitmapProperties = 
		D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
			m_dpi,
			m_dpi
			);

	ComPtr<IDXGISurface2> dxgiBackBuffer;
	DX::ThrowIfFailed(
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(&dxgiBackBuffer))
		);

	DX::ThrowIfFailed(
		m_d2dContext->CreateBitmapFromDxgiSurface(
			dxgiBackBuffer.Get(),
			&bitmapProperties,
			&m_d2dTargetBitmap
			)
		);

	m_d2dContext->SetTarget(m_d2dTargetBitmap.Get());
	m_d2dContext->SetDpi(m_effectiveDpi, m_effectiveDpi);

	// Se recomienda el suavizado de contorno de texto en escala de grises para todas las aplicaciones de Microsoft Store.
	m_d2dContext->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
}

// Determina las dimensiones del destino de representación y si se va a reducir.
void DX::DeviceResources::UpdateRenderTargetSize()
{
	m_effectiveDpi = m_dpi;

	// Para prolongar la duración de la batería en los dispositivos de alta resolución, use un destino de representación más pequeño
	// y permita que la GPU escale la salida en la presentación.
	if (!DisplayMetrics::SupportHighResolutions && m_dpi > DisplayMetrics::DpiThreshold)
	{
		float width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_dpi);
		float height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_dpi);

		// Cuando el dispositivo tiene orientación vertical, el alto es superior al ancho. Compare la
		// dimensión más grande con el umbral de ancho y la dimensión más pequeña
		// con el umbral de alto.
		if (max(width, height) > DisplayMetrics::WidthThreshold && min(width, height) > DisplayMetrics::HeightThreshold)
		{
			// Para escalar la aplicación, se cambia el valor PPP efectivo. El tamaño lógico no cambia.
			m_effectiveDpi /= 2.0f;
		}
	}

	// Calcular el tamaño del destino de presentación necesario en píxeles.
	m_outputSize.Width = DX::ConvertDipsToPixels(m_logicalSize.Width, m_effectiveDpi);
	m_outputSize.Height = DX::ConvertDipsToPixels(m_logicalSize.Height, m_effectiveDpi);

	// Impedir la creación de contenido de DirectX de tamaño cero.
	m_outputSize.Width = max(m_outputSize.Width, 1);
	m_outputSize.Height = max(m_outputSize.Height, 1);
}

// Este método se llama cuando se crea (o se vuelve a crear) el objeto CoreWindow.
void DX::DeviceResources::SetWindow(CoreWindow^ window)
{
	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	m_window = window;
	m_logicalSize = Windows::Foundation::Size(window->Bounds.Width, window->Bounds.Height);
	m_nativeOrientation = currentDisplayInformation->NativeOrientation;
	m_currentOrientation = currentDisplayInformation->CurrentOrientation;
	m_dpi = currentDisplayInformation->LogicalDpi;
	m_d2dContext->SetDpi(m_dpi, m_dpi);

	CreateWindowSizeDependentResources();
}

// Este método se llama en el controlador de eventos del evento SizeChanged.
void DX::DeviceResources::SetLogicalSize(Windows::Foundation::Size logicalSize)
{
	if (m_logicalSize != logicalSize)
	{
		m_logicalSize = logicalSize;
		CreateWindowSizeDependentResources();
	}
}

// Este método se llama en el controlador de eventos del evento DpiChanged.
void DX::DeviceResources::SetDpi(float dpi)
{
	if (dpi != m_dpi)
	{
		m_dpi = dpi;

		// Al cambiar el valor PPP de la pantalla, también cambia el tamaño lógico de la ventana (que se mide en PID) y se debe actualizar.
		m_logicalSize = Windows::Foundation::Size(m_window->Bounds.Width, m_window->Bounds.Height);

		m_d2dContext->SetDpi(m_dpi, m_dpi);
		CreateWindowSizeDependentResources();
	}
}

// Este método se llama en el controlador de eventos del evento OrientationChanged.
void DX::DeviceResources::SetCurrentOrientation(DisplayOrientations currentOrientation)
{
	if (m_currentOrientation != currentOrientation)
	{
		m_currentOrientation = currentOrientation;
		CreateWindowSizeDependentResources();
	}
}

// Este método se llama en el controlador de eventos del evento DisplayContentsInvalidated.
void DX::DeviceResources::ValidateDevice()
{
	// El dispositivo D3D deja de ser válido si el adaptador predeterminado cambió desde que el dispositivo
	// se creó o el dispositivo se quitadó.

	// En primer lugar, obtenga la información del adaptador predeterminado desde que se creó el dispositivo.

	ComPtr<IDXGIDevice3> dxgiDevice;
	DX::ThrowIfFailed(m_d3dDevice.As(&dxgiDevice));

	ComPtr<IDXGIAdapter> deviceAdapter;
	DX::ThrowIfFailed(dxgiDevice->GetAdapter(&deviceAdapter));

	ComPtr<IDXGIFactory4> deviceFactory;
	DX::ThrowIfFailed(deviceAdapter->GetParent(IID_PPV_ARGS(&deviceFactory)));

	ComPtr<IDXGIAdapter1> previousDefaultAdapter;
	DX::ThrowIfFailed(deviceFactory->EnumAdapters1(0, &previousDefaultAdapter));

	DXGI_ADAPTER_DESC1 previousDesc;
	DX::ThrowIfFailed(previousDefaultAdapter->GetDesc1(&previousDesc));

	// A continuación, obtenga la información para el adaptador predeterminado actual.

	ComPtr<IDXGIFactory4> currentFactory;
	DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&currentFactory)));

	ComPtr<IDXGIAdapter1> currentDefaultAdapter;
	DX::ThrowIfFailed(currentFactory->EnumAdapters1(0, &currentDefaultAdapter));

	DXGI_ADAPTER_DESC1 currentDesc;
	DX::ThrowIfFailed(currentDefaultAdapter->GetDesc1(&currentDesc));

	// Si los identificadores únicos locales (LUID) del adaptador no coinciden o el dispositivo notifica que se ha quitado,
	// se debe crear un nuevo dispositivo D3D.

	if (previousDesc.AdapterLuid.LowPart != currentDesc.AdapterLuid.LowPart ||
		previousDesc.AdapterLuid.HighPart != currentDesc.AdapterLuid.HighPart ||
		FAILED(m_d3dDevice->GetDeviceRemovedReason()))
	{
		// Libere referencias a los recursos relacionados con el dispositivo anterior.
		dxgiDevice = nullptr;
		deviceAdapter = nullptr;
		deviceFactory = nullptr;
		previousDefaultAdapter = nullptr;

		// Cree un dispositivo y una cadena de intercambio nuevos.
		HandleDeviceLost();
	}
}

// Vuelva a crear todos los recursos de dispositivo y vuélvalos a establecer en el estado actual.
void DX::DeviceResources::HandleDeviceLost()
{
	m_swapChain = nullptr;

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceLost();
	}

	CreateDeviceResources();
	m_d2dContext->SetDpi(m_dpi, m_dpi);
	CreateWindowSizeDependentResources();

	if (m_deviceNotify != nullptr)
	{
		m_deviceNotify->OnDeviceRestored();
	}
}

// Registre DeviceNotify para que se notifique la pérdida y creación del dispositivo.
void DX::DeviceResources::RegisterDeviceNotify(DX::IDeviceNotify* deviceNotify)
{
	m_deviceNotify = deviceNotify;
}

// Llame a este método cuando la aplicación se suspenda. Indica al controlador que la aplicación 
// está entrando en estado de inactividad y que otras aplicaciones pueden reclamar el uso de los búferes temporales.
void DX::DeviceResources::Trim()
{
	ComPtr<IDXGIDevice3> dxgiDevice;
	m_d3dDevice.As(&dxgiDevice);

	dxgiDevice->Trim();
}

// Presente el contenido de la cadena de intercambio en la pantalla.
void DX::DeviceResources::Present() 
{
	// El primer argumento indica a DXGI que se bloquee hasta VSync, lo que pone a la aplicación
	// en suspensión hasta el siguiente VSync. Esto asegura que no se desperdician ciclos presentando
	// fotogramas que no se mostrarán nunca en la pantalla.
	DXGI_PRESENT_PARAMETERS parameters = { 0 };
	HRESULT hr = m_swapChain->Present1(1, 0, &parameters);

	// Descartar el contenido del destino de presentación.
	// Esta es solo una operación válida cuando el contenido existente se va a sobrescribir
	// totalmente. Si se utilizan rectángulos de modificación o desplazamiento, se deberá quitar esta llamada.
	m_d3dContext->DiscardView1(m_d3dRenderTargetView.Get(), nullptr, 0);

	// Descarte el contenido de la galería de símbolos de profundidad.
	m_d3dContext->DiscardView1(m_d3dDepthStencilView.Get(), nullptr, 0);

	// Si el dispositivo se quitó por una desconexión o una actualización del controlador, 
	// debe volver a crear todos los recursos de dispositivo.
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		HandleDeviceLost();
	}
	else
	{
		DX::ThrowIfFailed(hr);
	}
}

// Este método determina la rotación entre la orientación nativa del dispositivo de pantalla y
// la orientación de pantalla actual.
DXGI_MODE_ROTATION DX::DeviceResources::ComputeDisplayRotation()
{
	DXGI_MODE_ROTATION rotation = DXGI_MODE_ROTATION_UNSPECIFIED;

	// Nota: NativeOrientation solo puede tener los valores Landscape o Portrait aunque
	// la enumeración DisplayOrientations tiene otros valores.
	switch (m_nativeOrientation)
	{
	case DisplayOrientations::Landscape:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;
		}
		break;

	case DisplayOrientations::Portrait:
		switch (m_currentOrientation)
		{
		case DisplayOrientations::Landscape:
			rotation = DXGI_MODE_ROTATION_ROTATE90;
			break;

		case DisplayOrientations::Portrait:
			rotation = DXGI_MODE_ROTATION_IDENTITY;
			break;

		case DisplayOrientations::LandscapeFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE270;
			break;

		case DisplayOrientations::PortraitFlipped:
			rotation = DXGI_MODE_ROTATION_ROTATE180;
			break;
		}
		break;
	}
	return rotation;
}