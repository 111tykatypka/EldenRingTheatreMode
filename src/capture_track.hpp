#pragma once
#include "capture_schema.hpp"
#include <vector>
#include <istream>
#include <ostream>
#include <stdexcept>
namespace erplay {
struct CaptureRecord {
 std::uint64_t timestamp_ns{}, source_ns{}, sequence{}, source_drops{};
 std::vector<std::uint32_t> valid, values;
 bool available(unsigned word)const{return word<values.size()&&(valid.at(word/32)&(1u<<(word%32)));}
};
inline const CaptureTrackDef* capture_track(unsigned id){for(const auto&t:capture_tracks)if(t.id==id)return &t;return nullptr;}
template<class T>inline void capture_put(std::ostream&o,T value){o.write(reinterpret_cast<const char*>(&value),sizeof(value));if(!o)throw std::runtime_error("capture write failed");}
template<class T>inline T capture_get(std::istream&i){T value{};i.read(reinterpret_cast<char*>(&value),sizeof(value));if(!i)throw std::runtime_error("capture truncated");return value;}
inline void write_capture_record(std::ostream&o,const CaptureTrackDef&t,const CaptureFrame&f,std::uint64_t time,std::uint64_t source,std::uint64_t sequence){
 capture_put(o,time);capture_put(o,source);capture_put(o,sequence);capture_put(o,f.source_drops);
 for(unsigned m=0;m<t.mask_words;++m){std::uint32_t bits=0;for(unsigned j=0;j<32&&m*32+j<t.words;++j){const auto n=t.begin+m*32+j;if(f.valid[n/32]&(1u<<(n%32)))bits|=1u<<j;}capture_put(o,bits);}
 for(unsigned j=0;j<t.words;++j)capture_put(o,f.values[t.begin+j]);
}
inline CaptureRecord read_capture_record(std::istream&i,const CaptureTrackDef&t){
 CaptureRecord r;r.timestamp_ns=capture_get<std::uint64_t>(i);r.source_ns=capture_get<std::uint64_t>(i);r.sequence=capture_get<std::uint64_t>(i);r.source_drops=capture_get<std::uint64_t>(i);
 for(unsigned m=0;m<t.mask_words;++m)r.valid.push_back(capture_get<std::uint32_t>(i));
 for(unsigned j=0;j<t.words;++j)r.values.push_back(capture_get<std::uint32_t>(i));
 if(t.words%32&&(r.valid.back()>>(t.words%32)))throw std::runtime_error("capture availability bits outside schema");return r;
}
inline bool capture_frame_valid(const CaptureFrame&f){return f.schema==1&&f.words==capture_words&&(!(capture_words%32)||!(f.valid.back()>>(capture_words%32)));}
}
