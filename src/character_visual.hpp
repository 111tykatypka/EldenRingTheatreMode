#pragma once
#include <array>
#include <cstdint>
#include <cstdio>
#include <istream>
#include <ostream>
#include <stdexcept>
namespace erplay {
// Optional ERPLAY03 track 4, schema 1. Entity 0 is the existing local-player track.
// Field IDs and bounded native face serialization only; never native pointers.
struct VisualState {
 std::uint64_t id{},timestamp_ns{};
 std::uint32_t version{1},model{},hp{},max_hp{},flags{},left_slot{},right_slot{},arm_style{},gender{},archetype{},item_effect{},ground{};
 std::array<std::int32_t,22> equipment{};
 std::array<unsigned char,288> face{};
 bool valid()const{return version==1&&flags<=31&&(!(flags&8)||(left_slot<3&&right_slot<3&&arm_style<=3));}
 bool same(const VisualState&b)const{auto a=*this,c=b;a.timestamp_ns=c.timestamp_ns=0;return a.id==c.id&&a.version==c.version&&a.model==c.model&&a.hp==c.hp&&a.max_hp==c.max_hp&&a.flags==c.flags&&a.left_slot==c.left_slot&&a.right_slot==c.right_slot&&a.arm_style==c.arm_style&&a.gender==c.gender&&a.archetype==c.archetype&&a.item_effect==c.item_effect&&a.ground==c.ground&&a.equipment==c.equipment&&a.face==c.face;}
};
inline constexpr std::uint32_t visual_record_bytes=440;
template<class T>inline void visual_put(std::ostream&o,T v){for(unsigned j=0;j<sizeof(T);++j)o.put(char((std::uint64_t(v)>>(j*8))&255));if(!o)throw std::runtime_error("visual write failed");}
template<class T>inline T visual_get(std::istream&i){std::uint64_t v=0;for(unsigned j=0;j<sizeof(T);++j){auto c=i.get();if(c==EOF)throw std::runtime_error("truncated visual record");v|=std::uint64_t(static_cast<unsigned char>(c))<<(j*8);}return static_cast<T>(v);}
inline void write_visual(std::ostream&o,const VisualState&s){visual_put(o,s.id);visual_put(o,s.timestamp_ns);for(auto v:{s.version,s.model,s.hp,s.max_hp,s.flags,s.left_slot,s.right_slot,s.arm_style,s.gender,s.archetype,s.item_effect,s.ground})visual_put(o,v);for(auto v:s.equipment)visual_put(o,v);o.write(reinterpret_cast<const char*>(s.face.data()),s.face.size());if(!o)throw std::runtime_error("visual write failed");}
inline VisualState read_visual(std::istream&i){VisualState s;s.id=visual_get<std::uint64_t>(i);s.timestamp_ns=visual_get<std::uint64_t>(i);for(auto p:{&s.version,&s.model,&s.hp,&s.max_hp,&s.flags,&s.left_slot,&s.right_slot,&s.arm_style,&s.gender,&s.archetype,&s.item_effect,&s.ground})*p=visual_get<std::uint32_t>(i);for(auto&v:s.equipment)v=visual_get<std::int32_t>(i);i.read(reinterpret_cast<char*>(s.face.data()),s.face.size());if(!i||!s.valid())throw std::runtime_error("invalid visual state/schema");return s;}
}
