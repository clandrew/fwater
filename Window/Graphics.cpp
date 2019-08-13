#include "pch.h"
#include "Graphics.h"

void VerifyHR(HRESULT hr)
{
	assert(SUCCEEDED(hr));
}

ComPtr<IDXGIAdapter1> GetHardwareAdapter(IDXGIFactory2* pFactory)
{
	ComPtr<IDXGIAdapter1> adapter;

	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &adapter); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the
		// actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

	return adapter;
}

void Graphics::Initialize(HWND hwnd)
{
	m_hwnd = hwnd;

	UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
	// Enable the debug layer (requires the Graphics Tools "optional feature").
	// NOTE: Enabling the debug layer after device creation will invalidate the active device.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();

			// Enable additional debug layers.
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	ComPtr<IDXGIFactory4> dxgiFactory;
	VerifyHR(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	ComPtr<IDXGIAdapter> adapter = GetHardwareAdapter(dxgiFactory.Get());
	VerifyHR(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&m_device)));

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	VerifyHR(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
	
	// Describe and create the swap chain.
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = sc_frameCount;
	swapChainDesc.Width = 500;
	swapChainDesc.Height = 250;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGISwapChain1> swapChain;
	VerifyHR(dxgiFactory->CreateSwapChainForHwnd(
		m_commandQueue.Get(), 
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain));
	VerifyHR(swapChain.As(&m_swapChain));

	VerifyHR(dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

	m_currentFrame = m_swapChain->GetCurrentBackBufferIndex();

	// Create descriptor heaps.
	{
		// Describe and create a render target view (RTV) descriptor heap.
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = sc_frameCount;
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		VerifyHR(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

		m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	// Create frame resources.
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

		// Create a RTV for each frame.
		for (UINT n = 0; n < sc_frameCount; n++)
		{
			VerifyHR(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, m_rtvDescriptorSize);
		}

		// Create copyable footprint
		D3D12_RESOURCE_DESC targetDesc0 = m_renderTargets[0]->GetDesc();
#if defined(_DEBUG)
		// Shouldn't matter which backbuffer we use
		for (int i = 1; i < sc_frameCount; ++i)
		{
			D3D12_RESOURCE_DESC targetDescN = m_renderTargets[i]->GetDesc();
			assert(targetDesc0 == targetDescN);
		}
#endif

		m_device->GetCopyableFootprints(&targetDesc0, 0, 1, 0, &m_renderTargetCopyableFootprint, nullptr, nullptr, nullptr);
	}
	{
		// m_lockable
		auto const& requiredFootprint = m_renderTargetCopyableFootprint.Footprint;
		CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(requiredFootprint.RowPitch * requiredFootprint.Height);

		VerifyHR(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), 
			D3D12_HEAP_FLAG_NONE, 
			&desc, 
			D3D12_RESOURCE_STATE_GENERIC_READ, // includes COPY_SOURCE
			nullptr, 
			IID_PPV_ARGS(&m_lockable)));

		VerifyHR(m_lockable->Map(0, nullptr, reinterpret_cast<void**>(&m_lockedData)));

		HMODULE module = LoadLibrary(L"Compute.dll");
		if (module)
		{
			// Load entrypoints
			m_pfnInitialize = (InitializeFn)GetProcAddress(module, "INITIALIZE");
			m_pfnRedraw = (RedrawFn)GetProcAddress(module, "REDRAW");
			m_pfnUpdate = (UpdateFn)GetProcAddress(module, "UPDATE");
			m_pfnMouseEvent = (OnMouseEventFn)GetProcAddress(module, "ONMOUSEEVENT");
		}

		if (m_pfnInitialize)
		{
			m_pfnInitialize();
		}

		RefreshWindowSize();
	}

	VerifyHR(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

	// Create the command list.
	VerifyHR(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));
	
	m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(500), static_cast<float>(250));
    m_scissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(500), static_cast<LONG>(250));

	VerifyHR(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

	// Create an event handle to use for frame synchronization.
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_fenceEvent == nullptr)
	{
		VerifyHR(HRESULT_FROM_WIN32(GetLastError()));
	}
}

void Graphics::Draw()
{
	if (m_pfnRedraw)
	{
		m_pfnRedraw(reinterpret_cast<int*>(m_lockedData));
	}

	// Set necessary state.
	m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	m_commandList->RSSetViewports(1, &m_viewport);
	m_commandList->RSSetScissorRects(1, &m_scissorRect);
	   
	// Indicate that the back buffer will be used as a render target.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_currentFrame].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_currentFrame, m_rtvDescriptorSize);
	m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_currentFrame].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST));

	D3D12_TEXTURE_COPY_LOCATION dst{};
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst.pResource = m_renderTargets[m_currentFrame].Get();

	D3D12_TEXTURE_COPY_LOCATION src{};
	src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src.pResource = m_lockable.Get();
	src.PlacedFootprint = m_renderTargetCopyableFootprint;

	m_commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_currentFrame].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Indicate that the back buffer will now be used to present.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_currentFrame].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	VerifyHR(m_commandList->Close());

	UINT64 fenceValue = m_fence->GetCompletedValue();

	// Execute the command list.
	ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// Present the frame.
	VerifyHR(m_swapChain->Present(1, 0));
	
	// Signal and increment the fence value.
	++fenceValue;
	VerifyHR(m_commandQueue->Signal(m_fence.Get(), fenceValue));

	// Wait until the previous frame is finished.
	if (m_fence->GetCompletedValue() < fenceValue)
	{
		VerifyHR(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}

	// Safe to reset allocator and command list now
	VerifyHR(m_commandAllocator->Reset());
	VerifyHR(m_commandList->Reset(m_commandAllocator.Get(), nullptr));

	m_currentFrame = m_swapChain->GetCurrentBackBufferIndex();
}

void Graphics::Update()
{
	if (m_pfnUpdate)
	{
		m_pfnUpdate();
	}
}

void Graphics::OnMouseEvent(int x, int y)
{
	// Do visual scale
	x = static_cast<int>((static_cast<float>(x) / static_cast<float>(m_windowWidth)) * 500.0f) + 1;
	x = max(x, 1);
	x = min(x, 500);

	y = static_cast<int>((static_cast<float>(y) / static_cast<float>(m_windowHeight)) * 250.0f) + 1;
	y = max(y, 1);
	y = min(y, 250);

	if (m_pfnMouseEvent)
	{
		m_pfnMouseEvent(&x, &y);
	}
}

void Graphics::RefreshWindowSize()
{
	RECT rect;
	GetClientRect(m_hwnd, &rect);

	m_windowWidth = rect.right - rect.left;
	m_windowHeight = rect.bottom - rect.top;
}