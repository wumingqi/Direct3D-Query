#pragma once

namespace Query {
	namespace D3D12Query
	{
		using Microsoft::WRL::ComPtr;
		using namespace DirectX;

		// Vertex definition.
		struct Vertex
		{
			XMFLOAT3 position;
			XMFLOAT4 color;
		};

		// Constant buffer definition.
		struct SceneConstantBuffer
		{
			XMFLOAT4 offset;

			// Constant buffers are 256-byte aligned. Add padding in the struct to allow multiple buffers
			// to be array-indexed.
			FLOAT padding[60];
		};

		class D3D12Query
		{
		private:
			HWND m_hWnd;
			UINT m_width, m_height;
			float m_aspectRatio;

			static const UINT FrameCount = 2;
			static const UINT CbvCountPerFrame = 2;

			UINT m_frameIndex = 0;
			UINT m_rtvDescriptorSize; 
			UINT m_cbvSrvDescriptorSize;

			ComPtr<ID3D12Device> m_device;
			ComPtr<ID3D12CommandQueue> m_commandQueue;
			ComPtr<ID3D12DescriptorHeap> m_rtvHeap;//RTVÃèÊö·û¶Ñ
			ComPtr<ID3D12DescriptorHeap> m_dsvHeap;//DSVÃèÊö·û¶Ñ
			ComPtr<ID3D12DescriptorHeap> m_cbvHeap;//CBVÃèÊö·û¶Ñ
			ComPtr<ID3D12QueryHeap> m_queryHeap;
			ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
			ComPtr<ID3D12CommandAllocator> m_commandAllocators[FrameCount];
			ComPtr<ID3D12GraphicsCommandList> m_commandList;
			ComPtr<ID3D12RootSignature> m_rootSignature;
			ComPtr<ID3D12PipelineState> m_pipelineState, m_queryState;

			ComPtr<ID3D12Resource> m_vertexBuffer;
			D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

			SceneConstantBuffer m_constantBufferData[CbvCountPerFrame];
			ComPtr<ID3D12Resource> m_constantBuffer;
			UINT8* m_pCbvDataBegin;

			ComPtr<ID3D12Resource> m_depthStencil;
			ComPtr<ID3D12Resource> m_queryResult;

			UINT64 m_fenceValues[FrameCount];
			ComPtr<ID3D12Fence> m_fence;
			HANDLE m_fenceEvent;

			D3D12_VIEWPORT m_viewport;
			D3D12_RECT m_scissorRect;

			ComPtr<IDXGISwapChain4> m_swapChain;
		private:
			void LoadPipeline();
			void LoadAssets();

			void PopulateCommandList();
			void WaitForGpu();
			void MoveToNextFrame();

		public:
			D3D12Query();
			void Initialize(HWND hWnd, UINT width, UINT height);
			void OnUpdate();
			void OnRender();
			void OnDestroy();
		};
	}
}
