// D3D12.cpp : 定义应用程序的入口点。
//

#include "SDKDDKVer.h"
#include "framework.h"
#include "D3D12.h"
//添加WTL支持 方便使用COM
#include <wrl.h>
#include <strsafe.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
//for d3d12
#include <d3d12.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

using namespace Microsoft;
using namespace Microsoft::WRL;
using namespace DirectX;

//linker
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

//#include "WindowsCommons\d3dx12.h"

#define GRS_WND_CLASS_NAME _T("Game Window Class")
#define GRS_WND_TITLE   _T("DirectX12 Trigger Sample")

#define GRS_THROW_IF_FAILED(hr) if (FAILED(hr)){ throw CGRSCOMException(hr); }


class CGRSCOMException
{
public:
	CGRSCOMException(HRESULT hr) : m_hrError(hr)
	{
	}
	HRESULT Error() const
	{
		return m_hrError;
	}
private:
	const HRESULT m_hrError;
};

struct GRS_VERTEX
{
	XMFLOAT4 position;
	XMFLOAT4 color;
};

// 全局变量:
HINSTANCE hInst;                                // 当前实例
int iWidth = 1024;
int iHeight = 768;

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// TODO: 在此处放置代码。

	const UINT nFrameBackBufCount = 3u;

	UINT nFrameIndex = 0;

	DXGI_FORMAT emRenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	const float faClearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };

	UINT nFrame = 0;

	UINT nDXGIFactoryFlags = 0U;
	UINT nRTVDescriptorSize = 0U;

	HWND hWnd = nullptr;
	MSG  msg = {};
	TCHAR pszAppPath[MAX_PATH] = {};

	float fTrangleSize = 3.0f;

	D3D12_VERTEX_BUFFER_VIEW stVertexBufferView = {};

	UINT64 n64FenceValue = 0ui64;
	HANDLE hEventFence = nullptr;


	D3D12_VIEWPORT						stViewPort = { 0.0f, 0.0f, static_cast<float>(iWidth), static_cast<float>(iHeight), D3D12_MIN_DEPTH, D3D12_MAX_DEPTH };
	D3D12_RECT							stScissorRect = { 0, 0, static_cast<LONG>(iWidth), static_cast<LONG>(iHeight) };

	D3D_FEATURE_LEVEL emFeatureLevel = D3D_FEATURE_LEVEL_12_1;

	ComPtr<IDXGIFactory5>                pIDXGIFactory5;
	ComPtr<IDXGIAdapter1>                pIAdapter1;
	ComPtr<ID3D12Device4>                pID3D12Device4;
	ComPtr<ID3D12CommandQueue>           pICMDQueue;
	ComPtr<IDXGISwapChain1>              pISwapChain1;
	ComPtr<IDXGISwapChain3>              pISwapChain3;
	ComPtr<ID3D12DescriptorHeap>         pIRTVHeap;
	ComPtr<ID3D12Resource>               pIARenderTargets[nFrameBackBufCount];

	ComPtr<ID3D12RootSignature>          pIRootSignature;
	ComPtr<ID3D12PipelineState>          pIPipelineState;

	ComPtr<ID3D12CommandAllocator>       pICMDAlloc;
	ComPtr<ID3D12GraphicsCommandList>    pICMDList;
	ComPtr<ID3D12Resource>               pIVertexBuffer;
	ComPtr<ID3D12Fence>                  pIFence;

	MyRegisterClass(hInstance);

	// 得到当前的工作目录，方便我们使用相对路径来访问各种资源文件
	{
		if (0 == ::GetModuleFileName(nullptr, pszAppPath, MAX_PATH))
		{
			GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
		}

		WCHAR* lastSlash = _tcsrchr(pszAppPath, _T('\\'));
		if (lastSlash)
		{
			*(lastSlash) = _T('\0');
		}

		lastSlash = _tcsrchr(pszAppPath, _T('\\'));
		if (lastSlash)
		{
			*(lastSlash) = _T('\0');
		}

		lastSlash = _tcsrchr(pszAppPath, _T('\\'));
		if (lastSlash)
		{
			*(lastSlash + 1) = _T('\0');
		}
	}

	// 创建窗口
	{
		hInst = hInstance; // 将实例句柄存储在全局变量中

		hWnd = CreateWindowW(GRS_WND_CLASS_NAME
			, GRS_WND_TITLE
			, WS_OVERLAPPED | WS_SYSMENU
			, CW_USEDEFAULT
			, 0
			, iWidth
			, iHeight
			, nullptr
			, nullptr
			, hInstance
			, nullptr);

		if (!hWnd)
		{
			return FALSE;
		}

		ShowWindow(hWnd, nCmdShow);
		UpdateWindow(hWnd);
	}

	// 执行应用程序初始化:
	//if (!InitInstance(hInstance, nCmdShow))
	//{
	//	return FALSE;
	//}

	// 打开显示子系统的调试支持
	{
#if defined(_DEBUG)
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
			// 打开附加的调试支持
			nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
#endif
	}

	// 创建DXGI Factory对象
	{
		GRS_THROW_IF_FAILED(CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pIDXGIFactory5)));
		// 关闭ALT+ENTER键切换全屏的功能，因为我们没有实现OnSize处理，所以先关闭
		GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
	}

	{
		DXGI_ADAPTER_DESC1 stAdapterDesc = {};
		for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pIDXGIFactory5->EnumAdapters1(adapterIndex, &pIAdapter1); ++adapterIndex)
		{
			pIAdapter1->GetDesc1(&stAdapterDesc);

			if (stAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(pIAdapter1.Get(), emFeatureLevel, __uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}

		GRS_THROW_IF_FAILED(D3D12CreateDevice(pIAdapter1.Get(), emFeatureLevel, IID_PPV_ARGS(&pID3D12Device4)));

		TCHAR pszWndTitle[MAX_PATH] = {};

		GRS_THROW_IF_FAILED(pIAdapter1->GetDesc1(&stAdapterDesc));
		::GetWindowText(hWnd, pszWndTitle, MAX_PATH);
		StringCchPrintf(pszWndTitle
			, MAX_PATH
			, _T("%s (GPU:%s)")
			, pszWndTitle
			, stAdapterDesc.Description);
		::SetWindowText(hWnd, pszWndTitle);
	}

	// 创建直接命令队列
	{
		D3D12_COMMAND_QUEUE_DESC stQueueDesc = {};
		stQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandQueue(&stQueueDesc, IID_PPV_ARGS(&pICMDQueue)));
	}

	// 创建交换链
	{
		DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
		stSwapChainDesc.BufferCount = nFrameBackBufCount;
		stSwapChainDesc.Width = iWidth;
		stSwapChainDesc.Height = iHeight;
		stSwapChainDesc.Format = emRenderTargetFormat;
		stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		stSwapChainDesc.SampleDesc.Count = 1;

		GRS_THROW_IF_FAILED(pIDXGIFactory5->CreateSwapChainForHwnd(
			pICMDQueue.Get(),
			hWnd,
			&stSwapChainDesc,
			nullptr,
			nullptr,
			&pISwapChain1
		));

		GRS_THROW_IF_FAILED(pISwapChain1.As(&pISwapChain3));

		nFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

		// 创建RTV(渲染目标视图)描述符堆(这里堆的含义应当理解为数组或者固定大小元素的固定大小显存池)
		{
			D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
			stRTVHeapDesc.NumDescriptors = nFrameBackBufCount;
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			stRTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(
				&stRTVHeapDesc
				, IID_PPV_ARGS(&pIRTVHeap)
			));
			//得到每个描述符元素的大小
			nRTVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(
				D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		}

		// 创建RTV的描述符
		{
			D3D12_CPU_DESCRIPTOR_HANDLE stRTVHandle
				= pIRTVHeap->GetCPUDescriptorHandleForHeapStart();
			for (UINT i = 0; i < nFrameBackBufCount; i++)
			{
				GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(i
					, IID_PPV_ARGS(&pIARenderTargets[i])));

				pID3D12Device4->CreateRenderTargetView(pIARenderTargets[i].Get()
					, nullptr
					, stRTVHandle);

				stRTVHandle.ptr += nRTVDescriptorSize;
			}
		}
	}

	// 创建一个空的根描述符，也就是默认的根描述符
	{
		D3D12_ROOT_SIGNATURE_DESC stRootSignatureDesc =
		{
			0
			,nullptr
			,0
			,nullptr
			,D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
		};

		ComPtr<ID3DBlob> pISignatureBlob;
		ComPtr<ID3DBlob> pIErrorBlob;

		GRS_THROW_IF_FAILED(D3D12SerializeRootSignature(
			&stRootSignatureDesc
			, D3D_ROOT_SIGNATURE_VERSION_1
			, &pISignatureBlob
			, &pIErrorBlob));

		GRS_THROW_IF_FAILED(pID3D12Device4->CreateRootSignature(0
			, pISignatureBlob->GetBufferPointer()
			, pISignatureBlob->GetBufferSize()
			, IID_PPV_ARGS(&pIRootSignature)));

		// 创建命令列表分配器
		{
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT
				, IID_PPV_ARGS(&pICMDAlloc)));

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandList(
				0
				, D3D12_COMMAND_LIST_TYPE_DIRECT
				, pICMDAlloc.Get()
				, pIPipelineState.Get()
				, IID_PPV_ARGS(&pICMDList)));
		}

		// 编译Shader创建渲染管线状态对象
		{
			ComPtr<ID3DBlob> pIBlobVertexShader;
			ComPtr<ID3DBlob> pIBlobPixelShader;

#if defined(_DEBUG)
			//调试状态下，打开Shader编译的调试标志，不优化
			UINT nCompileFlags =
				D3DCOMPILE_DEBUG
				| D3DCOMPILE_SKIP_OPTIMIZATION;
#else
			UINT nCompileFlags = 0;
#endif
			TCHAR pszShaderFileName[MAX_PATH] = {};
			StringCchPrintf(pszShaderFileName
				, MAX_PATH
				, _T("%s1-D3D12Triangle\\Shader\\shaders.hlsl")
				, pszAppPath);

			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName
				, nullptr
				, nullptr
				, "VSMain"
				, "vs_5_0"
				, nCompileFlags
				, 0
				, &pIBlobVertexShader
				, nullptr));

			GRS_THROW_IF_FAILED(D3DCompileFromFile(pszShaderFileName
				, nullptr
				, nullptr
				, "PSMain"
				, "ps_5_0"
				, nCompileFlags
				, 0
				, &pIBlobPixelShader
				, nullptr));

			D3D12_INPUT_ELEMENT_DESC stInputElementDescs[] =
			{
				{
					"POSITION"
					, 0
					, DXGI_FORMAT_R32G32B32A32_FLOAT
					, 0
					, 0
					, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA
					, 0
				},
				{
					"COLOR"
					, 0
					, DXGI_FORMAT_R32G32B32A32_FLOAT
					, 0
					, 0
					, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA
					, 0
				}
			};

			// 定义渲染管线状态描述结构，创建渲染管线对象
			D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};
			stPSODesc.InputLayout = { stInputElementDescs, _countof(stInputElementDescs) };
			stPSODesc.pRootSignature = pIRootSignature.Get();
			stPSODesc.VS.pShaderBytecode = pIBlobVertexShader->GetBufferPointer();
			stPSODesc.VS.BytecodeLength = pIBlobVertexShader->GetBufferSize();
			stPSODesc.PS.pShaderBytecode = pIBlobPixelShader->GetBufferPointer();
			stPSODesc.PS.BytecodeLength = pIBlobPixelShader->GetBufferSize();

			stPSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
			stPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

			stPSODesc.BlendState.AlphaToCoverageEnable = FALSE;
			stPSODesc.BlendState.IndependentBlendEnable = FALSE;
			stPSODesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			stPSODesc.DepthStencilState.DepthEnable = FALSE;
			stPSODesc.DepthStencilState.StencilEnable = FALSE;

			stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

			stPSODesc.NumRenderTargets = 1;
			stPSODesc.RTVFormats[0] = emRenderTargetFormat;

			stPSODesc.SampleMask = UINT_MAX;
			stPSODesc.SampleDesc.Count = 1;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateGraphicsPipelineState(&stPSODesc, IID_PPV_ARGS(&pIPipelineState)));
		}

		// 创建顶点缓冲
		{

			GRS_VERTEX stTriangleVertices[] =
			{
				{ { 0.0f, 0.25f * fTrangleSize, 0.0f ,1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } },
				{ { 0.25f * fTrangleSize, -0.25f * fTrangleSize, 0.0f ,1.0f  }, { 0.0f, 1.0f, 0.0f, 1.0f } },
				{ { -0.25f * fTrangleSize, -0.25f * fTrangleSize, 0.0f  ,1.0f }, { 0.0f, 0.0f, 1.0f, 1.0f } }
			};

			const UINT nVertexBufferSize = sizeof(stTriangleVertices);

			D3D12_HEAP_PROPERTIES stHeapProp = { D3D12_HEAP_TYPE_UPLOAD };

			D3D12_RESOURCE_DESC stResDesc = {};
			stResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			stResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			stResDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
			stResDesc.Format = DXGI_FORMAT_UNKNOWN;
			stResDesc.Width = nVertexBufferSize;
			stResDesc.Height = 1;
			stResDesc.DepthOrArraySize = 1;
			stResDesc.MipLevels = 1;
			stResDesc.SampleDesc.Count = 1;
			stResDesc.SampleDesc.Quality = 0;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&stHeapProp,
				D3D12_HEAP_FLAG_NONE,
				&stResDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pIVertexBuffer)));

			UINT8* pVertexDataBegin = nullptr;
			D3D12_RANGE stReadRange = { 0,0 };
			GRS_THROW_IF_FAILED(pIVertexBuffer->Map(
				0
				, &stReadRange
				, reinterpret_cast<void**>(&pVertexDataBegin)));


			memcpy(pVertexDataBegin
				, stTriangleVertices
				, sizeof(stTriangleVertices));

			pIVertexBuffer->Unmap(0, nullptr);

			stVertexBufferView.BufferLocation = pIVertexBuffer->GetGPUVirtualAddress();
			stVertexBufferView.StrideInBytes = sizeof(GRS_VERTEX);
			stVertexBufferView.SizeInBytes = nVertexBufferSize;
		}

		// 创建一个同步对象——围栏，用于等待渲染完成，因为现在Draw Call是异步的了
		{
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateFence(0
				, D3D12_FENCE_FLAG_NONE
				, IID_PPV_ARGS(&pIFence)));
			n64FenceValue = 1;

			hEventFence = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (hEventFence == nullptr)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}
		}
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_D3D12));

	// 主消息循环:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}



//
//  函数: MyRegisterClass()
//
//  目标: 注册窗口类。
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_GLOBALCLASS;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_D3D12));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_D3D12);
	wcex.lpszClassName = GRS_WND_CLASS_NAME;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   函数: InitInstance(HINSTANCE, int)
//
//   目标: 保存实例句柄并创建主窗口
//
//   注释:
//
//        在此函数中，我们在全局变量中保存实例句柄并
//        创建和显示主程序窗口。
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // 将实例句柄存储在全局变量中

	HWND hWnd = CreateWindowW(GRS_WND_CLASS_NAME
		, GRS_WND_TITLE
		, WS_OVERLAPPED | WS_SYSMENU
		, CW_USEDEFAULT
		, 0
		, iWidth
		, iHeight
		, nullptr
		, nullptr
		, hInstance
		, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  函数: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  目标: 处理主窗口的消息。
//
//  WM_COMMAND  - 处理应用程序菜单
//  WM_PAINT    - 绘制主窗口
//  WM_DESTROY  - 发送退出消息并返回
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// 分析菜单选择:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: 在此处添加使用 hdc 的任何绘图代码...
		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// “关于”框的消息处理程序。
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
