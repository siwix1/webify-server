#pragma once

#define NOMINMAX
#include <windows.h>
#include <bugcodes.h>
#include <wudfwdm.h>
#include <wdf.h>
#include <iddcx.h>

#include <dxgi1_5.h>
#include <d3d11_2.h>
#include <avrt.h>
#include <wrl.h>

#include <memory>
#include <vector>
#include <mutex>
#include <string>

// WPP tracing disabled for CMake build
// #include "Trace.h"

namespace Microsoft
{
    namespace WRL
    {
        namespace Wrappers
        {
            typedef HandleT<HandleTraits::HANDLENullTraits> Thread;
        }
    }
}

namespace Webify
{
    // Shared memory header for frame exchange between driver and webify-server
    struct SharedFrameHeader {
        UINT32 magic;           // 'WBFY'
        UINT32 width;
        UINT32 height;
        UINT32 stride;
        UINT64 frame_number;
        UINT32 ready;           // 1 = new frame available
        UINT32 reserved;
        // Pixel data follows immediately after header (BGRA32)
    };

    static constexpr UINT32 SHARED_MAGIC = 0x59464257; // 'WBFY'
    static constexpr size_t MAX_WIDTH = 1920;
    static constexpr size_t MAX_HEIGHT = 1080;
    static constexpr size_t SHARED_MEM_SIZE = sizeof(SharedFrameHeader) + (MAX_WIDTH * MAX_HEIGHT * 4);

    struct Direct3DDevice
    {
        Direct3DDevice(LUID AdapterLuid);
        Direct3DDevice();
        HRESULT Init();

        LUID AdapterLuid;
        Microsoft::WRL::ComPtr<IDXGIFactory5> DxgiFactory;
        Microsoft::WRL::ComPtr<IDXGIAdapter1> Adapter;
        Microsoft::WRL::ComPtr<ID3D11Device> Device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> DeviceContext;
    };

    class SwapChainProcessor
    {
    public:
        SwapChainProcessor(IDDCX_SWAPCHAIN hSwapChain, std::shared_ptr<Direct3DDevice> Device,
                           HANDLE NewFrameEvent, UINT MonitorIndex);
        ~SwapChainProcessor();

    private:
        static DWORD CALLBACK RunThread(LPVOID Argument);
        void Run();
        void RunCore();

        IDDCX_SWAPCHAIN m_hSwapChain;
        std::shared_ptr<Direct3DDevice> m_Device;
        HANDLE m_hAvailableBufferEvent;
        Microsoft::WRL::Wrappers::Thread m_hThread;
        Microsoft::WRL::Wrappers::Event m_hTerminateEvent;

        // Shared memory for this monitor's frames
        UINT m_MonitorIndex;
        HANDLE m_hSharedMem;
        SharedFrameHeader* m_pSharedMem;

        // Staging texture for GPU -> CPU copy
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_StagingTexture;
    };

    class IndirectDeviceContext
    {
    public:
        IndirectDeviceContext(_In_ WDFDEVICE WdfDevice);
        virtual ~IndirectDeviceContext();

        void InitAdapter();
        void FinishInit(UINT ConnectorIndex);

    protected:
        WDFDEVICE m_WdfDevice;
        IDDCX_ADAPTER m_Adapter;
    };

    class IndirectMonitorContext
    {
    public:
        IndirectMonitorContext(_In_ IDDCX_MONITOR Monitor, UINT MonitorIndex);
        virtual ~IndirectMonitorContext();

        void AssignSwapChain(IDDCX_SWAPCHAIN SwapChain, LUID RenderAdapter, HANDLE NewFrameEvent);
        void UnassignSwapChain();

    private:
        IDDCX_MONITOR m_Monitor;
        UINT m_MonitorIndex;
        std::unique_ptr<SwapChainProcessor> m_ProcessingThread;
    };
}
