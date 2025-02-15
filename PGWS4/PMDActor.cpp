#include "PMDActor.h"
#include "PMDRenderer.h"
#include "Dx12Wrapper.h"
#include <d3dx12.h>
#include <algorithm>


#pragma comment(lib,"winmm.lib")

using namespace DirectX;
using namespace Microsoft::WRL;

static inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw std::exception();
	}
}

static std::string GetTexturePathFromModelAndTexPath(
	const std::string& modelPath,
	const char* texPath)
{
	int pathIndex1 = static_cast<int>(modelPath.rfind('/'));
	int pathIndex2 = static_cast<int>(modelPath.rfind('\\'));
	int pathIndex = max(pathIndex1, pathIndex2);
	std::string folderPath = modelPath.substr(0, pathIndex + 1);
	return folderPath + texPath;
}

static std::string GetExtension(const std::string& path)
{
	int idx = static_cast<int>(path.rfind('.'));
	return path.substr(
		idx + 1, path.length() - idx - 1);
}

static std::pair<std::string, std::string> SplitFileName(
	const std::string& path, const char splitter = '*')
{
	int idx = static_cast<int>(path.find(splitter));
	std::pair<std::string, std::string> ret;
	ret.first = path.substr(0, idx);
	ret.second = path.substr(
		idx + 1, path.length() - idx - 1);
	return ret;
}

HRESULT PMDActor::LoadPMDFile(const char* path)
{
	struct PMDHeader
	{
		float version;
		char model_name[20];
		char comment[256];
	};

	char signature[3] = {};
	PMDHeader pmdheader = {};

	std::string strModelPath = path;
	FILE* fp;
	fopen_s(&fp, strModelPath.c_str(), "rb");

	if (fp == nullptr)
	{
		assert(0);	return ERROR_FILE_NOT_FOUND;
	}

	fread(signature, sizeof(signature), 1, fp);
	fread(&pmdheader, sizeof(pmdheader), 1, fp);

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

	constexpr unsigned int pmdvertex_size = 38;
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

	auto heapprop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resdesc = CD3DX12_RESOURCE_DESC::Buffer(vertices.size() * sizeof(PMD_VERTEX));
	ComPtr<ID3D12Device> dev = _dx12.Device();

	ThrowIfFailed(_dx12.Device()->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_vertBuff.ReleaseAndGetAddressOf())));

	PMD_VERTEX* vertMap = nullptr;
	ThrowIfFailed(_vertBuff->Map(0, nullptr, (void**)&vertMap));
	std::copy(std::begin(vertices), std::end(vertices), vertMap);
	_vertBuff->Unmap(0, nullptr);

	_vbView.BufferLocation = _vertBuff->GetGPUVirtualAddress();
	_vbView.SizeInBytes = static_cast<UINT>(vertices.size() * sizeof(PMD_VERTEX));
	_vbView.StrideInBytes = sizeof(vertices[0]);

	unsigned int indicesNum;
	fread(&indicesNum, sizeof(indicesNum), 1, fp);

	std::vector<unsigned short> indices;
	indices.resize(indicesNum);
	fread(indices.data(), indices.size() * sizeof(indices[0]), 1, fp);

	resdesc.Width = indices.size() * sizeof(indices[0]);
	ThrowIfFailed(dev->CreateCommittedResource(
		&heapprop,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_idxBuff.ReleaseAndGetAddressOf())));

	unsigned short* mappedIdx = nullptr;
	_idxBuff->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(std::begin(indices), std::end(indices), mappedIdx);
	_idxBuff->Unmap(0, nullptr);

	_ibView.BufferLocation = _idxBuff->GetGPUVirtualAddress();
	_ibView.Format = DXGI_FORMAT_R16_UINT;
	_ibView.SizeInBytes = static_cast<UINT>(indices.size() * sizeof(indices[0]));

	fread(&_materialNum, sizeof(_materialNum), 1, fp);

	materials.resize(_materialNum);
	_textureResources.resize(_materialNum);
	_sphResources.resize(_materialNum);
	_spaResources.resize(_materialNum);
	_toonResources.resize(_materialNum);

	std::vector<PMDMaterial> pmdMaterials(_materialNum);
	fread(
		pmdMaterials.data(),
		pmdMaterials.size() * sizeof(PMDMaterial),
		1,
		fp);

	for (int i = 0; i < pmdMaterials.size(); ++i)
	{
		materials[i].indicesNum = pmdMaterials[i].indicesNum;
		materials[i].material.diffuse = pmdMaterials[i].diffuse;
		materials[i].material.alpha = pmdMaterials[i].alpha;
		materials[i].material.specular = pmdMaterials[i].specular;
		materials[i].material.specularity = pmdMaterials[i].specularity;
		materials[i].material.ambient = pmdMaterials[i].ambient;
		materials[i].additional.toonIdx = pmdMaterials[i].toonIdx;
	}

	for (int i = 0; i < pmdMaterials.size(); ++i)
	{
		std::string toonFilePath = "toon/";
		char toonFileName[16];
		sprintf_s(toonFileName, "toon%02d.bmp", pmdMaterials[i].toonIdx + 1);
		toonFilePath += toonFileName;
		_toonResources[i] = _dx12.GetTextureByPath(toonFilePath.c_str());


		if (strlen(pmdMaterials[i].texFilePath) == 0) { continue; }

		std::string texFileName = pmdMaterials[i].texFilePath;
		std::string sphFileName = "";
		std::string spaFileName = "";

		if (std::count(texFileName.begin(), texFileName.end(), '*') > 0)
		{
			auto namepair = SplitFileName(texFileName);

			if (GetExtension(namepair.first) == "sph")
			{
				texFileName = namepair.second;
				sphFileName = namepair.first;
			}
			else if (GetExtension(namepair.first) == "spa")
			{
				texFileName = namepair.second;
				spaFileName = namepair.first;
			}
			else
			{
				texFileName = namepair.first;
				if (GetExtension(namepair.second) == "sph")
				{
					sphFileName = namepair.second;
				}
				else if (GetExtension(namepair.second) == "spa")
				{
					spaFileName = namepair.second;
				}
			}
		}
		else
		{
			std::string ext = GetExtension(pmdMaterials[i].texFilePath);
			if (ext == "sph")
			{
				sphFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			}
			else if (ext == "spa")
			{
				spaFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			}
			else
			{
				texFileName = pmdMaterials[i].texFilePath;
			}
		}

		auto texFilePath = GetTexturePathFromModelAndTexPath(
			strModelPath, texFileName.c_str());
		_textureResources[i] = _dx12.GetTextureByPath(texFilePath.c_str());

		if (sphFileName != "")
		{
			auto sphFilePath = GetTexturePathFromModelAndTexPath(strModelPath, sphFileName.c_str());
			_sphResources[i] = _dx12.GetTextureByPath(sphFilePath.c_str());
		}

		if (spaFileName != "")
		{
			auto spaFilePath = GetTexturePathFromModelAndTexPath(strModelPath, spaFileName.c_str());
			_spaResources[i] = _dx12.GetTextureByPath(spaFilePath.c_str());
		}

	}

	//骨の数の読み込み
	unsigned short boneNum = 0;

	//ファイルfpからboneNumバイトのデータを1個読み込み、&boneNumに格納する
	fread(&boneNum, sizeof(boneNum), 1, fp);
	//&boneNum:読み込みデータ格納先のポインタ
	//size_t sizeof(boneNum):読み込みデータのバイト長さ
	//size_t 1:読み込みデータの数
	//FILE* fp:FILEポインタ

	//パディングが発生するため、pragmaで囲む
#pragma pack(1)

	//読み込み用ボーン構造体
	//PMDファイルからPMDBoneの配列(pmdBones)を取得
	struct PMDBone 
	{
		char boneName[20];		//ボーン名(個々のボーンデータ)
		unsigned short parentNo;//親ボーン番号
		unsigned short nextNo;	//先端のボーン番号
		unsigned char type;		//ボーン種別
		unsigned short ikBoneNo;//IKボーン番号
		XMFLOAT3 pos;			//ボーンの基準点座標
	};
#pragma pack()

	std::vector<PMDBone> pmdBones(boneNum);
	fread(pmdBones.data(), sizeof(PMDBone), boneNum, fp);//PMDBoneの配列(pmdBones)を取得完了

	fclose(fp);

	//↓PMDBones配列からBoneNodeテーブル(_boneNodeTable)へと変換↓
	//インデックスと名前の対応関係構築のために後で使う
	std::vector<std::string> boneNames(pmdBones.size());

	//ボーンノードマップを作る
	for (int idx = 0; idx < pmdBones.size(); ++idx)
	{
		PMDBone& pb = pmdBones[idx];
		boneNames[idx] = pb.boneName;
		BoneNode& node = _boneNodeTable[pb.boneName];
		node.boneIdx = idx;
		node.startPos = pb.pos;
	}

	//親子関係を構築する
	for (PMDBone& pb : pmdBones)
	{
		//親インデックスをチェック(あり得ない番号なら飛ばす)
		if (pmdBones.size() <= pb.parentNo)
		{
			continue;
		}
		std::string& parentName = boneNames[pb.parentNo];
		_boneNodeTable[parentName].children.
			emplace_back(&_boneNodeTable[pb.boneName]);
	}
	//↑

	//ボーンをすべて初期化する。
	//XMMATRIX配列の中身を単位行列XMMatrixIdentity()で初期化
	_boneMatrices.resize(pmdBones.size());

	//_boneMatricesをXMMatrixIdentity()の値で埋める(ようするに初期化)
	std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());

	return S_OK;
}

void PMDActor::CreateMaterialData()
{
	auto materialBuffSize = (sizeof(MaterialForHlsl) + 0xff) & ~0xff;
	const D3D12_HEAP_PROPERTIES heapPropMat = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	const D3D12_RESOURCE_DESC resDescMat = CD3DX12_RESOURCE_DESC::Buffer(
		materialBuffSize * _materialNum);
	auto _dev = _dx12.Device();
	ThrowIfFailed(_dev->CreateCommittedResource(
		&heapPropMat,
		D3D12_HEAP_FLAG_NONE,
		&resDescMat,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_materialBuff.ReleaseAndGetAddressOf())));

	char* mapMaterial = nullptr;
	ThrowIfFailed(_materialBuff->Map(0, nullptr, (void**)&mapMaterial));
	for (auto& m : materials)
	{
		*((MaterialForHlsl*)mapMaterial) = m.material;
		mapMaterial += materialBuffSize;
	}
	_materialBuff->Unmap(0, nullptr);
}

void PMDActor::CreateMaterialAndTextureView()
{
	ID3D12Device* _dev = _dx12.Device().Get();

	D3D12_DESCRIPTOR_HEAP_DESC materialDescHeapDesc = {};
	materialDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	materialDescHeapDesc.NodeMask = 0;
	materialDescHeapDesc.NumDescriptors = _materialNum * 5;
	materialDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	ThrowIfFailed(_dev->CreateDescriptorHeap(
		&materialDescHeapDesc, IID_PPV_ARGS(_materialDescHeap.ReleaseAndGetAddressOf())));

	UINT materialBuffSize = (sizeof(MaterialForHlsl) + 0xff) & ~0xff;
	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = _materialBuff->GetGPUVirtualAddress();
	matCBVDesc.SizeInBytes = static_cast<UINT>(materialBuffSize);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	srvDesc.Shader4ComponentMapping =
		D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension =
		D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	CD3DX12_CPU_DESCRIPTOR_HANDLE matDescHeapH(
		_materialDescHeap->GetCPUDescriptorHandleForHeapStart());
	UINT incSize = _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	for (UINT i = 0; i < _materialNum; ++i)
	{
		_dev->CreateConstantBufferView(&matCBVDesc, matDescHeapH);
		matDescHeapH.ptr += incSize;
		matCBVDesc.BufferLocation += static_cast<UINT>(materialBuffSize);

		if (_textureResources[i] == nullptr)
		{
			srvDesc.Format = _renderer._whiteTex->GetDesc().Format;
			_dev->CreateShaderResourceView(
				_renderer._whiteTex.Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = _textureResources[i]->GetDesc().Format;
			_dev->CreateShaderResourceView(
				_textureResources[i].Get(),
				&srvDesc,
				matDescHeapH);
		}

		matDescHeapH.ptr += incSize;

		if (_sphResources[i] == nullptr)
		{
			srvDesc.Format = _renderer._whiteTex->GetDesc().Format;
			_dev->CreateShaderResourceView(
				_renderer._whiteTex.Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = _sphResources[i]->GetDesc().Format;
			_dev->CreateShaderResourceView(
				_sphResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;

		if (_spaResources[i] == nullptr)
		{
			srvDesc.Format = _renderer._blackTex->GetDesc().Format;
			_dev->CreateShaderResourceView(
				_renderer._blackTex.Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = _spaResources[i]->GetDesc().Format;
			_dev->CreateShaderResourceView(
				_spaResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;

		if (_toonResources[i] == nullptr)
		{
			srvDesc.Format = _renderer._gradTex->GetDesc().Format;
			_dev->CreateShaderResourceView(_renderer._gradTex.Get(), &srvDesc, matDescHeapH);
		}
		else
		{
			srvDesc.Format = _toonResources[i]->GetDesc().Format;
			_dev->CreateShaderResourceView(_toonResources[i].Get(), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += incSize;
	}
}

void PMDActor::CreateTransformView()
{
	//GPUバッファ作成
	unsigned int buffSize = static_cast <UINT>(sizeof(XMMATRIX) * (1 + _boneMatrices.size()));
	buffSize = (buffSize + 0xff) & ~0xff;
	CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC resDesc = CD3DX12_RESOURCE_DESC::Buffer(buffSize);

	ThrowIfFailed(_dx12.Device()->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(_transformBuff.ReleaseAndGetAddressOf())
	));

	//マップとコピー
	ThrowIfFailed(_transformBuff->Map(0, nullptr, (void**)&_mappedMatrices));

	_mappedMatrices[0] = _transform.world;
	std::copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);

	D3D12_DESCRIPTOR_HEAP_DESC transformDescHeapDesc = {};
	transformDescHeapDesc.NumDescriptors = 1;
	transformDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	transformDescHeapDesc.NodeMask = 0;

	transformDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	ThrowIfFailed(_dx12.Device()->CreateDescriptorHeap(&transformDescHeapDesc, IID_PPV_ARGS(_transformHeap.ReleaseAndGetAddressOf())));

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _transformBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = buffSize;
	_dx12.Device()->CreateConstantBufferView(&cbvDesc, _transformHeap->GetCPUDescriptorHandleForHeapStart());
}

void* PMDActor::Transform::operator new(size_t size)
{
	return _aligned_malloc(size, 16);
}

PMDActor::PMDActor(const char* filepath, PMDRenderer& renderer) :
	_renderer(renderer),
	_dx12(renderer._dx12),
	_angle(0.0f)
{
	ThrowIfFailed(LoadPMDFile(filepath));

	CreateMaterialData();
	CreateMaterialAndTextureView();

	_transform.world = XMMatrixIdentity();
	CreateTransformView();
}

PMDActor::~PMDActor()
{

}

static float GetYFromXOnBezier(float x, const XMFLOAT2& a, const XMFLOAT2& b)
{
	if (a.x == a.y && b.x == b.y)return x;//計算不要

	float t = x;
	const float k0 = 1.0f + 3.0f * a.x - 3.0f * b.x;//t^3の係数
	const float k1 = 3.0f * b.x - 6.0f * a.x;		//t^2の係数
	const float k2 = 3.0f * a.x;					//tの係数

	constexpr float epsilon = 0.0005f;//誤差の範囲内かどうかに使用する定数
	constexpr uint8_t n = 12;//最大ループ回数

	for (int i = 0; i < n; ++i)
	{
		//f(t)を求める
		auto ft = k0 * t * t * t + k1 * t * t + k2 * t - x;

		//もし結果が0に近い(誤差の範囲内)なら打ち切り
		if (ft <= epsilon && ft >= -epsilon)break;

		t -= ft / 2.0f;
	}

	//既に求めたいtは求めているのでyを計算する
	float r = 1.0f - t;
	return t * t * t + 3 * t * t * r * b.y + 3 * t * r * r * a.y;
}

//読み込み関数(詳細)
void PMDActor::LoadVMDFile(const char* filepath, const char* name)
{
	FILE* fp;
	fopen_s(&fp, filepath, "rb");

	//SEEK_SET:ファイルの始め
	//ようするに、「ファイルの位置を、最初から50バイトの位置に変更するよ」ということになる
	fseek(fp, 50, SEEK_SET); //最初の50バイトは飛ばしてOK

	unsigned int keyframeNum = 0;

	//ファイルfpからkeyframeNumバイトのデータを1個読み込み、&keyframeNumに格納する
	fread(&keyframeNum, sizeof(keyframeNum), 1, fp);

	//ロード用構造体
	struct VMDKeyFrame
	{
		char boneName[15];		  //ボーン名
		unsigned int frameNo;	  //フレーム番号(読込時は現在のフレーム位置を0とした相対位置)
		XMFLOAT3 location;		  //位置
		XMFLOAT4 quaternion;	  //Quaternion(回転)
		unsigned char bezier[64]; //[4][4][4]　ベジェ補完パラメータ
	};
	std::vector<VMDKeyFrame> keyframes(keyframeNum);

	for (VMDKeyFrame& keyframe : keyframes)
	{
		fread(keyframe.boneName, sizeof(keyframe.boneName), 1, fp); //ボーン名
		fread(&keyframe.frameNo, sizeof(keyframe.frameNo) +			//フレーム番号
			sizeof(keyframe.location) +								//位置(IKのときに使用予定)
			sizeof(keyframe.quaternion) +							//クオータニオン
			sizeof(keyframe.bezier),								//補間ベジェデータ
			1, fp);
	}
	fclose(fp);

	//VMDのキーフレームデータから、実際に使用するキーフレームテーブルへ変換
	_duration = 0;
	for (VMDKeyFrame& f : keyframes)
	{
		_motiondata[f.boneName].emplace_back(
			KeyFrame(f.frameNo,
				XMLoadFloat4(&f.quaternion),
				XMFLOAT2((float)f.bezier[3] / 127.0f, (float)f.bezier[7] / 127.0f),
				XMFLOAT2((float)f.bezier[11] / 127.0f, (float)f.bezier[15] / 127.0f)));

		//総フレーム数の取得
		_duration = std::max<unsigned int>(_duration, f.frameNo);
	}

	for (auto& motion : _motiondata)
	{
		sort(motion.second.begin(), motion.second.end(),
			[](const KeyFrame& lval, const KeyFrame& rval) {
				return lval.frameNo <= rval.frameNo;
			});
	}

	for (auto& bonemotion : _motiondata)
	{
		BoneNode& node = _boneNodeTable[bonemotion.first];
		DirectX::XMFLOAT3& pos = node.startPos;
		DirectX::XMMATRIX mat =
			XMMatrixTranslation(-pos.x, -pos.y, -pos.z) *
			XMMatrixRotationQuaternion(bonemotion.second[0].quaternion) *
			XMMatrixTranslation(pos.x, pos.y, pos.z);
		_boneMatrices[node.boneIdx] = mat;
	}

	RecursiveMatrixMultipy(_boneNodeTable["センター"], XMMatrixIdentity());
	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
}

void PMDActor::PlayAnimation()
{
	_startTime = timeGetTime();
}

void PMDActor::MotionUpdate()
{
	DWORD elapsedTime = timeGetTime() - _startTime;//経過時間を測る
	unsigned int frameNo = (30 * elapsedTime / 1000);

	//ここからループのための追加コード
	if (frameNo > _duration)
	{
		_startTime = timeGetTime();
		frameNo = 0;
	}

	//行列情報クリア(してないと前フレームのポーズが重ね掛けされてモデルが壊れる)
	std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());

	//モーションデータ更新
	for (auto& bonemotion : _motiondata)
	{
		auto itBoneNode = _boneNodeTable.find(bonemotion.first);
		if (itBoneNode == _boneNodeTable.end()) continue;
		BoneNode& node = itBoneNode->second;

		//合致するものを探す
		auto& keyframes = bonemotion.second;

		auto rit = find_if(keyframes.rbegin(), keyframes.rend(),
			[frameNo](const KeyFrame& keyframe)
			{
				return keyframe.frameNo <= frameNo;
			});
		if (rit == keyframes.rend())continue;//合致するものがなければ飛ばす

		DirectX::XMMATRIX rotation;
		auto it = rit.base();

		if (it != keyframes.end())
		{
			auto t = static_cast<float>(frameNo - rit->frameNo) /
				static_cast<float>(it->frameNo - rit->frameNo);
			t = GetYFromXOnBezier(t, it->p1, it->p2);

			rotation = XMMatrixRotationQuaternion(
				XMQuaternionSlerp(rit->quaternion, it->quaternion, t));
		}
		else
		{
			rotation = XMMatrixRotationQuaternion(rit->quaternion);
		}

		DirectX::XMFLOAT3& pos = node.startPos;
		DirectX::XMMATRIX mat =
			XMMatrixTranslation(-pos.x, -pos.y, -pos.z) *	//原点に戻し
			rotation *										//回転
			XMMatrixTranslation(pos.x, pos.y, pos.z);		//元の座標に戻す
		_boneMatrices[node.boneIdx] = mat;
	}

	// 親の影響の反映
	RecursiveMatrixMultipy(_boneNodeTable["センター"], XMMatrixIdentity());
	copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
}

//再帰処理で末端まで回転を伝播させる(行列を乗算)
void PMDActor::RecursiveMatrixMultipy(BoneNode& node, const DirectX::XMMATRIX& mat)
{
	_boneMatrices[node.boneIdx] = mat;

	for (BoneNode* cnode : node.children) {
		RecursiveMatrixMultipy(*cnode, _boneMatrices[cnode->boneIdx] * mat);
	}
}

void PMDActor::Update()
{
	_angle += 0.00f;
	_mappedMatrices[0] = XMMatrixRotationY(_angle);

	MotionUpdate();
	//_angle += 0.03f;
	////_mappedMatricesの0番にワールド変換行列、1番以降にボーン行列が入っていると考える
	//_mappedMatrices[0] = XMMatrixRotationY(_angle);

	////行列情報クリア(してないと前フレームのポーズが重ね掛けされてモデルが壊れる)
	//std::fill(_boneMatrices.begin(), _boneMatrices.end(), XMMatrixIdentity());

	////左腕を90°曲げる
	//BoneNode& armNode = _boneNodeTable["左腕"];

	////XM_PIDIV2=π/2
	//DirectX::XMFLOAT3& armPos = armNode.startPos;
	//DirectX::XMMATRIX armMat =
	//	//腕のボーン基準点を原点へ戻すように平行移動させて
	//	XMMatrixTranslation(-armPos.x, -armPos.y, -armPos.z)
	//	//z軸を中に腕をπ/2回転させて
	//	* XMMatrixRotationZ(XM_PIDIV2)
	//	//腕のボーンを元のボーン基準点に戻すように平行移動させる
	//	* XMMatrixTranslation(armPos.x, armPos.y, armPos.z);

	////左ひじを-90°曲げる
	//BoneNode& elbowNode = _boneNodeTable["左ひじ"];
	//DirectX::XMFLOAT3& elbowPos = elbowNode.startPos;
	//DirectX::XMMATRIX elbowMat =
	//	XMMatrixTranslation(-elbowPos.x, -elbowPos.y, -elbowPos.z)
	//	* XMMatrixRotationZ(-XM_PIDIV2)
	//	* XMMatrixTranslation(elbowPos.x, elbowPos.y, elbowPos.z);

	//_boneMatrices[armNode.boneIdx] = armMat;
	//_boneMatrices[elbowNode.boneIdx] = elbowMat;

	////根から再帰処理して親の影響を伝搬させたのちにコピー
	//RecursiveMatrixMultipy(_boneNodeTable["センター"], XMMatrixIdentity());
	////_boneMatricesの最初から_boneMatricesの最後までを_mappedMatrices + 1にコピー
	////※再帰関数を呼び出した後でコピーをする
	//copy(_boneMatrices.begin(), _boneMatrices.end(), _mappedMatrices + 1);
}

void PMDActor::Draw()
{
	ID3D12GraphicsCommandList* cmdList = _dx12.CommandList().Get();

	ID3D12DescriptorHeap* transheaps[] = { _transformHeap.Get() };
	cmdList->SetDescriptorHeaps(1, transheaps);
	cmdList->SetGraphicsRootDescriptorTable(1, _transformHeap->GetGPUDescriptorHandleForHeapStart());

	cmdList->IASetVertexBuffers(0, 1, &_vbView);
	cmdList->IASetIndexBuffer(&_ibView);

	ID3D12DescriptorHeap* mdh[] = { _materialDescHeap.Get() };
	cmdList->SetDescriptorHeaps(1, mdh);

	D3D12_GPU_DESCRIPTOR_HANDLE materialH = _materialDescHeap->GetGPUDescriptorHandleForHeapStart(); // ヒープ先頭
	unsigned int idxOffset = 0;
	UINT cbvsrvIncSize = _dx12.Device()->GetDescriptorHandleIncrementSize(
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5;
	for (Material& m : materials)
	{
		cmdList->SetGraphicsRootDescriptorTable(2, materialH);
		cmdList->DrawIndexedInstanced(m.indicesNum, 1, idxOffset, 0, 0);
		materialH.ptr += cbvsrvIncSize;
		idxOffset += m.indicesNum;
	}
}