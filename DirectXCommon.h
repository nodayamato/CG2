#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <dxcapi.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <ostream>
#include <string>

#include "externals/DirectXTex/DirectXTex.h"

class WinApp;

class DirectXCommon {
public:
    // バックバッファ数
    static constexpr UINT kBackBufferCount = 2;

    // 初期化
    void Initialize(WinApp* winApp);

    // 終了処理
    void Finalize();

    // 描画前処理
    void PreDraw();

    // 描画後処理
    void PostDraw();

    // デバイス取得
    ID3D12Device* GetDevice() const {
        return device_.Get();
    }

    // コマンドキュー取得
    ID3D12CommandQueue* GetCommandQueue() const {
        return commandQueue_.Get();
    }

    // コマンドアロケータ取得
    ID3D12CommandAllocator* GetCommandAllocator() const {
        return commandAllocator_.Get();
    }

    // コマンドリスト取得
    ID3D12GraphicsCommandList* GetCommandList() const {
        return commandList_.Get();
    }

    // スワップチェーン取得
    IDXGISwapChain4* GetSwapChain() const {
        return swapChain_.Get();
    }

    // SRVヒープ取得
    ID3D12DescriptorHeap* GetSRVDescriptorHeap() const {
        return srvDescriptorHeap_.Get();
    }

    // RTVヒープ取得
    ID3D12DescriptorHeap* GetRTVDescriptorHeap() const {
        return rtvDescriptorHeap_.Get();
    }

    // DSVヒープ取得
    ID3D12DescriptorHeap* GetDSVDescriptorHeap() const {
        return dsvDescriptorHeap_.Get();
    }

    // 深度バッファ取得
    ID3D12Resource* GetDepthStencilResource() const {
        return depthStencilResource_.Get();
    }

    // SRVのディスクリプタサイズ
    uint32_t GetDescriptorSizeSRV() const {
        return descriptorSizeSRV_;
    }

    // RTVのディスクリプタサイズ
    uint32_t GetDescriptorSizeRTV() const {
        return descriptorSizeRTV_;
    }

    // DSVのディスクリプタサイズ
    uint32_t GetDescriptorSizeDSV() const {
        return descriptorSizeDSV_;
    }

    // SRVのCPUハンドル
    D3D12_CPU_DESCRIPTOR_HANDLE
        GetSRVCPUDescriptorHandle(uint32_t index) const;

    // SRVのGPUハンドル
    D3D12_GPU_DESCRIPTOR_HANDLE
        GetSRVGPUDescriptorHandle(uint32_t index) const;

    // シェーダーコンパイル
    Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
        const std::wstring& filePath,
        const wchar_t* profile
    );

    // --------------------------------------
    // リソース生成
    // --------------------------------------

    // バッファリソース生成
    Microsoft::WRL::ComPtr<ID3D12Resource>
        CreateBufferResource(size_t sizeInBytes);

    // テクスチャリソース生成
    Microsoft::WRL::ComPtr<ID3D12Resource>
        CreateTextureResource(
            const DirectX::TexMetadata& metadata
        );

    // テクスチャデータ転送
    // 返り値は転送用中間リソース
    [[nodiscard]]
    Microsoft::WRL::ComPtr<ID3D12Resource>
        UploadTextureData(
            const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
            const DirectX::ScratchImage& mipImages
        );

    // テクスチャファイル読み込み
    static DirectX::ScratchImage LoadTexture(
        const std::string& filePath
    );

    // DXCユーティリティ取得
    IDxcUtils* GetDxcUtils() const {
        return dxcUtils_.Get();
    }

    // DXCコンパイラ取得
    IDxcCompiler3* GetDxcCompiler() const {
        return dxcCompiler_.Get();
    }

    // IncludeHandler取得
    IDxcIncludeHandler* GetIncludeHandler() const {
        return includeHandler_.Get();
    }

private:
    // デバイス生成
    void InitializeDevice();

    // コマンド関連の初期化
    void InitializeCommand();

    // スワップチェーン生成
    void CreateSwapChain(HWND hwnd);

    // ディスクリプタヒープ生成
    void CreateDescriptorHeaps();

    // バックバッファとRTV生成
    void InitializeRenderTargetView();

    // 深度バッファ生成
    void CreateDepthBuffer();

    // DSV生成
    void InitializeDepthStencilView();

    // Fence生成
    void InitializeFence();

    // ビューポート初期化
    void InitializeViewport();

    // シザー矩形初期化
    void InitializeScissorRect();

    // FPS固定
    void InitializeFixFPS();
    void UpdateFixFPS();

    // DXC初期化
    void InitializeDXC();

    // ImGui初期化
    void InitializeImGui(HWND hwnd);

    // ImGui終了
    void FinalizeImGui();

private:
    // DXGIファクトリー
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;

    // GPUアダプター
    Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter_;

    // デバイス
    Microsoft::WRL::ComPtr<ID3D12Device> device_;

    // コマンド関連
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

    // スワップチェーン
    Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;

    // バックバッファ
    std::array<
        Microsoft::WRL::ComPtr<ID3D12Resource>,
        kBackBufferCount
    > swapChainResources_;

    // ディスクリプタヒープ
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;

    // 深度バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;

    // ディスクリプタサイズ
    uint32_t descriptorSizeRTV_ = 0;
    uint32_t descriptorSizeSRV_ = 0;
    uint32_t descriptorSizeDSV_ = 0;

    // RTVハンドル
    std::array<
        D3D12_CPU_DESCRIPTOR_HANDLE,
        kBackBufferCount
    > rtvHandles_{};

    // ビューポート
    D3D12_VIEWPORT viewport_{};

    // シザー矩形
    D3D12_RECT scissorRect_{};

    // Fence
    Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
    uint64_t fenceValue_ = 0;
    HANDLE fenceEvent_ = nullptr;

    // FPS固定用
    std::chrono::steady_clock::time_point reference_{};

    // ------------------
    // DXC関連
	// ------------------

    // DXCユーティリティ
    Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_;

    // DXCコンパイラ
    Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_;

    // IncludeHandler
    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;
};