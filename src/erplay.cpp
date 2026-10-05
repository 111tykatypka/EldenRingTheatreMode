#include "erplay.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace erplay {
namespace {
constexpr std::array<char, 8> magic{'E','R','P','L','A','Y','0','2'};
constexpr std::uint32_t version = 2, chunk_marker = 0x4B4E4843, footer_marker = 0x544F4F46;
constexpr std::uint64_t fixed_header_bytes = 8 + 4 + 4 + 8 + 8 + 8 + 8 + 8 + 8;
static_assert(std::endian::native == std::endian::little, "ERPLAY v2 currently requires little-endian Windows/x64");

template<class T> void put(std::ostream& o, T v) {
    static_assert(std::is_trivially_copyable_v<T>);
    o.write(reinterpret_cast<const char*>(&v), sizeof(v));
    if (!o) throw std::runtime_error("replay write failed");
}
template<class T> T get(std::istream& i) {
    static_assert(std::is_trivially_copyable_v<T>);
    T v{}; i.read(reinterpret_cast<char*>(&v), sizeof(v));
    if (!i) throw std::runtime_error("truncated replay file");
    return v;
}
void put_string(std::ostream& o, const std::string& s) {
    if (s.size() > std::numeric_limits<std::uint32_t>::max()) throw std::length_error("metadata field too large");
    put(o, static_cast<std::uint32_t>(s.size())); o.write(s.data(), static_cast<std::streamsize>(s.size()));
    if (!o) throw std::runtime_error("metadata write failed");
}
std::string get_string(std::istream& i) {
    auto n = get<std::uint32_t>(i);
    if (n > 16U * 1024U * 1024U) throw std::runtime_error("metadata field exceeds 16 MiB");
    std::string s(n, '\0'); i.read(s.data(), n); if (!i) throw std::runtime_error("truncated metadata"); return s;
}
std::uint32_t crc32(const unsigned char* p, std::size_t n) {
    std::uint32_t c = 0xFFFFFFFFu;
    for (std::size_t j=0;j<n;++j) { c ^= p[j]; for(int k=0;k<8;++k) c = (c >> 1) ^ (0xEDB88320u & (0u-(c&1u))); }
    return ~c;
}
bool finite(const Sample& s) {
    const auto& q=s.orientation;
    if (!(std::isfinite(s.position.x)&&std::isfinite(s.position.y)&&std::isfinite(s.position.z)&&
          std::isfinite(q.x)&&std::isfinite(q.y)&&std::isfinite(q.z)&&std::isfinite(q.w))) return false;
    const double norm=double(q.x)*q.x+double(q.y)*q.y+double(q.z)*q.z+double(q.w)*q.w;
    return norm >= 0.25 && norm <= 2.25;
}
void encode(std::ostream& out, const Sample& s) {
    put(out,s.index); put(out,s.replay_time_ns); put(out,s.source_time_ns);
    put(out,s.position.x); put(out,s.position.y); put(out,s.position.z);
    put(out,s.orientation.x); put(out,s.orientation.y); put(out,s.orientation.z); put(out,s.orientation.w);
}
Sample decode(std::istream& in) {
    Sample s; s.index=get<std::uint64_t>(in); s.replay_time_ns=get<std::uint64_t>(in); s.source_time_ns=get<std::uint64_t>(in);
    s.position={get<float>(in),get<float>(in),get<float>(in)};
    s.orientation={get<float>(in),get<float>(in),get<float>(in),get<float>(in)}; return s;
}
void write_header(std::ostream& o,const Metadata& m,std::uint64_t count,std::uint64_t duration,std::uint64_t paused,double rate) {
    o.write(magic.data(), magic.size()); put(o,version); put(o,std::uint32_t{0});
    put(o,m.recording_start_unix_ns); put(o,m.requested_rate_hz); put(o,rate); put(o,count); put(o,duration); put(o,paused);
    put_string(o,m.game_version); put_string(o,m.mod_version); put_string(o,m.title); put_string(o,m.description); put_string(o,m.tags);
}
Metadata read_header(std::istream& in, Summary& s) {
    std::array<char,8> got{}; in.read(got.data(),got.size());
    if (!in || got!=magic) throw std::runtime_error("invalid ERPLAY magic");
    if (get<std::uint32_t>(in)!=version) throw std::runtime_error("unsupported ERPLAY version");
    (void)get<std::uint32_t>(in);
    s.metadata.recording_start_unix_ns=get<std::uint64_t>(in);
    s.metadata.requested_rate_hz=get<double>(in); s.actual_rate_hz=get<double>(in);
    s.sample_count=get<std::uint64_t>(in); s.duration_ns=get<std::uint64_t>(in); s.paused_duration_ns=get<std::uint64_t>(in);
    s.metadata.game_version=get_string(in); s.metadata.mod_version=get_string(in); s.metadata.title=get_string(in);
    s.metadata.description=get_string(in); s.metadata.tags=get_string(in); return s.metadata;
}
} // namespace

struct Writer::Impl {
    std::filesystem::path final_path, temp_path;
    Metadata metadata;
    std::ofstream out;
    std::vector<Sample> pending;
    std::uint32_t chunk_limit;
    std::uint64_t count{}, duration{}, chunks{};
    std::uint64_t first_source{}, last_source{}, last_replay{};
    bool has_sample{}, done{};

    Impl(std::filesystem::path p, Metadata m, std::uint32_t n):final_path(std::move(p)),metadata(std::move(m)),chunk_limit(n) {
        if (!chunk_limit) throw std::invalid_argument("chunk sample count must be positive");
        if (!(std::isfinite(metadata.requested_rate_hz)&&metadata.requested_rate_hz>0)) throw std::invalid_argument("requested rate must be finite and positive");
        temp_path=final_path; temp_path += ".tmp";
        if (!final_path.parent_path().empty()) std::filesystem::create_directories(final_path.parent_path());
        out.open(temp_path,std::ios::binary|std::ios::trunc);
        if(!out) throw std::runtime_error("cannot create replay temporary file");
        write_header(out,metadata,0,0,0,0.0);
        pending.reserve(chunk_limit);
    }
    void flush_chunk() {
        if(pending.empty()) return;
        std::ostringstream bytes(std::ios::out|std::ios::binary);
        for(const auto& s:pending) encode(bytes,s);
        const auto payload=bytes.str();
        put(out,chunk_marker); put(out,static_cast<std::uint32_t>(pending.size()));
        put(out,static_cast<std::uint64_t>(payload.size()));
        put(out,crc32(reinterpret_cast<const unsigned char*>(payload.data()),payload.size()));
        out.write(payload.data(),static_cast<std::streamsize>(payload.size())); out.flush();
        if(!out) throw std::runtime_error("failed flushing replay chunk");
        ++chunks; pending.clear();
    }
};

Writer::Writer(std::filesystem::path path, Metadata meta, std::uint32_t n):impl_(new Impl(std::move(path),std::move(meta),n)) {}
Writer::~Writer() { if(impl_) { if(impl_->out.is_open()) impl_->out.close(); delete impl_; } }
const std::filesystem::path& Writer::temporary_path() const noexcept { return impl_->temp_path; }
void Writer::append(Sample s) {
    auto& x=*impl_; if(x.done) throw std::logic_error("replay writer is finalized");
    if(!finite(s)) throw std::invalid_argument("sample contains non-finite transform or invalid quaternion");
    if(s.index!=x.count) throw std::invalid_argument("sample indices must be contiguous from zero");
    if(x.has_sample && (s.replay_time_ns<x.last_replay || s.source_time_ns<x.last_source))
        throw std::invalid_argument("sample timestamps regress");
    if(!x.has_sample) x.first_source=s.source_time_ns;
    x.has_sample=true; x.last_source=s.source_time_ns; x.last_replay=s.replay_time_ns; x.duration=s.replay_time_ns;
    x.pending.push_back(s); ++x.count; if(x.pending.size()>=x.chunk_limit) x.flush_chunk();
}
RecordingSession::RecordingSession(Writer& writer) noexcept : writer_(writer) {}
void RecordingSession::start() {
    if(state_!=RecordingState::idle) throw std::logic_error("session can only start once");
    state_=RecordingState::recording;
}
void RecordingSession::ingest(Sample sample) {
    if(state_==RecordingState::paused) return;
    if(state_!=RecordingState::recording) throw std::logic_error("session is not recording");
    if(!has_origin_) { source_origin_ns_=sample.source_time_ns; has_origin_=true; }
    if(sample.source_time_ns<source_origin_ns_+paused_total_ns_) throw std::invalid_argument("source clock precedes active recording time");
    sample.index=sample_index_;
    sample.replay_time_ns=sample.source_time_ns-source_origin_ns_-paused_total_ns_;
    writer_.append(sample);
    ++sample_index_;
}
void RecordingSession::pause(std::uint64_t t) {
    if(state_!=RecordingState::recording) throw std::logic_error("session is not recording");
    if(has_origin_ && t<source_origin_ns_+paused_total_ns_) throw std::invalid_argument("pause time regressed");
    pause_started_ns_=t; state_=RecordingState::paused;
}
void RecordingSession::resume(std::uint64_t t) {
    if(state_!=RecordingState::paused) throw std::logic_error("session is not paused");
    if(t<pause_started_ns_) throw std::invalid_argument("resume time precedes pause");
    paused_total_ns_+=t-pause_started_ns_; state_=RecordingState::recording;
}
Summary RecordingSession::stop() {
    if(state_!=RecordingState::recording && state_!=RecordingState::paused) throw std::logic_error("session cannot stop from current state");
    state_=RecordingState::saving;
    try { auto result=writer_.finalize(paused_total_ns_); state_=RecordingState::ready; return result; }
    catch(...) { state_=RecordingState::error; throw; }
}
Summary Writer::finalize(std::uint64_t paused_duration_ns) {
    auto& x=*impl_; if(x.done) throw std::logic_error("replay writer already finalized");
    x.flush_chunk();
    put(x.out,footer_marker); put(x.out,x.chunks); put(x.out,x.count); put(x.out,x.duration); put(x.out,paused_duration_ns);
    const double rate=x.count>1 && x.duration>0 ? double(x.count-1)*1e9/double(x.duration) : 0.0;
    x.out.seekp(0); write_header(x.out,x.metadata,x.count,x.duration,paused_duration_ns,rate);
    x.out.flush(); if(!x.out) throw std::runtime_error("failed finalizing replay header"); x.out.close();
    const auto verified=validate(x.temp_path);
    std::error_code ec; std::filesystem::rename(x.temp_path,x.final_path,ec);
    if(ec) throw std::filesystem::filesystem_error("atomic replay finalization failed",x.temp_path,x.final_path,ec);
    x.done=true; auto result=verified; result.chunk_count=x.chunks; return result;
}

Summary validate(const std::filesystem::path& p) {
    std::ifstream in(p,std::ios::binary); if(!in) throw std::runtime_error("cannot open replay for validation");
    Summary s; read_header(in,s); std::uint64_t total=0, duration=0, chunks=0, previous_time=0, previous_source=0; bool first=true;
    while(true) {
        const auto marker=get<std::uint32_t>(in);
        if(marker==footer_marker) {
            const auto footer_chunks=get<std::uint64_t>(in), footer_samples=get<std::uint64_t>(in), footer_duration=get<std::uint64_t>(in), footer_paused=get<std::uint64_t>(in);
            if(footer_chunks!=chunks || footer_samples!=total || footer_duration!=duration || footer_paused!=s.paused_duration_ns) throw std::runtime_error("replay footer mismatch");
            if(total!=s.sample_count || duration!=s.duration_ns) throw std::runtime_error("header summary mismatch");
            if(in.peek()!=std::char_traits<char>::eof()) throw std::runtime_error("unexpected bytes after replay footer");
            s.chunk_count=chunks; return s;
        }
        if(marker!=chunk_marker) throw std::runtime_error("invalid chunk marker");
        const auto n=get<std::uint32_t>(in); const auto bytes=get<std::uint64_t>(in); const auto expected=get<std::uint32_t>(in);
        constexpr std::uint64_t sample_size=52;
        if(!n || bytes!=std::uint64_t(n)*sample_size) throw std::runtime_error("invalid chunk length");
        std::string payload(static_cast<std::size_t>(bytes),'\0'); in.read(payload.data(),static_cast<std::streamsize>(bytes));
        if(!in) throw std::runtime_error("truncated replay chunk");
        if(crc32(reinterpret_cast<const unsigned char*>(payload.data()),payload.size())!=expected) throw std::runtime_error("replay chunk checksum mismatch");
        std::istringstream data(payload,std::ios::in|std::ios::binary);
        for(std::uint32_t j=0;j<n;++j) {
            const auto sample=decode(data);
            if(!finite(sample)) throw std::runtime_error("invalid sample transform/quaternion");
            if(sample.index!=total) throw std::runtime_error("sample index discontinuity");
            if(!first && (sample.replay_time_ns<previous_time || sample.source_time_ns<previous_source)) throw std::runtime_error("sample timestamp ordering invalid");
            previous_time=sample.replay_time_ns; previous_source=sample.source_time_ns; duration=sample.replay_time_ns; first=false; ++total;
        }
        ++chunks;
    }
}

struct Reader::Impl {
    struct Entry { std::uint64_t time{}, offset{}; std::uint32_t chunk{}; };
    struct Chunk { std::uint64_t offset{}; std::uint32_t count{}; };
    std::filesystem::path path;
    Summary info;
    mutable std::ifstream input;
    std::vector<Entry> index;
    std::vector<Chunk> chunks;
    mutable std::uint32_t cached_chunk{std::numeric_limits<std::uint32_t>::max()};
    mutable std::vector<Sample> cache;

    explicit Impl(const std::filesystem::path& p) : path(p), info(validate(p)), input(p,std::ios::binary) {
        if(!input) throw std::runtime_error("cannot open replay for indexed reading");
        Summary header; read_header(input,header);
        index.reserve(static_cast<std::size_t>(info.sample_count));
        while(true) {
            const auto marker=get<std::uint32_t>(input);
            if(marker==footer_marker) break;
            if(marker!=chunk_marker) throw std::runtime_error("invalid chunk marker while indexing replay");
            const auto n=get<std::uint32_t>(input); (void)get<std::uint64_t>(input); (void)get<std::uint32_t>(input);
            const auto start=static_cast<std::uint64_t>(input.tellg());
            const auto chunk_id=static_cast<std::uint32_t>(chunks.size());
            chunks.push_back({start,n});
            for(std::uint32_t j=0;j<n;++j) {
                const auto s=decode(input);
                index.push_back({s.replay_time_ns,start+std::uint64_t(j)*52,chunk_id});
            }
        }
        if(index.size()!=info.sample_count) throw std::runtime_error("sample index count disagrees with validated replay");
    }
    Sample at(std::uint64_t i) const {
        if(i>=index.size()) throw std::out_of_range("replay sample index out of range");
        const auto id=index[static_cast<std::size_t>(i)].chunk;
        if(id!=cached_chunk) {
            const auto& c=chunks[id]; input.clear(); input.seekg(static_cast<std::streamoff>(c.offset));
            if(!input) throw std::runtime_error("failed seeking to replay chunk");
            cache.clear(); cache.reserve(c.count);
            for(std::uint32_t j=0;j<c.count;++j) cache.push_back(decode(input));
            cached_chunk=id;
        }
        const auto first=std::lower_bound(index.begin(),index.end(),id,[](const Entry&e,std::uint32_t v){return e.chunk<v;});
        const auto local=static_cast<std::size_t>(index.begin()+static_cast<std::ptrdiff_t>(i)-first);
        return cache.at(local);
    }
};

Reader::Reader(const std::filesystem::path& path):impl_(std::make_unique<Impl>(path)){}
Reader::~Reader()=default;
Reader::Reader(Reader&&) noexcept=default;
Reader& Reader::operator=(Reader&&) noexcept=default;
const Summary& Reader::summary() const noexcept{return impl_->info;}
Sample Reader::sample(std::uint64_t i) const{return impl_->at(i);}
std::uint64_t Reader::lower_sample(std::uint64_t t) const {
    if(impl_->index.empty()) throw std::runtime_error("replay contains no samples");
    const auto it=std::upper_bound(impl_->index.begin(),impl_->index.end(),t,[](std::uint64_t v,const Impl::Entry&e){return v<e.time;});
    if(it==impl_->index.begin()) return 0;
    return static_cast<std::uint64_t>(std::distance(impl_->index.begin(),it)-1);
}
std::pair<Sample,Sample> Reader::bracket(std::uint64_t t) const {
    const auto lo=lower_sample(t);
    if(lo+1>=impl_->index.size()) return {sample(lo),sample(lo)};
    return {sample(lo),sample(lo+1)};
}

Summary recover_incomplete(const std::filesystem::path& source,const std::filesystem::path& final_path) {
    std::ifstream in(source,std::ios::binary); if(!in) throw std::runtime_error("cannot open incomplete replay");
    Summary summary; read_header(in,summary);
    if(summary.sample_count||summary.duration_ns) throw std::runtime_error("temporary file already has finalized header data");
    auto output_temp=final_path; output_temp += ".recovery.tmp";
    std::ofstream out(output_temp,std::ios::binary|std::ios::trunc); if(!out) throw std::runtime_error("cannot create recovered replay temporary file");
    write_header(out,summary.metadata,0,0,summary.paused_duration_ns,0.0);
    std::uint64_t total=0,duration=0,chunks=0,previous_time=0,previous_source=0;bool first=true;
    constexpr std::uint64_t sample_size=52;
    while(in.peek()!=std::char_traits<char>::eof()) {
        std::array<char,4> marker_bytes{};in.read(marker_bytes.data(),marker_bytes.size());
        if(in.gcount()!=static_cast<std::streamsize>(marker_bytes.size())) break;
        std::uint32_t marker{};std::memcpy(&marker,marker_bytes.data(),sizeof(marker));
        if(marker!=chunk_marker) throw std::runtime_error("unexpected record in incomplete replay");
        std::array<char,16> meta{};in.read(meta.data(),meta.size());
        if(in.gcount()!=static_cast<std::streamsize>(meta.size())) break;
        std::uint32_t n{},checksum{};std::uint64_t payload_size{};
        std::memcpy(&n,meta.data(),4);std::memcpy(&payload_size,meta.data()+4,8);std::memcpy(&checksum,meta.data()+12,4);
        if(!n||payload_size!=std::uint64_t(n)*sample_size) throw std::runtime_error("invalid incomplete chunk size");
        if(payload_size>std::numeric_limits<std::size_t>::max()) throw std::runtime_error("chunk exceeds addressable memory");
        std::string payload(static_cast<std::size_t>(payload_size),'\0');in.read(payload.data(),static_cast<std::streamsize>(payload_size));
        if(in.gcount()!=static_cast<std::streamsize>(payload_size)) break;
        if(crc32(reinterpret_cast<const unsigned char*>(payload.data()),payload.size())!=checksum) throw std::runtime_error("incomplete replay contains a checksum failure");
        std::istringstream data(payload,std::ios::in|std::ios::binary);
        for(std::uint32_t i=0;i<n;++i){const auto s=decode(data);if(!finite(s)||s.index!=total)throw std::runtime_error("incomplete replay contains invalid sample data");if(!first&&(s.replay_time_ns<previous_time||s.source_time_ns<previous_source))throw std::runtime_error("incomplete replay timestamps regress");previous_time=s.replay_time_ns;previous_source=s.source_time_ns;duration=s.replay_time_ns;first=false;++total;}
        out.write(marker_bytes.data(),marker_bytes.size());out.write(meta.data(),meta.size());out.write(payload.data(),static_cast<std::streamsize>(payload.size()));if(!out)throw std::runtime_error("failed writing recovered chunks");++chunks;
    }
    if(!total) throw std::runtime_error("incomplete replay has no complete chunks to recover");
    put(out,footer_marker);put(out,chunks);put(out,total);put(out,duration);put(out,summary.paused_duration_ns);
    const double rate=total>1&&duration?double(total-1)*1e9/double(duration):0.0;
    out.seekp(0);write_header(out,summary.metadata,total,duration,summary.paused_duration_ns,rate);out.flush();if(!out)throw std::runtime_error("failed finalizing recovered replay");out.close();
    const auto verified=validate(output_temp);std::error_code ec;std::filesystem::rename(output_temp,final_path,ec);if(ec)throw std::filesystem::filesystem_error("recovered replay rename failed",output_temp,final_path,ec);return verified;
}

} // namespace erplay
