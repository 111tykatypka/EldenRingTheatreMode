#pragma once
// Video / image-sequence exporter. The render backend copies each presented game frame (after the Look effects, before any Theater
// UI is drawn, so the export never contains the overlay or the export status banner) and hands it to this module, which pipes the
// frames to an ffmpeg process (NVENC or CPU encoders) or writes an image sequence through ffmpeg too.
#include <cstdint>
#include <string>
namespace video_export {
enum class Container : int { Avi = 0, PngSequence = 1, JpegSequence = 2 };
enum class Codec : int { H264Nvenc = 0, H264Cpu = 1, Mjpeg = 2, Ffv1 = 3 };
struct Settings {
    int container = 0;          // Container
    int codec = 0;              // Codec (video containers only)
    int fps = 60;               // constant output frame rate; frames are duplicated or dropped to hold it
    int quality = 75;           // 1 to 100 (ignored by lossless codecs and PNG)
    std::string folder;         // UTF-8; empty = Videos\EldenRingTheaterMode
    std::string ffmpeg;         // UTF-8; empty = look next to the DLL, then PATH
};
struct Status {
    bool active = false;
    bool ffmpeg_found = false;
    std::string ffmpeg_path;
    std::string text;           // one line for the on-screen banner
    std::string output;         // last finished output
    std::string error;          // last error (empty when none)
    double seconds = 0;         // length of the current export
    std::uint64_t frames = 0, duplicated = 0, dropped = 0;
    bool recently_finished = false; // true for a few seconds after a stop, for the "saved" message
};
Settings settings();
void configure(const Settings& value);   // also saved to disk
void load_settings();                    // once at start
Status status();
void request_toggle();                   // F7 / button: start or stop at the next presented frame
bool toggle_requested();                 // render thread: consumes the request
bool toggle_pending();                   // peek without consuming
bool banner_visible();                   // anything worth showing on screen right now
bool active();
// Render thread API.
bool begin(unsigned width, unsigned height, bool bgra);
void end(const char* reason = nullptr);
void submit(const std::uint8_t* pixels, std::size_t row_pitch, double qpc_seconds);
void fail(const std::string& message);   // stops the export and shows the error
std::string find_ffmpeg();               // path or empty
}
