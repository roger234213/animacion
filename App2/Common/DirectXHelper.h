#pragma once

#include <ppltasks.h>	// Para create_task

namespace DX
{
	inline void ThrowIfFailed(HRESULT hr)
	{
		if (FAILED(hr))
		{
			// Configure un punto de interrupción en esta línea para detectar errores de la API Win32.
			throw Platform::Exception::CreateException(hr);
		}
	}

	// Función que lee desde un archivo binario de forma asincrónica.
	inline Concurrency::task<std::vector<byte>> ReadDataAsync(const std::wstring& filename)
	{
		using namespace Windows::Storage;
		using namespace Concurrency;

		auto folder = Windows::ApplicationModel::Package::Current->InstalledLocation;

		return create_task(folder->GetFileAsync(Platform::StringReference(filename.c_str()))).then([] (StorageFile^ file) 
		{
			return FileIO::ReadBufferAsync(file);
		}).then([] (Streams::IBuffer^ fileBuffer) -> std::vector<byte> 
		{
			std::vector<byte> returnBuffer;
			returnBuffer.resize(fileBuffer->Length);
			Streams::DataReader::FromBuffer(fileBuffer)->ReadBytes(Platform::ArrayReference<byte>(returnBuffer.data(), fileBuffer->Length));
			return returnBuffer;
		});
	}

	// Convierte una longitud expresada en píxeles independientes del dispositivo (PID) en una longitud expresada en píxeles físicos.
	inline float ConvertDipsToPixels(float dips, float dpi)
	{
		static const float dipsPerInch = 96.0f;
		return floorf(dips * dpi / dipsPerInch + 0.5f); // Redondear al entero más próximo.
	}

#if defined(_DEBUG)
	// Comprobar la compatibilidad con la capa del SDK.
	inline bool SdkLayersAvailable()
	{
		HRESULT hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_NULL,       // No es necesario crear un dispositivo de hardware real.
			0,
			D3D11_CREATE_DEVICE_DEBUG,  // Comprobar las capas del SDK.
			nullptr,                    // Para ello sirve cualquier nivel de funcionalidad.
			0,
			D3D11_SDK_VERSION,          // Para aplicaciones de Microsoft Store, siempre debe establecerse en D3D11_SDK_VERSION.
			nullptr,                    // No es necesario mantener la referencia del dispositivo D3D.
			nullptr,                    // No es necesario conocer el nivel de funcionalidad.
			nullptr                     // No es necesario mantener la referencia de contexto del dispositivo D3D.
			);

		return SUCCEEDED(hr);
	}
#endif
}
