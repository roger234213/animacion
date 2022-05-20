#pragma once

#include <string>
#include "..\Common\DeviceResources.h"
#include "..\Common\StepTimer.h"

namespace App2
{
	// Presenta el valor de FPS actual en la esquina inferior derecha de la pantalla usando Direct2D y DirectWrite.
	class SampleFpsTextRenderer
	{
	public:
		SampleFpsTextRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources);
		void CreateDeviceDependentResources();
		void ReleaseDeviceDependentResources();
		void Update(DX::StepTimer const& timer);
		void Render();

	private:
		// Puntero almacenado en caché para los recursos del dispositivo.
		std::shared_ptr<DX::DeviceResources> m_deviceResources;

		// Recursos relacionados con la representación del texto.
		std::wstring                                    m_text;
		DWRITE_TEXT_METRICS	                            m_textMetrics;
		Microsoft::WRL::ComPtr<ID2D1SolidColorBrush>    m_whiteBrush;
		Microsoft::WRL::ComPtr<ID2D1DrawingStateBlock1> m_stateBlock;
		Microsoft::WRL::ComPtr<IDWriteTextLayout3>      m_textLayout;
		Microsoft::WRL::ComPtr<IDWriteTextFormat2>      m_textFormat;
	};
}