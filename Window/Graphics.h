#pragma once

using namespace Microsoft::WRL;

class Graphics
{
	HWND m_hwnd;
	LONG m_windowWidth;
	LONG m_windowHeight;

	ComPtr<ID3D12Device> m_device;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;

	static const int sc_frameCount = 2;
	int m_currentFrame;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;

	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	UINT m_rtvDescriptorSize;
	ComPtr<ID3D12Resource> m_renderTargets[sc_frameCount];
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT m_renderTargetCopyableFootprint;

	ComPtr<ID3D12Resource> m_lockable;
	byte* m_lockedData;

	ComPtr<ID3D12Fence> m_fence;
	HANDLE m_fenceEvent;

	typedef void(*InitializeFn)();
	typedef void(*RedrawFn)(int*);
	typedef void(*UpdateFn)();
	typedef void(*OnMouseEventFn)(int*, int*);

	InitializeFn m_pfnInitialize;
	RedrawFn m_pfnRedraw;
	UpdateFn m_pfnUpdate;
	OnMouseEventFn m_pfnMouseEvent;

public:
	void Initialize(HWND hwnd);
	void Draw();
	void Update();
	void OnMouseEvent(int x, int y);
	void RefreshWindowSize();
};