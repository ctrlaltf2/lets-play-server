class EmulatorController;
struct EmulatorControllerProxy;
struct RGBColor;
struct RGBAColor;
struct VideoFormat;
struct Frame;
#pragma once
#include <condition_variable>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <variant>

#include <websocketpp/frame.hpp>

#include <webp/encode.h>

#include "libretro.h"

#include "Config.h"
#include "LetsPlayServer.h"
#include "LetsPlayUser.h"
#include "RetroCore.h"

using AlphaChannel = bool;
using ShouldUpdateVector = bool;
using KeyFrame = bool;

// Because you can't pass a pointer to a static instance of a class...
struct EmulatorControllerProxy {
    std::function<void(LetsPlayUser*)> addTurnRequest, userDisconnected,
        userConnected;
    std::function<Frame(bool)> getFrame;
    bool isReady{false};
};

// NOTE: Doesn't need to be thread safe, the only use of this is a member that
// is access protected by a mutex
struct Frame {
    // Width and height are in pixels
    std::uint32_t width{0}, height{0};

    // Packed array, RGB(a)
    std::variant<std::vector<RGBColor>, std::vector<RGBAColor>> colors;

    // If the pixel array has an alpha channel and is therefore a vector of
    // RGBAColors instead of RGBColor
    bool alphaChannel{false};

    // True if the size of the vector inside the colors variant doesn't match
    // the value of width * height. When this is true, the next call of GetFrame
    // will clear out the vector in colors and create a blank one that is width
    // * height size
    bool needsVectorUpdate{false};
};

#pragma pack(push, 1)
struct RGBColor {
    /*
     * 0-255 value
     */
    std::uint8_t r{0}, g{0}, b{0};
};

struct RGBAColor {
    /*
     * 0-255 value
     */
    std::uint8_t r{0}, g{0}, b{0}, a{0};
};
#pragma pack(pop)

struct VideoFormat {
    /*
     * Masks for red, green, blue, and alpha
     */
    std::atomic<std::uint16_t> rMask{0b1111100000000000},
        gMask{0b0000011111000000}, bMask{0b0000000000111110}, aMask{0b0};

    /*
     * Bit shifts for red, green, blue, and alpha
     */
    std::atomic<std::uint8_t> rShift{10}, gShift{5}, bShift{0}, aShift{15};

    /*
     * How many bits per pixel
     */
    std::atomic<std::uint8_t> bitsPerPel{16};

    /*
     * Width, height
     */
    std::atomic<std::uint32_t> width{0}, height{0}, pitch{0};
};

/*
 * Class to be used once per thread, manages a libretro core, smulator, and its
 * own turns through callbacks
 */
class EmulatorController {
    /*
     * Pointer to the server managing the emulator controller
     */
    static LetsPlayServer* m_server;

    /*
     * Turn queue for this emulator
     */
    static std::vector<LetsPlayUser*> m_TurnQueue;

    /*
     * Mutex for accessing the turn queue
     */
    static std::mutex m_TurnMutex;

    /*
     * Condition variable for waking up/sleeping the turn thread
     */
    static std::condition_variable m_TurnNotifier;

    /*
     * Keep the turn thread running
     */
    static std::atomic<bool> m_TurnThreadRunning;

    /*
     * Thread that runs EmulatorController::TurnThread()
     */
    static std::thread m_TurnThread;

    /*
     * ID of the emulator controller / emulator
     */
    static EmuID_t id;

    /*
     * Stores the masks and shifts required to generate a rgb 0xRRGGBB vector
     * from the video_refresh callback data
     */
    static VideoFormat m_videoFormat;

    /*
     * Key frame, a frame similar to a video compression key frame that contains
     * all the information for the last used frame. This is only updated when a
     * keyframe or deltaframe are requested.
     */
    static Frame m_keyFrame;

    /*
     * Pointer to the current video buffer
     */
    static const void* m_currentBuffer;

    /*
     * Mutex for accessing m_screen or m_nextFrame or updating the buffer
     */
    static std::mutex m_videoMutex;

    static retro_system_av_info m_avinfo;

   public:
    /*
     * Pointer to some functions that the managing server needs to call
     */
    static EmulatorControllerProxy proxy;

    /*
     * The object that manages the libretro lower level functions. Used mostly
     * for loading symbols and storing function pointers.
     */
    static RetroCore Core;

    static std::atomic<std::uint64_t> usersConnected;

    /*
     * Kind of the constructor. Blocks when called.
     */
    static void Run(const std::string& corePath, const std::string& romPath,
                    LetsPlayServer* server, EmuID_t t_id);

    // libretro_core -> Controller ?> Server
    /*
     * Callback for when the libretro core sends eztra info about the
     * environment
     * @return (?) Possibly if the command was recognized
     */
    static bool OnEnvironment(unsigned cmd, void* data);
    // Either:
    //  1) libretro_core -> Controller
    static void OnVideoRefresh(const void* data, unsigned width,
                               unsigned height, size_t stride);
    // Controller -> Server.getInput (input is TOGGLE)
    static void OnPollInput();
    // Controller -> libretro_core
    static std::int16_t OnGetInputState(unsigned port, unsigned device,
                                        unsigned index, unsigned id);
    // Controller -> Server
    static void OnLRAudioSample(std::int16_t left, std::int16_t right);
    // Controller -> Server
    static size_t OnBatchAudioSample(const std::int16_t* data, size_t frames);

    /*
     * Thread that manages the turns for the users that are connected to the
     * emulator
     */
    static void TurnThread();

    /*
     * Adds a user to the turn request queue, invoked by parent LetsPlayServer
     */
    static void AddTurnRequest(LetsPlayUser* user);

    /*
     * Called when a user disonnects, updates turn queue if applicable
     */
    static void UserDisconnected(LetsPlayUser* user);

    /*
     * Called when a user connects
     */
    static void UserConnected(LetsPlayUser* user);

    /*
     * Called when the emulator requests/announces a change in the pixel format
     */
    static bool SetPixelFormat(const retro_pixel_format fmt);

    /*
     * Function that overlays fg (possibly containing transparebt pixels) on top
     * of bg (assumed to contain all opaque pixels)
     */
    static void Overlay(Frame& fg, Frame& bg);

    /*
     * Gets a key frame based on the current video buffer, updates m_keyFrame
     */
    static Frame GetFrame(bool isKeyFrame);

    static void SaveImage();
};
