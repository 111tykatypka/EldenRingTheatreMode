#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <string>
#include <array>
#include <unordered_map>
namespace color_grading {
struct Settings {
    bool enabled=false;float exposure_ev=0,contrast=1,saturation=1,vibrance=0;
    float grain=0,grain_size=1,grain_speed=1,sharpen=0;
    float vignette=0,vignette_radius=.45f,vignette_softness=.5f;
    float aberration=0,distortion=0,lut_blend=0;
};
struct View {Settings settings;bool ready=false;std::string status="Disabled",lut_status="No LUT loaded",lut_path;};
Settings settings();View view();void configure(Settings value);void report(bool ready,const std::string& message);
void load_lut(const std::string& utf8_path);void unload_lut();
bool neutral(const Settings& value);
// Owned by the existing DX12 backend and released only after its GPU fence.
class Pass {
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    bool attempted_=false;
    DXGI_FORMAT format_=DXGI_FORMAT_UNKNOWN;
    struct LutGpu {
        Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
        std::uint64_t generation=~std::uint64_t(0);unsigned size=0;
        std::array<float,3> minimum{},range{1,1,1};
    };
    std::unordered_map<UINT,LutGpu> luts_;
public:
    bool prepare(ID3D12Device* device,DXGI_FORMAT format);
    // The caller must have completed this backbuffer slot's GPU fence before replacing its LUT.
    bool prepare_lut(ID3D12Device* device,UINT slot);
    void record(ID3D12GraphicsCommandList* list,D3D12_GPU_DESCRIPTOR_HANDLE source,UINT width,UINT height,const Settings& values,UINT slot);
    void reset();
};
}
