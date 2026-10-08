#include "ParticleCatalog.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <utility>
#include <algorithm>
#include <iterator>
namespace particle_catalog { namespace {
const Reference reference_data[]={
#include "ParticleReferenceData.inc"
};
std::map<std::uint32_t,Entry> catalog;
bool loaded=false;
std::string message;
std::filesystem::path path(){
    wchar_t root[32768]{};const auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",root,32768);
    if(!n||n>=32768)throw std::runtime_error("LOCALAPPDATA unavailable");
    return std::filesystem::path(root)/L"EldenRingTheaterMode"/L"particle_catalog.ertfxr";
}
void load(){
    if(loaded)return;loaded=true;
    try{
        const auto p=path();if(!std::filesystem::exists(p))return;
        std::ifstream f(p,std::ios::binary);std::string magic;unsigned version=0;
        if(!(f>>magic>>version)||magic!="ERTFXR"||version!=1)throw std::runtime_error("unsupported catalog format");
        std::map<std::uint32_t,Entry> next;
        for(;;){f>>std::ws;if(f.eof())break;std::uint32_t id=0;Entry e;
            if(!(f>>id>>e.favorite>>std::quoted(e.name)>>std::quoted(e.category))||!id||next.contains(id))throw std::runtime_error("invalid catalog entry");
            next.emplace(id,std::move(e));
        }
        catalog=std::move(next);
    }catch(const std::exception& e){message=std::string("Catalog load failed: ")+e.what();}
}
}
const std::map<std::uint32_t,Entry>& entries(){load();return catalog;}
const std::string& status(){load();return message;}
const Reference* reference(std::uint32_t id){
    const auto it=std::lower_bound(std::begin(reference_data),std::end(reference_data),id,[](const Reference& r,std::uint32_t value){return r.id<value;});
    return it!=std::end(reference_data)&&it->id==id?it:nullptr;
}
std::string label(std::uint32_t id){
    load();const auto it=catalog.find(id);const auto number=std::to_string(id);
    if(it!=catalog.end()&&!it->second.name.empty())return it->second.name+" [FXR "+number+"]";
    if(const auto* r=reference(id)){
        const char* name=r->info[0]?r->info:r->behavior;
        if(name[0]){std::string text=name;std::replace(text.begin(),text.end(),'\n',' ');std::replace(text.begin(),text.end(),'\r',' ');return text+" [FXR "+number+" / Sheet]";}
    }
    return "Unknown effect [FXR "+number+"]";
}
std::string search_text(std::uint32_t id){
    auto text=label(id);if(auto it=catalog.find(id);it!=catalog.end())text+=" "+it->second.category;
    if(const auto* r=reference(id)){text+=" ";text+=r->bank;text+=" ";text+=r->origin;text+=" ";text+=r->color;text+=" ";text+=r->behavior;text+=" ";text+=r->info;}
    return text;
}
bool save(std::uint32_t id,Entry entry){
    load();if(!id)return false;
    try{
        auto next=catalog;next[id]=std::move(entry);
        auto p=path();std::filesystem::create_directories(p.parent_path());auto temp=p;temp+=L".tmp";
        std::ofstream f(temp,std::ios::binary|std::ios::trunc);f<<"ERTFXR 1\n";
        for(const auto& [key,e]:next)f<<key<<' '<<e.favorite<<' '<<std::quoted(e.name)<<' '<<std::quoted(e.category)<<'\n';
        f.flush();if(!f)throw std::runtime_error("write failed");f.close();
        if(!MoveFileExW(temp.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("replace failed");
        catalog=std::move(next);message="Effect name / category / favorite saved";return true;
    }catch(const std::exception& e){message=std::string("Catalog save failed: ")+e.what();return false;}
}
}
