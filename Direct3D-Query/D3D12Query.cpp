#include "pch.h"
#include "D3D12Query.h"

namespace Query {
	namespace D3D12Query
	{
		D3D12Query::D3D12Query():
			m_frameIndex(0),
			m_rtvDescriptorSize(0),
			m_cbvSrvDescriptorSize(0),
			m_constantBufferData{},
			m_fenceValues{}
		{
		}

		void D3D12Query::Initialize(HWND hWnd, UINT width, UINT height)
		{
			m_hWnd = hWnd; m_width = width; m_height = height;
			m_aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);

			m_viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) ,0.f, 1.f};
			m_scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };

			LoadPipeline();
			LoadAssets();
		}

		void D3D12Query::LoadPipeline()
		{
			UINT dxgiFlag = 0;
#if defined(_DEBUG)
			ComPtr<ID3D12Debug> debugController;
			D3D12GetDebugInterface(IID_PPV_ARGS(&debugController));
			debugController->EnableDebugLayer();
			dxgiFlag = DXGI_CREATE_FACTORY_DEBUG;
#endif

			D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device));
			D3D12_COMMAND_QUEUE_DESC queueDesc = {};
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue));

			ComPtr<IDXGIFactory6> dxgiFactory;
			CreateDXGIFactory2(dxgiFlag, IID_PPV_ARGS(&dxgiFactory));

			DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
			swapChainDesc.Width = m_width;
			swapChainDesc.Height = m_height;
			swapChainDesc.BufferCount = FrameCount;
			swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			swapChainDesc.Scaling = DXGI_SCALING_NONE;
			swapChainDesc.SampleDesc.Count = 1;

			ComPtr<IDXGISwapChain1> swapChain;
			dxgiFactory->CreateSwapChainForHwnd(m_commandQueue.Get(), m_hWnd, &swapChainDesc, nullptr, nullptr, &swapChain);
			swapChain.As(&m_swapChain);
			m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

			dxgiFactory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER);

			//创建描述符堆
			{
				//RTV
				D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
				rtvHeapDesc.NumDescriptors = FrameCount;
				rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
				rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
				m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap));

				//DSV
				D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
				dsvHeapDesc.NumDescriptors = 1;
				dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
				dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
				m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap));

				//CBV
				D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
				cbvHeapDesc.NumDescriptors = CbvCountPerFrame * FrameCount;
				cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
				cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
				m_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap));

				//查询堆(Query Heap)
				D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
				queryHeapDesc.Count = 1;
				queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
				m_device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&m_queryHeap));

				m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
				m_cbvSrvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			}

			//创建帧资源
			{
				CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

				//每一帧都需要一个RTV和一个Command Allocator
				for (UINT i = 0; i < FrameCount; i++)
				{
					m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]));
					m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);

					rtvHandle.Offset(1, m_rtvDescriptorSize);
					m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[i]));
				}
			}
		}
		void D3D12Query::LoadAssets()
		{
			//创建根签名
			{
				D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
				
				// This is the highest version the sample supports. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
				featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

				if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
				{
					featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
				}

				CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
				CD3DX12_ROOT_PARAMETER1 rootParameters[1];

				ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
				rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_VERTEX);

				D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
					D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
					D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
					D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
					D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
					D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

				CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
				rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

				ComPtr<ID3DBlob> signature;
				ComPtr<ID3DBlob> error;
				D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error);

				m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
			}

			//创建流水线状态，包括编译和加载着色器Shader
			{
				ComPtr<ID3DBlob> vertexShader;
				ComPtr<ID3DBlob> pixelShader;

#if defined(_DEBUG)
				// Enable better shader debugging with the graphics debugging tools.
				UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
				UINT compileFlags = 0;
#endif
				auto filename = Utility::GetModulePath().append(L"Shaders.hlsl");
				D3DCompileFromFile(filename.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, &vertexShader, nullptr);
				D3DCompileFromFile(filename.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, &pixelShader, nullptr);

				//定义输入布局
				D3D12_INPUT_ELEMENT_DESC inputElementDescs[]=
				{
					{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
					{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
				};

				CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);
				blendDesc.RenderTarget[0] =
				{
					TRUE, FALSE,
					D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_INV_SRC_ALPHA, D3D12_BLEND_OP_ADD,
					D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
					D3D12_LOGIC_OP_NOOP,
					D3D12_COLOR_WRITE_ENABLE_ALL,
				};

				//描述并创建PSO
				D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
				psoDesc.InputLayout = { inputElementDescs,_countof(inputElementDescs) };
				psoDesc.pRootSignature = m_rootSignature.Get();
				psoDesc.VS = CD3DX12_SHADER_BYTECODE(vertexShader.Get());
				psoDesc.PS = CD3DX12_SHADER_BYTECODE(pixelShader.Get());
				psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
				psoDesc.BlendState = blendDesc;
				psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
				psoDesc.SampleMask = UINT_MAX;
				psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
				psoDesc.NumRenderTargets = 1;
				psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
				psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
				psoDesc.SampleDesc.Count = 1;

				m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState));

				//禁止颜色写和深度写
				psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0;
				psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
				m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_queryState));
			}

			//创建Command List
			m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList));

			ComPtr<ID3D12Resource> vertexBufferUpload;
			{
				Vertex quadVertices[] =
				{
					// Far quad - in practice this would be a complex geometry.
					{ { -0.25f, -0.25f * m_aspectRatio, 0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
					{ { -0.25f, 0.25f * m_aspectRatio, 0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
					{ { 0.25f, -0.25f * m_aspectRatio, 0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f } },
					{ { 0.25f, 0.25f * m_aspectRatio, 0.5f }, { 1.0f, 1.0f, 1.0f, 1.0f } },

					// Near quad.
					{ { -0.5f, -0.35f * m_aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.65f } },
					{ { -0.5f, 0.35f * m_aspectRatio, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.65f } },
					{ { 0.5f, -0.35f * m_aspectRatio, 0.0f }, { 1.0f, 1.0f, 0.0f, 0.65f } },
					{ { 0.5f, 0.35f * m_aspectRatio, 0.0f }, { 1.0f, 1.0f, 0.0f, 0.65f } },

					// Far quad bounding box used for occlusion query (offset slightly to avoid z-fighting).
					{ { -0.25f, -0.25f * m_aspectRatio, 0.4999f }, { 0.0f, 0.0f, 0.0f, 1.0f } },
					{ { -0.25f, 0.25f * m_aspectRatio, 0.4999f }, { 0.0f, 0.0f, 0.0f, 1.0f } },
					{ { 0.25f, -0.25f * m_aspectRatio, 0.4999f }, { 0.0f, 0.0f, 0.0f, 1.0f } },
					{ { 0.25f, 0.25f * m_aspectRatio, 0.4999f }, { 0.0f, 0.0f, 0.0f, 1.0f } },
				};

				const UINT vertexBufferSize = sizeof(quadVertices);

				m_device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS(&m_vertexBuffer));

				m_device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&vertexBufferUpload));

				D3D12_SUBRESOURCE_DATA vertexData = {};
				vertexData.pData = reinterpret_cast<UINT8*>(quadVertices);
				vertexData.RowPitch = vertexBufferSize;
				vertexData.SlicePitch = vertexData.RowPitch;

				UpdateSubresources<1>(
					m_commandList.Get(), 
					m_vertexBuffer.Get(), 
					vertexBufferUpload.Get(), 
					0, 0, 1, 
					&vertexData);

				m_commandList->ResourceBarrier(1, 
					&CD3DX12_RESOURCE_BARRIER::Transition(
						m_vertexBuffer.Get(), 
						D3D12_RESOURCE_STATE_COPY_DEST, 
						D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
				);

				m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
				m_vertexBufferView.SizeInBytes = sizeof(quadVertices);
				m_vertexBufferView.StrideInBytes = sizeof(Vertex);
			}

			//创建Constant Buffer
			{
				m_device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(FrameCount * sizeof(m_constantBufferData)),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&m_constantBuffer));
				
				CD3DX12_RANGE readRange(0, 0);
				m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pCbvDataBegin));
				ZeroMemory(m_pCbvDataBegin, FrameCount * sizeof(m_constantBufferData));

				CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
				D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = m_constantBuffer->GetGPUVirtualAddress();

				D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
				cbvDesc.SizeInBytes = sizeof(SceneConstantBuffer);

				for (UINT n = 0; n < FrameCount; n++)
				{
					cbvDesc.BufferLocation = gpuAddress;
					m_device->CreateConstantBufferView(&cbvDesc, cpuHandle);

					cpuHandle.Offset(m_cbvSrvDescriptorSize);
					gpuAddress += cbvDesc.SizeInBytes;
					cbvDesc.BufferLocation = gpuAddress;

					m_device->CreateConstantBufferView(&cbvDesc, cpuHandle);

					cpuHandle.Offset(m_cbvSrvDescriptorSize);
					gpuAddress += cbvDesc.SizeInBytes;
				}
			}

			//创建depth stencil view(DSV)
			{
				D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc = {};
				depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
				depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
				depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;

				D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
				depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
				depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
				depthOptimizedClearValue.DepthStencil.Stencil = 0;

				m_device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, m_width, m_height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
					D3D12_RESOURCE_STATE_DEPTH_WRITE,
					&depthOptimizedClearValue,
					IID_PPV_ARGS(&m_depthStencil)
				);

				m_device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
			}

			//创建Query Result Buffer
			{
				m_device->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(8),
					D3D12_RESOURCE_STATE_PREDICATION,
					nullptr,
					IID_PPV_ARGS(&m_queryResult)
				);
			}

			//关闭Command List并且执行，将顶点缓冲复制到默认堆
			m_commandList->Close();
			ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
			m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

			//创建同步对象
			{
				m_device->CreateFence(m_fenceValues[m_frameIndex], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));
				m_fenceValues[m_frameIndex]++;
				m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
				
				WaitForGpu();
			}
		}

		void D3D12Query::OnUpdate()
		{
			const float translationSpeed = 0.01f;
			const float offsetBounds = 1.5f;

			// Animate the near quad.
			m_constantBufferData[1].offset.x += translationSpeed;
			if (m_constantBufferData[1].offset.x > offsetBounds)
			{
				m_constantBufferData[1].offset.x = -offsetBounds;
			}

			UINT cbvIndex = m_frameIndex * CbvCountPerFrame + 1;
			UINT8* destination = m_pCbvDataBegin + (cbvIndex * sizeof(SceneConstantBuffer));
			memcpy(destination, &m_constantBufferData[1], sizeof(SceneConstantBuffer));
		}

		void D3D12Query::OnRender()
		{
			// Record all the commands we need to render the scene into the command list.
			PopulateCommandList();

			// Execute the command list.
			ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
			m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
			m_swapChain->Present(1, 0);

			MoveToNextFrame();
		}

		void D3D12Query::PopulateCommandList()
		{
			m_commandAllocators[m_frameIndex]->Reset();
			m_commandList->Reset(m_commandAllocators[m_frameIndex].Get(), m_pipelineState.Get());

			m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

			ID3D12DescriptorHeap* ppHeaps[] = { m_cbvHeap.Get() };
			m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

			m_commandList->RSSetViewports(1, &m_viewport);
			m_commandList->RSSetScissorRects(1, &m_scissorRect);

			// Indicate that the back buffer will be used as a render target.
			m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

			CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
			CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
			m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

			// Record commands.
			const float clearColor[] = { 0.5f, 0.7f, 0.8f, 1.0f };
			m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
			m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			// Draw the quads and perform the occlusion query.
			{
				CD3DX12_GPU_DESCRIPTOR_HANDLE cbvFarQuad(m_cbvHeap->GetGPUDescriptorHandleForHeapStart(), m_frameIndex * CbvCountPerFrame, m_cbvSrvDescriptorSize);
				CD3DX12_GPU_DESCRIPTOR_HANDLE cbvNearQuad(cbvFarQuad, m_cbvSrvDescriptorSize);

				m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
				m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);

				// Draw the far quad conditionally based on the result of the occlusion query
				// from the previous frame.
				m_commandList->SetGraphicsRootDescriptorTable(0, cbvFarQuad);
				m_commandList->SetPredication(m_queryResult.Get(), 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
				m_commandList->DrawInstanced(4, 1, 0, 0);

				// Disable predication and always draw the near quad.
				m_commandList->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
				m_commandList->SetGraphicsRootDescriptorTable(0, cbvNearQuad);
				m_commandList->DrawInstanced(4, 1, 4, 0);

				// Run the occlusion query with the bounding box quad.
				m_commandList->SetGraphicsRootDescriptorTable(0, cbvFarQuad);
				m_commandList->SetPipelineState(m_queryState.Get());
				m_commandList->BeginQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0);
				m_commandList->DrawInstanced(4, 1, 8, 0);
				m_commandList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0);

				// Resolve the occlusion query and store the results in the query result buffer
				// to be used on the subsequent frame.
				m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_queryResult.Get(), D3D12_RESOURCE_STATE_PREDICATION, D3D12_RESOURCE_STATE_COPY_DEST));
				m_commandList->ResolveQueryData(m_queryHeap.Get(), D3D12_QUERY_TYPE_BINARY_OCCLUSION, 0, 1, m_queryResult.Get(), 0);
				m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_queryResult.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PREDICATION));
			}
			m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

			m_commandList->Close();
		}

		void D3D12Query::OnDestroy()
		{
			// Ensure that the GPU is no longer referencing resources that are about to be
			// cleaned up by the destructor.
			WaitForGpu();

			CloseHandle(m_fenceEvent);
		}


		// Wait for pending GPU work to complete.
		void D3D12Query::WaitForGpu()
		{
			// Schedule a Signal command in the queue.
			m_commandQueue->Signal(m_fence.Get(), m_fenceValues[m_frameIndex]);

			// Wait until the fence has been processed.
			m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
			WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

			// Increment the fence value for the current frame.
			m_fenceValues[m_frameIndex]++;
		}

		// Prepare to render the next frame.
		void D3D12Query::MoveToNextFrame()
		{
			// Schedule a Signal command in the queue.
			const UINT64 currentFenceValue = m_fenceValues[m_frameIndex];
			m_commandQueue->Signal(m_fence.Get(), currentFenceValue);

			// Update the frame index.
			m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

			// If the next frame is not ready to be rendered yet, wait until it is ready.
			if (m_fence->GetCompletedValue() < m_fenceValues[m_frameIndex])
			{
				m_fence->SetEventOnCompletion(m_fenceValues[m_frameIndex], m_fenceEvent);
				WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
			}

			// Set the fence value for the next frame.
			m_fenceValues[m_frameIndex] = currentFenceValue + 1;
		}

	}
}
