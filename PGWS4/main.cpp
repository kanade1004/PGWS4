﻿#include <Windows.h>
#include <tchar.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <vector>
#include <d3dcompiler.h>
#include <DirectXTex.h>
#include <d3dx12.h>
#ifdef _DEBUG
#include <iostream>
#endif

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib,"DirectXTex.lib")

using namespace std;
using namespace DirectX;

void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	printf(format, valist);
	va_end(valist);
#endif
}

LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

std::string GetTexturePathFromModelAndTexPath(
	const std::string& modelPath,
	const char* texPath)
{
	int pathIndex1 = modelPath.rfind('/');
	int pathIndex2 = modelPath.rfind('\\');
	auto pathIndex = max(pathIndex1, pathIndex2);
	auto folderPath = modelPath.substr(0, pathIndex + 1);
	return folderPath + texPath;
}

std::string GetExtension(const std::string& path)
{
	int idx = static_cast<int>(path.rfind('.'));
	return path.substr(
		idx + 1, path.length() - idx - 1);
}

std::pair<std::string, std::string> SplitFileName(
	const std::string& path, const char splitter = '*')
{
	int idx = static_cast<int>(path.find(splitter));
	pair<std::string, std::string> ret;
	ret.first = path.substr(0, idx);
	ret.second = path.substr(
		idx + 1, path.length() - idx - 1);
	return ret;
}


std::wstring GetWideStringFromString(const std::string& str)
{
	auto num1 = MultiByteToWideChar(
		CP_ACP,
		MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
		str.c_str(),
		-1,
		nullptr,
		0);

	std::wstring wstr;
	wstr.resize(num1);

	auto num2 = MultiByteToWideChar(
		CP_ACP,
		MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
		str.c_str(),
		-1,
		&wstr[0],
		num1);

	assert(num1 == num2);
	return wstr;
}

ID3D12Resource* LoadTextureFromFile(std::string& texPath, ID3D12Device* dev)
{
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	HRESULT result = LoadFromWICFile(
		GetWideStringFromString(texPath).c_str(),
		WIC_FLAGS_NONE,
		&metadata,
		scratchImg);

	if (FAILED(result)) { return nullptr; }

	D3D12_HEAP_PROPERTIES texHeapProp = {};
	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	texHeapProp.CreationNodeMask = 0;
	texHeapProp.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Format = metadata.format;
	resDesc.Width = static_cast<UINT>(metadata.width);
	resDesc.Height = static_cast<UINT>(metadata.height);
	resDesc.DepthOrArraySize = static_cast<UINT16>(metadata.arraySize);
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.MipLevels = static_cast<UINT16>(metadata.mipLevels);
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* texbuff = nullptr;
	result = dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&texbuff)
	);
	if (FAILED(result)) { return nullptr; }

	const Image* img = scratchImg.GetImage(0, 0, 0);

	result = texbuff->WriteToSubresource(
		0,
		nullptr,
		img->pixels,
		static_cast<UINT>(img->rowPitch),
		static_cast<UINT>(img->slicePitch)
	);
	if (FAILED(result)) { return nullptr; }

	return texbuff;
}

ID3D12Resource* CreateWhiteTexture(ID3D12Device* dev)
{
	D3D12_HEAP_PROPERTIES texHeapProp = {};

	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	texHeapProp.CreationNodeMask = 0;
	texHeapProp.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resDesc.Width = 4;
	resDesc.Height = 4;
	resDesc.DepthOrArraySize = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.MipLevels = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* whiteBuff = nullptr;
	auto result = dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS(&whiteBuff)
	);
	if (FAILED(result)) { return nullptr; }

	std::vector<unsigned char> data(4 * 4 * 4);
	std::fill(data.begin(), data.end(), 0xff);

	result = whiteBuff->WriteToSubresource(
		0,
		nullptr,
		data.data(),
		4 * 4,
		static_cast<UINT>(data.size()));

	return whiteBuff;
}

size_t AlignmentedSize(size_t size, size_t alignment)
{
	return size + alignment - size % alignment;
}

#ifdef _DEBUG
void EnableDebugLayer()
{
	ID3D12Debug* debugLayer = nullptr;
	HRESULT result = D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer));

	if (!SUCCEEDED(result)) return;

	debugLayer->EnableDebugLayer();
	debugLayer->Release();
}
#endif

#ifdef _DEBUG
int main()
{
#else
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#endif
const unsigned int window_width = 1280;
const unsigned int window_height = 720;

ID3D12Device* _dev = nullptr;
IDXGIFactory6* _dxgiFactory = nullptr;
ID3D12CommandAllocator* _cmdAllocator = nullptr;
ID3D12GraphicsCommandList* _cmdList = nullptr;
ID3D12CommandQueue* _cmdQueue = nullptr;
IDXGISwapChain4* _swapchain = nullptr;

WNDCLASSEX w = {};

w.cbSize = sizeof(WNDCLASSEX);
w.lpfnWndProc = (WNDPROC)WindowProcedure;
w.lpszClassName = _T("DX12Sample");
w.hInstance = GetModuleHandle(nullptr);

RegisterClassEx(&w);

RECT wrc = { 0,0,window_width,window_height };

AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

HWND hwnd = CreateWindow(w.lpszClassName, _T("DX12 テスト "),
	WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, wrc.right - wrc.left, wrc.bottom - wrc.top,
	nullptr, nullptr, w.hInstance, nullptr);

HRESULT D3D12CreateDevice(
	IUnknown * pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void** ppDevice);

#ifdef _DEBUG
EnableDebugLayer();
#endif

D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0,
D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0, };

#ifdef _DEBUG
auto result = CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory));
#else
auto result = CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory));
#endif

std::vector <IDXGIAdapter*> adapters;
IDXGIAdapter* tmpAdapter = nullptr;
for (int i = 0;
	_dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND;
	++i)
{
	adapters.push_back(tmpAdapter);
}
for (auto adpt : adapters)
{
	DXGI_ADAPTER_DESC adesc = {};
	adpt->GetDesc(&adesc);
	std::wstring strDesc = adesc.Description;
	if (strDesc.find(L"NVIDIA") != std::string::npos)
	{
		tmpAdapter = adpt;
		break;
	}
	else if (strDesc.find(L"Radeon") != std::string::npos)
	{
		tmpAdapter = adpt;
		break;
	}
}

D3D_FEATURE_LEVEL featureLevel;
for (auto lv : levels)
{
	if (D3D12CreateDevice(tmpAdapter, lv, IID_PPV_ARGS(&_dev)) == S_OK)
	{
		featureLevel = lv;
		break;
	}
}

result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
	IID_PPV_ARGS(&_cmdAllocator));
result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
	_cmdAllocator, nullptr,
	IID_PPV_ARGS(&_cmdList));

D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
cmdQueueDesc.NodeMask = 0;
cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue));

DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
swapchainDesc.Width = window_width;
swapchainDesc.Height = window_height;
swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
swapchainDesc.Stereo = false;
swapchainDesc.SampleDesc.Count = 1;
swapchainDesc.SampleDesc.Quality = 0;
swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
swapchainDesc.BufferCount = 2;
swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
result = _dxgiFactory->CreateSwapChainForHwnd(
	_cmdQueue, hwnd,
	&swapchainDesc, nullptr, nullptr,
	(IDXGISwapChain1**)&_swapchain);

D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
heapDesc.NodeMask = 0;
heapDesc.NumDescriptors = 2;
heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

ID3D12DescriptorHeap* rtvHeaps = nullptr;
result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));

DXGI_SWAP_CHAIN_DESC swcDesc = {};
result = _swapchain->GetDesc(&swcDesc);

D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);
for (UINT idx = 0; idx < swcDesc.BufferCount; ++idx)
{
	result = _swapchain->GetBuffer(idx, IID_PPV_ARGS(&_backBuffers[idx]));
	D3D12_CPU_DESCRIPTOR_HANDLE handle
		= rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += idx * _dev->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	_dev->CreateRenderTargetView(_backBuffers[idx], &rtvDesc, handle);
}

D3D12_RESOURCE_DESC depthResDesc = {};
depthResDesc.Dimension =
D3D12_RESOURCE_DIMENSION_TEXTURE2D;
depthResDesc.Width = window_width;
depthResDesc.Height = window_height;
depthResDesc.DepthOrArraySize = 1;
depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;
depthResDesc.SampleDesc.Count = 1;
depthResDesc.Flags =
D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; 

D3D12_HEAP_PROPERTIES depthHeapProp = {};
depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

D3D12_CLEAR_VALUE depthClearValue = {};
depthClearValue.DepthStencil.Depth = 1.0f;
depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;

ID3D12Resource* depthBuffer = nullptr;
result = _dev->CreateCommittedResource(
	&depthHeapProp,
	D3D12_HEAP_FLAG_NONE,
	&depthResDesc,
	D3D12_RESOURCE_STATE_DEPTH_WRITE,
	&depthClearValue,
	IID_PPV_ARGS(&depthBuffer));

D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
dsvHeapDesc.NumDescriptors = 1;
dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
ID3D12DescriptorHeap* dsvHeap = nullptr;
result = _dev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));

D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
_dev->CreateDepthStencilView(
	depthBuffer,
	&dsvDesc,
	dsvHeap->GetCPUDescriptorHandleForHeapStart());

ID3D12Fence* _fence = nullptr;
UINT64 _fenceVal = 0;
result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));


ShowWindow(hwnd, SW_SHOW);

ID3D12Resource* whiteTex = CreateWhiteTexture(_dev);

struct PMDHeader
{
	float version;
	char model_name[20];
	char comment[256];
};

char signature[3] = {};
PMDHeader pmdheader = {};
std::string strModelPath = "Model/初音ミクmetal.pmd";
FILE* fp;
fopen_s(&fp, strModelPath.c_str(), "rb");

fread(signature, sizeof(signature), 1, fp);
fread(&pmdheader, sizeof(pmdheader), 1, fp);

constexpr size_t pmdvertex_size = 38;

unsigned int vertNum;
fread(&vertNum, sizeof(vertNum), 1, fp);

#pragma pack(1)
struct PMDMaterial
{
	XMFLOAT3 diffuse;
	float alpha;
	float specularity;
	XMFLOAT3 specular;
	XMFLOAT3 ambient;
	unsigned char toonIdx;
	unsigned char edgeFlg;
	unsigned int indicesNum;
	char texFilePath[20];
};
#pragma pack()

#pragma pack(push, 1)
struct PMD_VERTEX
{
	XMFLOAT3 pos;
	XMFLOAT3 normal;
	XMFLOAT2 uv;
	uint16_t bone_no[2];
	uint8_t  weight;
	uint8_t  EdgeFlag;
	uint16_t dummy;
};
#pragma pack(pop)

std::vector<PMD_VERTEX> vertices(vertNum);
for (unsigned int i = 0; i < vertNum; i++)
{
	fread(&vertices[i], pmdvertex_size, 1, fp);
}

ID3D12Resource* vertBuff = nullptr;

auto heapPropDx = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
auto resourceDescDx = CD3DX12_RESOURCE_DESC::Buffer(vertices.size() * sizeof(PMD_VERTEX));
result = _dev->CreateCommittedResource(
	&heapPropDx,
	D3D12_HEAP_FLAG_NONE,
	&resourceDescDx,
	D3D12_RESOURCE_STATE_GENERIC_READ,
	nullptr,
	IID_PPV_ARGS(&vertBuff));

PMD_VERTEX* vertMap = nullptr;
result = vertBuff->Map(0, nullptr, (void**)&vertMap);
std::copy(std::begin(vertices), std::end(vertices), vertMap);
vertBuff->Unmap(0, nullptr);

D3D12_VERTEX_BUFFER_VIEW vbView = {};
vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();
vbView.SizeInBytes = static_cast<UINT>(vertices.size() * sizeof(PMD_VERTEX));
vbView.StrideInBytes = sizeof(vertices[0]);

unsigned int indicesNum;
fread(&indicesNum, sizeof(indicesNum), 1, fp);

std::vector<unsigned short> indices;
indices.resize(indicesNum);
fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp);

ID3D12Resource* idxBuff = nullptr;

resourceDescDx.Width = indices.size() * sizeof(indices[0]);
result = _dev->CreateCommittedResource(
	&heapPropDx,
	D3D12_HEAP_FLAG_NONE,
	&resourceDescDx,
	D3D12_RESOURCE_STATE_GENERIC_READ,
	nullptr,
	IID_PPV_ARGS(&idxBuff));

unsigned short* mappedIdx = nullptr;
idxBuff->Map(0, nullptr, (void**)&mappedIdx);
std::copy(std::begin(indices), std::end(indices), mappedIdx);
idxBuff->Unmap(0, nullptr);

D3D12_INDEX_BUFFER_VIEW ibView = {};
ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
ibView.Format = DXGI_FORMAT_R16_UINT;
ibView.SizeInBytes = indices.size() * sizeof(indices[0]);

unsigned int materialNum;
fread(&materialNum, sizeof(materialNum), 1, fp);

std::vector<PMDMaterial> pmdMaterials(materialNum);

fread(
	pmdMaterials.data(),
	pmdMaterials.size() * sizeof(PMDMaterial),
	1,
	fp);


struct MaterialForHlsl
{
	XMFLOAT3 diffuse;
	float alpha;
	XMFLOAT3 specular;
	float specularity;
	XMFLOAT3 ambient;
};

struct AdditionalMaterial
{
	std::string texPath;
	int toonIdx;
	bool edgeFlg;
};

struct Material
{
	unsigned int indicesNum;
	MaterialForHlsl material;
	AdditionalMaterial additional;
};

std::vector<Material> materials(pmdMaterials.size());

for (int i = 0; i < pmdMaterials.size(); ++i)
{
	materials[i].indicesNum = pmdMaterials[i].indicesNum;
	materials[i].material.diffuse = pmdMaterials[i].diffuse;
	materials[i].material.alpha = pmdMaterials[i].alpha;
	materials[i].material.specular = pmdMaterials[i].specular;
	materials[i].material.specularity = pmdMaterials[i].specularity;
	materials[i].material.ambient = pmdMaterials[i].ambient;
}

vector<ID3D12Resource*> textureResources(materialNum);
vector<ID3D12Resource*> sphResources(materialNum, nullptr);

for (int i = 0; i < pmdMaterials.size(); ++i)
{
	if (strlen(pmdMaterials[i].texFilePath) == 0)
	{
		textureResources[i] = nullptr;
		continue;
	}

	std::string texFileName = pmdMaterials[i].texFilePath;
	std::string sphFileName = "";

	if (std::count(texFileName.begin(), texFileName.end(), '*') > 0)
	{
		auto namepair = SplitFileName(texFileName);
		if (GetExtension(namepair.first) == "sph" ||
			GetExtension(namepair.first) == "spa")
		{
			texFileName = namepair.second;
			sphFileName = namepair.first;
		}
		else
		{
			texFileName = namepair.first;
			sphFileName = namepair.second;
		}
	}
	else
	{
		if (GetExtension(pmdMaterials[i].texFilePath) == "sph")
		{
			sphFileName = pmdMaterials[i].texFilePath;
			texFileName = "";
		}
	}

	auto texFilePath = GetTexturePathFromModelAndTexPath(
		strModelPath, texFileName.c_str());
	textureResources[i] = LoadTextureFromFile(texFilePath, _dev);

	if (sphFileName != "")
	{
		auto sphFilePath = GetTexturePathFromModelAndTexPath(strModelPath, sphFileName.c_str());
		sphResources[i] = LoadTextureFromFile(sphFilePath, _dev);
	}
}


auto materialBuffSize = sizeof(MaterialForHlsl);
materialBuffSize = (materialBuffSize + 0xff) & ~0xff;

ID3D12Resource* materialBuff = nullptr;

const D3D12_HEAP_PROPERTIES heapPropMat =
CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
const D3D12_RESOURCE_DESC resDescMat = CD3DX12_RESOURCE_DESC::Buffer(
	materialBuffSize * materialNum);
result = _dev->CreateCommittedResource(
	&heapPropMat,
	D3D12_HEAP_FLAG_NONE,
	&resDescMat,
	D3D12_RESOURCE_STATE_GENERIC_READ,
	nullptr,
	IID_PPV_ARGS(&materialBuff)
);

char* mapMaterial = nullptr;
result = materialBuff->Map(0, nullptr, (void**)&mapMaterial);
for (auto& m : materials) {
	*((MaterialForHlsl*)mapMaterial) = m.material;
	mapMaterial += materialBuffSize;
}
materialBuff->Unmap(0, nullptr);

ID3D12DescriptorHeap* materialDescHeap = nullptr;

D3D12_DESCRIPTOR_HEAP_DESC matDescHeapDesc = {};
matDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
matDescHeapDesc.NodeMask = 0;
matDescHeapDesc.NumDescriptors = materialNum * 3;
matDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

result = _dev->CreateDescriptorHeap(
	&matDescHeapDesc, IID_PPV_ARGS(&materialDescHeap));

D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};

matCBVDesc.BufferLocation =
materialBuff->GetGPUVirtualAddress();
matCBVDesc.SizeInBytes =
static_cast<UINT>(materialBuffSize);

D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
srvDesc.Shader4ComponentMapping =
D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
srvDesc.Texture2D.MipLevels = 1;

auto matDescHeapH = materialDescHeap->GetCPUDescriptorHandleForHeapStart();
UINT incSize = _dev->GetDescriptorHandleIncrementSize(
	D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

for (UINT i = 0; i < materialNum; ++i)
{
	_dev->CreateConstantBufferView(&matCBVDesc, matDescHeapH);
	matDescHeapH.ptr += incSize;
	matCBVDesc.BufferLocation += materialBuffSize;

	if (textureResources[i] == nullptr)
	{
		srvDesc.Format = whiteTex->GetDesc().Format;
		_dev->CreateShaderResourceView(
			whiteTex, &srvDesc, matDescHeapH);
	}
	else
	{
		srvDesc.Format = textureResources[i]->GetDesc().Format;
		_dev->CreateShaderResourceView(
			textureResources[i],
			&srvDesc,
			matDescHeapH);
	}

	matDescHeapH.ptr += incSize;

	if (sphResources[i] == nullptr)
	{
		srvDesc.Format = whiteTex->GetDesc().Format;
		_dev->CreateShaderResourceView(
			whiteTex, &srvDesc, matDescHeapH);
	}
	else
	{
		srvDesc.Format = sphResources[i]->GetDesc().Format;
		_dev->CreateShaderResourceView(
			sphResources[i], &srvDesc, matDescHeapH);
	}
	matDescHeapH.ptr += incSize;
}

fclose(fp);

ID3DBlob* _vsBlob = nullptr;
ID3DBlob* _psBlob = nullptr;

ID3DBlob* errorBlob = nullptr;
result = D3DCompileFromFile(
	L"BasicVertexShader.hlsl",
	nullptr,
	D3D_COMPILE_STANDARD_FILE_INCLUDE,
	"BasicVS", "vs_5_0",
	D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
	0,
	&_vsBlob, &errorBlob);

if (FAILED(result))
{
	if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
	{
		::OutputDebugStringA("ファイルが見当たりません");
	}
	else
	{
		std::string errstr;
		errstr.resize(errorBlob->GetBufferSize());
		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(), errstr.begin());
		errstr += "\n";
		OutputDebugStringA(errstr.c_str());
	}
	exit(1);
}

result = D3DCompileFromFile(
	L"BasicPixelShader.hlsl",
	nullptr,
	D3D_COMPILE_STANDARD_FILE_INCLUDE,
	"BasicPS", "ps_5_0",
	D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
	0,
	&_psBlob, &errorBlob);

if (FAILED(result))
{
	if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
	{
		::OutputDebugStringA("ファイルが見当たりません");
	}
	else
	{
		std::string errstr;
		errstr.resize(errorBlob->GetBufferSize());
		std::copy_n((char*)errorBlob->GetBufferPointer(),
			errorBlob->GetBufferSize(), errstr.begin());
		errstr += "\n";
		OutputDebugStringA(errstr.c_str());
		OutputDebugStringA(errstr.c_str());
	}
	exit(1);
}

D3D12_INPUT_ELEMENT_DESC inputLayout[] =
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BONE_NO", 0, DXGI_FORMAT_R16G16_UINT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHT", 0, DXGI_FORMAT_R8_UINT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "EDGE_FLG", 0, DXGI_FORMAT_R8_UINT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};

gpipeline.pRootSignature = nullptr;

gpipeline.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
gpipeline.VS.BytecodeLength = _vsBlob->GetBufferSize();
gpipeline.PS.pShaderBytecode = _psBlob->GetBufferPointer();
gpipeline.PS.BytecodeLength = _psBlob->GetBufferSize();
gpipeline.InputLayout.pInputElementDescs = inputLayout;
gpipeline.InputLayout.NumElements = _countof(inputLayout);

gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

gpipeline.RasterizerState.MultisampleEnable = false;

gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
gpipeline.RasterizerState.DepthClipEnable = true;

gpipeline.DepthStencilState.DepthEnable = true;
gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;

gpipeline.BlendState.AlphaToCoverageEnable = false;
gpipeline.BlendState.IndependentBlendEnable = false;

D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};

renderTargetBlendDesc.BlendEnable = false;

renderTargetBlendDesc.LogicOpEnable = false;
renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

gpipeline.InputLayout.pInputElementDescs = inputLayout;
gpipeline.InputLayout.NumElements = _countof(inputLayout);

gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

gpipeline.NumRenderTargets = 1;
gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

gpipeline.SampleDesc.Count = 1;
gpipeline.SampleDesc.Quality = 0;

D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

D3D12_DESCRIPTOR_RANGE descTblRange[3] = {};
descTblRange[0].NumDescriptors = 1;
descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
descTblRange[0].BaseShaderRegister = 0;
descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

descTblRange[1].NumDescriptors = 1;
descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
descTblRange[1].BaseShaderRegister = 1;
descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

descTblRange[2].NumDescriptors = 2;
descTblRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
descTblRange[2].BaseShaderRegister = 0;
descTblRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

D3D12_ROOT_PARAMETER rootparam[2] = {};
rootparam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
rootparam[0].DescriptorTable.pDescriptorRanges = descTblRange;
rootparam[0].DescriptorTable.NumDescriptorRanges = 1;
rootparam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

rootparam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
rootparam[1].DescriptorTable.pDescriptorRanges = &descTblRange[1];
rootparam[1].DescriptorTable.NumDescriptorRanges = 2;
rootparam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

rootSignatureDesc.pParameters = rootparam;
rootSignatureDesc.NumParameters = 2;

D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
samplerDesc.MinLOD = 0.0f;
samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

rootSignatureDesc.pStaticSamplers = &samplerDesc;
rootSignatureDesc.NumStaticSamplers = 1;

ID3DBlob* rootSigBlob = nullptr;
result = D3D12SerializeRootSignature(
	&rootSignatureDesc,
	D3D_ROOT_SIGNATURE_VERSION_1_0,
	&rootSigBlob,
	&errorBlob);

ID3D12RootSignature* rootsignature = nullptr;
result = _dev->CreateRootSignature(
	0,
	rootSigBlob->GetBufferPointer(),
	rootSigBlob->GetBufferSize(),
	IID_PPV_ARGS(&rootsignature));
rootSigBlob->Release();

gpipeline.pRootSignature = rootsignature;

ID3D12PipelineState* _pipelinestate = nullptr;
result = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&_pipelinestate));

D3D12_VIEWPORT viewport = {};
viewport.Width = window_width;
viewport.Height = window_height;
viewport.TopLeftX = 0;
viewport.TopLeftY = 0;
viewport.MaxDepth = 1.0f;
viewport.MinDepth = 0.0f;

D3D12_RECT scissorrect = {};
scissorrect.top = 0;
scissorrect.left = 0;
scissorrect.right = scissorrect.left + window_width;
scissorrect.bottom = scissorrect.top + window_height;

TexMetadata metadata = {};
ScratchImage scratchImg = {};

result = LoadFromWICFile(
	L"img/textest.png", WIC_FLAGS_NONE,
	&metadata, scratchImg);

auto img = scratchImg.GetImage(0, 0, 0);

D3D12_HEAP_PROPERTIES uploadHeapProp = {};

uploadHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;

uploadHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
uploadHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

uploadHeapProp.CreationNodeMask = 0;
uploadHeapProp.VisibleNodeMask = 0;

D3D12_RESOURCE_DESC resDesc = {};

resDesc.Format = DXGI_FORMAT_UNKNOWN;
resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;

resDesc.Width = AlignmentedSize(img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)
* img->height;
resDesc.Height = 1;
resDesc.DepthOrArraySize = 1;
resDesc.MipLevels = 1;

resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

resDesc.SampleDesc.Count = 1;
resDesc.SampleDesc.Quality = 0;

ID3D12Resource* uploadbuff = nullptr;

result = _dev->CreateCommittedResource(
	&uploadHeapProp,
	D3D12_HEAP_FLAG_NONE,
	&resDesc,
	D3D12_RESOURCE_STATE_GENERIC_READ,
	nullptr,
	IID_PPV_ARGS(&uploadbuff)
);

D3D12_HEAP_PROPERTIES texHeapProp = {};

texHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
texHeapProp.CreationNodeMask = 0;
texHeapProp.VisibleNodeMask = 0;

resDesc.Format = metadata.format;
resDesc.Width = static_cast<UINT>(metadata.width);
resDesc.Height = static_cast<UINT>(metadata.height);
resDesc.DepthOrArraySize = static_cast<uint16_t>(metadata.arraySize);
resDesc.MipLevels = static_cast<uint16_t>(metadata.mipLevels);
resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);
resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

ID3D12Resource* texbuff = nullptr;
result = _dev->CreateCommittedResource(
	&texHeapProp,
	D3D12_HEAP_FLAG_NONE,
	&resDesc,
	D3D12_RESOURCE_STATE_COPY_DEST,
	nullptr,
	IID_PPV_ARGS(&texbuff));

result = texbuff->WriteToSubresource(
	0,
	nullptr,
	img->pixels,
	static_cast<UINT>(img->rowPitch),
	static_cast<UINT>(img->slicePitch)
);

uint8_t* mapforImg = nullptr;
result = uploadbuff->Map(0, nullptr, (void**)&mapforImg);

auto srcAddress = img->pixels;

auto rowPitch = AlignmentedSize(img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

for (int y = 0; y < img->height; ++y)
{
	std::copy_n(srcAddress,
		rowPitch,
		mapforImg);


	srcAddress += img->rowPitch;
	mapforImg += rowPitch;
}


uploadbuff->Unmap(0, nullptr);

D3D12_TEXTURE_COPY_LOCATION src = {};

src.pResource = uploadbuff;
src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
src.PlacedFootprint.Offset = 0;
src.PlacedFootprint.Footprint.Width = static_cast<UINT>(metadata.width);
src.PlacedFootprint.Footprint.Height = static_cast<UINT>(metadata.height);
src.PlacedFootprint.Footprint.Depth = static_cast<UINT>(metadata.depth);
src.PlacedFootprint.Footprint.RowPitch = AlignmentedSize(img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
src.PlacedFootprint.Footprint.Format = img->format;

D3D12_TEXTURE_COPY_LOCATION dst = {};

dst.pResource = texbuff;
dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
dst.SubresourceIndex = 0;

_cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

D3D12_RESOURCE_BARRIER BarrierDesc = {};
BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
BarrierDesc.Transition.pResource = texbuff;
BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
BarrierDesc.Transition.StateBefore =
D3D12_RESOURCE_STATE_COPY_DEST;
BarrierDesc.Transition.StateAfter =
D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

_cmdList->ResourceBarrier(1, &BarrierDesc);
_cmdList->Close();

ID3D12CommandList* cmdlists[] = { _cmdList };
_cmdQueue->ExecuteCommandLists(1, cmdlists);

_cmdQueue->Signal(_fence, ++_fenceVal);

if (_fence->GetCompletedValue() != _fenceVal)
{
	auto event = CreateEvent(nullptr, false, false, nullptr);
	_fence->SetEventOnCompletion(_fenceVal, event);
	WaitForSingleObject(event, INFINITE);
	CloseHandle(event);
}
_cmdAllocator->Reset();
_cmdList->Reset(_cmdAllocator, nullptr);


struct MatricesData
{
	XMMATRIX world; // モデル本体を回転させたり移動させたりする行列
	XMMATRIX viewproj; // ビューとプロジェクション合成行列
};

XMMATRIX worldMat = XMMatrixRotationY(XM_PIDIV4);

//matrix.r[0].m128_f32[0] = +2.0f / window_width;
//matrix.r[1].m128_f32[1] = -2.0f / window_height;
//
//matrix.r[3].m128_f32[0] = -1.0f;
//matrix.r[3].m128_f32[1] = +1.0f;

XMFLOAT3 eye(0, 10, -15);
XMFLOAT3 target(0, 10, 0);
XMFLOAT3 up(0, 1, 0);

auto viewMat = XMMatrixLookAtLH(
	XMLoadFloat3(&eye), XMLoadFloat3(&target), XMLoadFloat3(&up));

auto projMat = XMMatrixPerspectiveFovLH(
	XM_PIDIV2,
	static_cast<float>(window_width)
	/ static_cast<float>(window_height),
	1.0f,
	100.0f
);

ID3D12Resource* constBuff = nullptr;
auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
resDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(MatricesData) + 0xff) & ~0xff);
_dev->CreateCommittedResource(
	&heapProp,
	D3D12_HEAP_FLAG_NONE,
	&resDesc,
	D3D12_RESOURCE_STATE_GENERIC_READ,
	nullptr,
	IID_PPV_ARGS(&constBuff)
);

MatricesData* mapMatrix;
result = constBuff->Map(0, nullptr, (void**)&mapMatrix);
//*mapMatrix = matrix;
mapMatrix->world = worldMat;
mapMatrix->viewproj = viewMat * projMat;

ID3D12DescriptorHeap* basicDescHeap = nullptr;
D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
descHeapDesc.NodeMask = 0;
descHeapDesc.NumDescriptors = 1;
descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
result = _dev->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&basicDescHeap));

auto basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();

D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
cbvDesc.SizeInBytes = static_cast<UINT>(constBuff->GetDesc().Width);

_dev->CreateConstantBufferView(&cbvDesc, basicHeapHandle);

float angle = 0.0f;

while (true)
{
	MSG msg;

	if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			break;
		}

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	
	angle += 0.1f;
	worldMat = XMMatrixRotationY(angle);
	mapMatrix->world = worldMat;
	mapMatrix->viewproj = viewMat * projMat;

	auto bbIdx = _swapchain->GetCurrentBackBufferIndex();

	auto BarrierDesc = CD3DX12_RESOURCE_BARRIER::Transition(
		_backBuffers[bbIdx], D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	_cmdList->ResourceBarrier(1, &BarrierDesc);

	_cmdList->SetPipelineState(_pipelinestate);

	auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
	rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	auto dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();
	_cmdList->OMSetRenderTargets(1, &rtvH, true, &dsvH);

	float clearColor[] = { 1.0f,1.0f,1.0f,1.0f };

	_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
	_cmdList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	_cmdList->ResourceBarrier(1, &BarrierDesc);

	_cmdList->SetGraphicsRootSignature(rootsignature);
	_cmdList->SetDescriptorHeaps(1, &basicDescHeap);

	_cmdList->SetGraphicsRootDescriptorTable(0,
		basicDescHeap->GetGPUDescriptorHandleForHeapStart());

	_cmdList->SetDescriptorHeaps(1, &materialDescHeap);
	_cmdList->SetGraphicsRootDescriptorTable(
		1,
		materialDescHeap->GetGPUDescriptorHandleForHeapStart());

	_cmdList->RSSetViewports(1, &viewport);
	_cmdList->RSSetScissorRects(1, &scissorrect);

	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	_cmdList->IASetVertexBuffers(0, 1, &vbView);
	_cmdList->IASetIndexBuffer(&ibView);

	_cmdList->SetDescriptorHeaps(1, &materialDescHeap);
	auto materialH = materialDescHeap->
		GetGPUDescriptorHandleForHeapStart();

	unsigned int idxOffset = 0;

	UINT cbvsrvIncSize = _dev->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 3;

	for (auto& m : materials)
	{
		_cmdList->SetGraphicsRootDescriptorTable(1, materialH);

		_cmdList->DrawIndexedInstanced(
			m.indicesNum, 1, idxOffset, 0, 0);

		materialH.ptr += cbvsrvIncSize;
		idxOffset += m.indicesNum;
	}

	BarrierDesc = CD3DX12_RESOURCE_BARRIER::Transition(
		_backBuffers[bbIdx], D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);
	_cmdList->ResourceBarrier(1, &BarrierDesc);

	_cmdList->DrawIndexedInstanced(indicesNum, 1, 0, 0, 0);

	_cmdList->Close();

	ID3D12CommandList* cmdlists[] = { _cmdList };
	_cmdQueue->ExecuteCommandLists(1, cmdlists);

	_cmdQueue->Signal(_fence, ++_fenceVal);

	if (_fence->GetCompletedValue() != _fenceVal)
	{
		auto event = CreateEvent(nullptr, false, false, nullptr);
		_fence->SetEventOnCompletion(_fenceVal, event);
		WaitForSingleObject(event, INFINITE);
		CloseHandle(event);
	}

	_cmdAllocator->Reset();
	_cmdList->Reset(_cmdAllocator, nullptr);

	_swapchain->Present(1, 0);
}

UnregisterClass(w.lpszClassName, w.hInstance);

return 0;
}