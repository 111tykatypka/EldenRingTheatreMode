#include "ColorGrading.h"
#include <d3dcompiler.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <locale>
#include <vector>
#include <memory>
#include <thread>
#include <set>
#include <stdexcept>
#include <iomanip>
namespace color_grading { namespace {
struct Lut {unsigned size=0;std::array<float,3> minimum{},maximum{1,1,1};std::vector<std::array<float,4>> data;};
struct Store {std::mutex mutex;View state;std::shared_ptr<const Lut> lut;std::uint64_t generation=0,request=0;bool loading=false;};
// Adapter has process lifetime; worker completion must not race static destruction.
Store& store(){static auto* s=new Store;return *s;}
const char shader[]=R"HLSL(
Texture2D<float4> scene : register(t0);
ByteAddressBuffer lut : register(t1);
SamplerState clampLinear : register(s0);
cbuffer Grade : register(b0) {
 float4 basic; float4 textureFx; float4 lens; float4 viewport;
 float4 lutInfo; float4 domainMin; float4 domainRange;
};
float4 VS(uint id : SV_VertexID) : SV_Position {
    float2 p=id==0?float2(-1,1):(id==1?float2(3,1):float2(-1,-3));
    return float4(p,0,1);
}
float3 Decode(float3 v){return lerp(v/12.92,pow((v+0.055)/1.055,2.4),step(0.04045,v));}
float3 Encode(float3 v){return lerp(v*12.92,1.055*pow(v,1.0/2.4)-0.055,step(0.0031308,v));}
float4 Sample(float2 uv){return scene.SampleLevel(clampLinear,uv,0);}
float3 LutAt(uint3 i,uint n){return asfloat(lut.Load3((i.x+n*(i.y+n*i.z))*16));}
float3 ApplyLut(float3 c){
 uint n=(uint)lutInfo.y;float3 p=saturate((c-domainMin.xyz)/domainRange.xyz)*(n-1);
 uint3 a=(uint3)floor(p),b=min(a+1,n-1);float3 f=frac(p);
 return lerp(lerp(lerp(LutAt(a,n),LutAt(uint3(b.x,a.y,a.z),n),f.x),
                  lerp(LutAt(uint3(a.x,b.y,a.z),n),LutAt(uint3(b.x,b.y,a.z),n),f.x),f.y),
             lerp(lerp(LutAt(uint3(a.x,a.y,b.z),n),LutAt(uint3(b.x,a.y,b.z),n),f.x),
                  lerp(LutAt(uint3(a.x,b.y,b.z),n),LutAt(b,n),f.x),f.y),f.z);
}
float Hash(float3 p){p=frac(p*.1031);p+=dot(p,p.yzx+33.33);return frac((p.x+p.y)*p.z);}
float4 PS(float4 position : SV_Position) : SV_Target {
    float2 inv=1/viewport.xy,uv=position.xy*inv,center=uv-.5;
    float2 warped=.5+center*(1+viewport.w*dot(center*2,center*2));
    float4 original=Sample(warped);
    float3 src=original.rgb;
    if(lens.w>0){float2 shift=center*lens.w*inv*2;src.r=Sample(warped+shift).r;src.b=Sample(warped-shift).b;}
    float3 c=Decode(saturate(src));
    if(textureFx.w>0){
        float3 avg=(Decode(saturate(Sample(warped+float2(inv.x,0)).rgb))+Decode(saturate(Sample(warped-float2(inv.x,0)).rgb))+
                    Decode(saturate(Sample(warped+float2(0,inv.y)).rgb))+Decode(saturate(Sample(warped-float2(0,inv.y)).rgb)))*.25;
        c=max(c+(c-avg)*textureFx.w,0);
    }
    c*=exp2(basic.x);c=max((c-0.18)*basic.y+0.18,0);
    float l=dot(c,float3(0.2126,0.7152,0.0722));
    float hi=max(c.r,max(c.g,c.b)),lo=min(c.r,min(c.g,c.b));
    float chroma=(hi-lo)/max(hi,.00001);
    c=lerp(l.xxx,c,basic.z*(1+basic.w*(1-chroma)));
    float3 outColor=Encode(saturate(c));
    if(lutInfo.x>0&&lutInfo.y>=2)outColor=lerp(outColor,ApplyLut(outColor),lutInfo.x);
    if(lens.x>0){float radius=length(center*2)*.70710678;outColor*=1-lens.x*smoothstep(lens.y,lens.y+max(lens.z,.01),radius);}
    if(textureFx.x>0){float3 p=float3(floor(position.xy/max(textureFx.y,1)),floor(viewport.z*24*textureFx.z));
        float noise=(Hash(p)+Hash(p+17.17))*.5-.5;outColor+=noise*textureFx.x;}
    return float4(saturate(outColor),original.a);
}
)HLSL";
std::shared_ptr<Lut> parse_lut(const std::string& path,std::uint64_t request){
    const auto file=std::filesystem::u8path(path);
    // Bounded parsing prevents accidental huge files from monopolizing RAM. 256^3 is the Cube spec maximum.
    if(std::filesystem::file_size(file)>1024ULL*1024*1024)throw std::runtime_error("LUT exceeds 1 GiB parser budget");
    std::ifstream in(file);if(!in)throw std::runtime_error("Cannot open LUT file");
    auto result=std::make_shared<Lut>();std::string line;std::set<std::string> keys;bool data=false;
    std::size_t number=0,expected=0;
    while(std::getline(in,line)){
        ++number;if(line.size()>4096)throw std::runtime_error("LUT line too long");
        if((number&4095)==0){auto&s=store();std::lock_guard lock(s.mutex);if(request!=s.request)throw std::runtime_error("LUT load cancelled");}
        if(number==1&&line.compare(0,3,"\xef\xbb\xbf")==0)line.erase(0,3);
        auto first=line.find_first_not_of(" \t\r");if(first==std::string::npos||line[first]=='#')continue;
        std::istringstream row(line);row.imbue(std::locale::classic());std::string key;row>>key;
        auto fail=[&](const char* reason){throw std::runtime_error(std::string(reason)+" at line "+std::to_string(number));};
        if(key=="TITLE"||key=="DOMAIN_MIN"||key=="DOMAIN_MAX"||key=="LUT_3D_SIZE"){
            if(data||!keys.insert(key).second)fail("Duplicate or late header");
            if(key=="TITLE"){std::string title;if(!(row>>std::quoted(title)))fail("Invalid TITLE");}
            else if(key=="LUT_3D_SIZE"){
                if(!(row>>result->size)||result->size<2||result->size>256)fail("3D LUT size must be 2..256");
                expected=std::size_t(result->size)*result->size*result->size;
            } else {
                auto& v=key=="DOMAIN_MIN"?result->minimum:result->maximum;
                if(!(row>>v[0]>>v[1]>>v[2]))fail("Invalid input domain");
                for(float x:v)if(!std::isfinite(x))fail("Nonfinite input domain");
            }
            std::string extra;if(row>>extra)fail("Unexpected header data");continue;
        }
        if(key=="LUT_1D_SIZE"||key=="LUT_3D_INPUT_RANGE"||key=="LUT_1D_INPUT_RANGE")fail("Unsupported LUT variant; use 3D DOMAIN_MIN/MAX Cube");
        if(!expected)fail("Missing LUT_3D_SIZE");
        data=true;std::istringstream values(line);values.imbue(std::locale::classic());std::array<float,4> v{};
        if(!(values>>v[0]>>v[1]>>v[2]))fail("Invalid RGB row");
        for(float x:v)if(!std::isfinite(x)||std::abs(x)>1e37f)fail("Invalid RGB value");
        std::string extra;if(values>>extra)fail("Unexpected RGB data");
        if(result->data.size()>=expected)fail("Too many RGB rows");result->data.push_back(v);
    }
    if(in.bad())throw std::runtime_error("LUT read failed");
    if(!expected||result->data.size()!=expected)throw std::runtime_error("LUT row count does not match N cubed");
    for(unsigned i=0;i<3;++i){float range=result->maximum[i]-result->minimum[i];if(!std::isfinite(range)||range<=0)throw std::runtime_error("LUT input domain must have finite positive range");}
    return result;
}
}
Settings settings(){auto&s=store();std::lock_guard lock(s.mutex);return s.state.settings;}
View view(){auto&s=store();std::lock_guard lock(s.mutex);return s.state;}
void configure(Settings v){
    const float all[]={v.exposure_ev,v.contrast,v.saturation,v.vibrance,v.grain,v.grain_size,v.grain_speed,v.sharpen,v.vignette,v.vignette_radius,v.vignette_softness,v.aberration,v.distortion,v.lut_blend};
    for(float x:all)if(!std::isfinite(x))return;
    v.exposure_ev=std::clamp(v.exposure_ev,-5.f,5.f);v.contrast=std::clamp(v.contrast,0.f,2.f);v.saturation=std::clamp(v.saturation,0.f,2.f);
    v.vibrance=std::clamp(v.vibrance,-1.f,1.f);v.grain=std::clamp(v.grain,0.f,.25f);v.grain_size=std::clamp(v.grain_size,1.f,8.f);v.grain_speed=std::clamp(v.grain_speed,0.f,4.f);v.sharpen=std::clamp(v.sharpen,0.f,2.f);
    v.vignette=std::clamp(v.vignette,0.f,1.f);v.vignette_radius=std::clamp(v.vignette_radius,0.f,1.f);v.vignette_softness=std::clamp(v.vignette_softness,.01f,1.f);v.aberration=std::clamp(v.aberration,0.f,10.f);v.distortion=std::clamp(v.distortion,-.5f,.5f);v.lut_blend=std::clamp(v.lut_blend,0.f,1.f);
    auto&s=store();std::lock_guard lock(s.mutex);s.state.settings=v;
}
void report(bool ready,const std::string& message){auto&s=store();std::lock_guard lock(s.mutex);s.state.ready=ready;s.state.status=message;}
void load_lut(const std::string& path){
    if(path.empty())return;auto&s=store();std::uint64_t request;
    {std::lock_guard lock(s.mutex);if(s.loading)return;s.loading=true;request=++s.request;s.state.lut_status="Loading LUT...";}
    try{std::thread([path,request]{auto&s=store();try{
        auto lut=parse_lut(path,request);std::lock_guard lock(s.mutex);if(request!=s.request){s.loading=false;s.state.lut_status="No LUT loaded";return;}
        s.lut=lut;++s.generation;s.loading=false;s.state.lut_path=path;
        s.state.lut_status="Loaded "+std::to_string(lut->size)+" cubed LUT (SDR input)";
    }catch(const std::exception&e){std::lock_guard lock(s.mutex);s.loading=false;if(request!=s.request){s.state.lut_status="No LUT loaded";return;}s.state.lut_status=std::string("LUT load failed: ")+e.what()+"; previous LUT retained";}}).detach();}
    catch(const std::exception&e){std::lock_guard lock(s.mutex);s.loading=false;s.state.lut_status=std::string("LUT worker failed: ")+e.what();}
}
void unload_lut(){auto&s=store();std::lock_guard lock(s.mutex);++s.request;s.lut.reset();++s.generation;s.state.lut_path.clear();s.state.lut_status=s.loading?"LUT unloaded; cancelling loader...":"No LUT loaded";}
bool neutral(const Settings& v){return v.exposure_ev==0&&v.contrast==1&&v.saturation==1&&v.vibrance==0&&v.grain==0&&v.sharpen==0&&v.vignette==0&&v.aberration==0&&v.distortion==0&&v.lut_blend==0;}
void Pass::reset(){luts_.clear();pipeline_.Reset();root_.Reset();attempted_=false;format_=DXGI_FORMAT_UNKNOWN;}
bool Pass::prepare(ID3D12Device* device,DXGI_FORMAT format){
    if(pipeline_&&format_==format)return true;
    if(attempted_)return false;attempted_=true;format_=format;
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3DBlob> vs,ps,errors;
    auto compile=[&](const char* entry,const char* target,ComPtr<ID3DBlob>& out){
        errors.Reset();HRESULT hr=D3DCompile(shader,std::strlen(shader),"TheaterColorGrading",nullptr,nullptr,entry,target,D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&out,&errors);
        if(FAILED(hr)){report(false,std::string("Color shader compilation failed: ")+(errors?static_cast<const char*>(errors->GetBufferPointer()):"no compiler diagnostic"));return false;}return true;
    };
    if(!compile("VS","vs_5_0",vs)||!compile("PS","ps_5_0",ps))return false;
    D3D12_DESCRIPTOR_RANGE range{};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;range.NumDescriptors=1;range.BaseShaderRegister=0;range.OffsetInDescriptorsFromTableStart=0;
    D3D12_ROOT_PARAMETER parameters[3]{};
    parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;parameters[0].DescriptorTable={1,&range};
    parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;parameters[1].Constants={0,0,28};
    parameters[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;parameters[2].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;parameters[2].Descriptor={1,0};
    D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC desc{};desc.NumParameters=3;desc.pParameters=parameters;desc.NumStaticSamplers=1;desc.pStaticSamplers=&sampler;
    desc.Flags=D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS|D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS|D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
    ComPtr<ID3DBlob> blob;
    if(FAILED(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors))||FAILED(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root_)))){report(false,"Color root signature creation failed");return false;}
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};p.pRootSignature=root_.Get();p.VS={vs->GetBufferPointer(),vs->GetBufferSize()};p.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
    auto& blend=p.BlendState.RenderTarget[0];blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;blend.SrcBlend=D3D12_BLEND_ONE;blend.DestBlend=D3D12_BLEND_ZERO;blend.BlendOp=D3D12_BLEND_OP_ADD;blend.SrcBlendAlpha=D3D12_BLEND_ONE;blend.DestBlendAlpha=D3D12_BLEND_ZERO;blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;blend.LogicOp=D3D12_LOGIC_OP_NOOP;
    p.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;p.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;p.RasterizerState.DepthClipEnable=TRUE;
    p.DepthStencilState.DepthEnable=FALSE;p.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;p.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;
    p.DepthStencilState.FrontFace.StencilFailOp=p.DepthStencilState.FrontFace.StencilDepthFailOp=p.DepthStencilState.FrontFace.StencilPassOp=D3D12_STENCIL_OP_KEEP;p.DepthStencilState.FrontFace.StencilFunc=D3D12_COMPARISON_FUNC_ALWAYS;p.DepthStencilState.BackFace=p.DepthStencilState.FrontFace;
    p.SampleMask=UINT_MAX;p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;p.NumRenderTargets=1;p.RTVFormats[0]=format;p.SampleDesc.Count=1;
    if(FAILED(device->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&pipeline_)))){report(false,"Color pipeline creation failed");return false;}
    return true;
}
bool Pass::prepare_lut(ID3D12Device* device,UINT slot){
    auto&s=store();std::shared_ptr<const Lut> lut;std::uint64_t generation;
    {std::lock_guard lock(s.mutex);lut=s.lut;generation=s.generation;}
    auto& gpu=luts_[slot];if(gpu.buffer&&gpu.generation==generation)return true;
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=lut?lut->data.size()*16:16;desc.Height=1;desc.DepthOrArraySize=desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(&resource)))){report(false,"LUT GPU allocation failed; Look bypassed");return false;}
    void* mapped=nullptr;D3D12_RANGE empty{};if(FAILED(resource->Map(0,&empty,&mapped))){report(false,"LUT upload map failed; Look bypassed");return false;}
    if(lut)memcpy(mapped,lut->data.data(),desc.Width);else memset(mapped,0,16);resource->Unmap(0,nullptr);
    gpu.buffer=resource;gpu.generation=generation;gpu.size=lut?lut->size:0;
    if(lut){gpu.minimum=lut->minimum;for(unsigned i=0;i<3;++i)gpu.range[i]=lut->maximum[i]-lut->minimum[i];}
    return true;
}
void Pass::record(ID3D12GraphicsCommandList* list,D3D12_GPU_DESCRIPTOR_HANDLE source,UINT width,UINT height,const Settings& v,UINT slot){
    const auto& lut=luts_.at(slot);
    const float values[28]={v.exposure_ev,v.contrast,v.saturation,v.vibrance,v.grain,v.grain_size,v.grain_speed,v.sharpen,
        v.vignette,v.vignette_radius,v.vignette_softness,v.aberration,float(width),float(height),float(GetTickCount64()%86400000)/1000.f,v.distortion,
        lut.size?v.lut_blend:0.f,float(lut.size),0,0,lut.minimum[0],lut.minimum[1],lut.minimum[2],0,lut.range[0],lut.range[1],lut.range[2],0};
    D3D12_VIEWPORT viewport{0,0,float(width),float(height),0,1};D3D12_RECT scissor{0,0,LONG(width),LONG(height)};
    list->SetPipelineState(pipeline_.Get());list->SetGraphicsRootSignature(root_.Get());list->SetGraphicsRootDescriptorTable(0,source);list->SetGraphicsRoot32BitConstants(1,28,values,0);list->SetGraphicsRootShaderResourceView(2,lut.buffer->GetGPUVirtualAddress());
    list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&scissor);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->DrawInstanced(3,1,0,0);
}
}
