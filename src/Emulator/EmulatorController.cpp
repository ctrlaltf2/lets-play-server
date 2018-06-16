#include "EmulatorController.h"
#include <memory>

EmuID_t EmulatorController::id;
LetsPlayServer* EmulatorController::m_server{nullptr};
EmulatorControllerProxy EmulatorController::proxy;
RetroCore EmulatorController::Core;

std::vector<LetsPlayUser*> EmulatorController::m_TurnQueue;
std::mutex EmulatorController::m_TurnMutex;
std::condition_variable EmulatorController::m_TurnNotifier;
std::atomic<bool> EmulatorController::m_TurnThreadRunning;
std::thread EmulatorController::m_TurnThread;
std::atomic<std::uint64_t> EmulatorController::usersConnected{0};

VideoFormat EmulatorController::m_videoFormat;
Frame EmulatorController::m_keyFrame;
const void* EmulatorController::m_currentBuffer{nullptr};
std::mutex EmulatorController::m_videoMutex;
retro_system_av_info EmulatorController::m_avinfo;

void EmulatorController::Run(const std::string& corePath,
                             const std::string& romPath, LetsPlayServer* server,
                             EmuID_t t_id) {
    Core.Init(corePath.c_str());
    m_server = server;
    id = t_id;
    proxy = EmulatorControllerProxy{AddTurnRequest, UserDisconnected,
                                    UserConnected, GetFrame};
    m_server->AddEmu(id, &proxy);

    (*(Core.fSetEnvironment))(OnEnvironment);
    (*(Core.fSetVideoRefresh))(OnVideoRefresh);
    (*(Core.fSetInputPoll))(OnPollInput);
    (*(Core.fSetInputState))(OnGetInputState);
    (*(Core.fSetAudioSample))(OnLRAudioSample);
    (*(Core.fSetAudioSampleBatch))(OnBatchAudioSample);
    (*(Core.fInit))();

    // TODO: C++
    retro_system_info system = {0};
    retro_game_info info = {romPath.c_str(), 0};
    FILE* file = fopen(romPath.c_str(), "rb");

    if (!file) {
        std::cout << "invalid file" << '\n';
        return;
    }

    fseek(file, 0, SEEK_END);
    info.size = ftell(file);
    rewind(file);

    (*(Core.fGetSystemInfo))(&system);

    if (!system.need_fullpath) {
        info.data = malloc(info.size);

        if (!info.data || !fread((void*)info.data, info.size, 1, file)) {
            std::cout << "Some error about sizing" << '\n';
            fclose(file);
            return;
        }
    }

    if (!((*(Core.fLoadGame))(&info))) {
        std::cout << "failed to do a thing" << '\n';
    }

    m_TurnThreadRunning = true;
    m_TurnThread = std::thread(EmulatorController::TurnThread);

    (*(Core.fGetAudioVideoInfo))(&m_avinfo);
    unsigned msWait = (1.0 / m_avinfo.timing.fps) * 1000;

    // TODO: Manage this thread
    std::thread t([&]() {
        using namespace std::chrono;
        auto nextKeyFrame = steady_clock::now();

        while (true) {
            if (nextKeyFrame < steady_clock::now()) {
                server->SendFrame(id, frametype::key);
                nextKeyFrame = steady_clock::now() + seconds(2);
            } else {
                server->SendFrame(id, frametype::delta);
            }
            // Around 60 fps
            std::this_thread::sleep_for(milliseconds(33));
        }
    });
    t.detach();

    proxy.isReady = true;
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(msWait));
        (*(Core.fRun))();
    }
}

bool EmulatorController::OnEnvironment(unsigned cmd, void* data) {
    switch (cmd) {
        case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT: {
            const retro_pixel_format* fmt =
                static_cast<retro_pixel_format*>(data);

            if (*fmt > RETRO_PIXEL_FORMAT_RGB565) return false;

            return SetPixelFormat(*fmt);
        } break;
        case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO: {
            return false;
            //...?
        } break;
        default:
            return false;
    }
}

void EmulatorController::OnVideoRefresh(const void* data, unsigned width,
                                        unsigned height, size_t pitch) {
    std::unique_lock<std::mutex> lk(m_videoMutex);
    if (width != m_videoFormat.width || height != m_videoFormat.height ||
        pitch != m_videoFormat.pitch) {
        std::clog << "Screen Res changed from " << m_videoFormat.width << 'x'
                  << m_videoFormat.height << " to " << width << 'x' << height
                  << '\n';
        m_videoFormat.width = width;
        m_videoFormat.height = height;
        m_videoFormat.pitch = pitch;

        /*
         * Solves two problems, one of the frame vector size being wrong on res
         * change (and therefore not being able to be used for making delta
         * frames) and the problem of a key frame being needed when the
         * resolution changes (basically a scene change)
         */
        m_keyFrame.width = width;
        m_keyFrame.height = height;
        m_keyFrame.needsVectorUpdate = true;
    }
    m_currentBuffer = data;
}

void EmulatorController::OnPollInput() {}

std::int16_t EmulatorController::OnGetInputState(unsigned port, unsigned device,
                                                 unsigned index, unsigned id) {
    return 0;
}

void EmulatorController::OnLRAudioSample(std::int16_t left,
                                         std::int16_t right) {}

size_t EmulatorController::OnBatchAudioSample(const std::int16_t* data,
                                              size_t frames) {
    return frames;
}

void EmulatorController::TurnThread() {
    while (m_TurnThreadRunning) {
        std::unique_lock<std::mutex> lk((m_TurnMutex));

        // Wait for a nonempty queue
        while (m_TurnQueue.empty()) {
            m_TurnNotifier.wait(lk);
        }

        if (m_TurnThreadRunning == false) break;
        // XXX: Unprotected access to top of the queue, only access
        // threadsafe members
        auto& currentUser = m_TurnQueue[0];
        std::string username = currentUser->username();
        currentUser->hasTurn = true;
        m_server->BroadcastAll(id + ": " + username + " now has a turn!",
                               websocketpp::frame::opcode::binary);
        const auto turnEnd = std::chrono::steady_clock::now() +
                             std::chrono::seconds(c_turnLength);

        while (currentUser && currentUser->hasTurn &&
               (std::chrono::steady_clock::now() < turnEnd)) {
            // FIXME?: does turnEnd - std::chrono::steady_clock::now() cause
            // underflow or UB for the case where now() is greater than
            // turnEnd?
            m_TurnNotifier.wait_for(lk,
                                    turnEnd - std::chrono::steady_clock::now());
        }

        if (currentUser) {
            currentUser->hasTurn = false;
            currentUser->requestedTurn = false;
        }

        m_server->BroadcastAll(id + ": " + username + "'s turn has ended!",
                               websocketpp::frame::opcode::text);
        m_TurnQueue.erase(m_TurnQueue.begin());
    }
}

void EmulatorController::AddTurnRequest(LetsPlayUser* user) {
    std::unique_lock<std::mutex> lk((m_TurnMutex));
    m_TurnQueue.emplace_back(user);
    m_TurnNotifier.notify_one();
}

void EmulatorController::UserDisconnected(LetsPlayUser* user) {
    --usersConnected;
    std::unique_lock<std::mutex> lk((m_TurnMutex));
    auto& currentUser = m_TurnQueue[0];

    if (currentUser->hasTurn) {  // Current turn is user
        m_TurnNotifier.notify_one();
    } else if (currentUser->requestedTurn) {  // In the queue
        m_TurnQueue.erase(
            std::remove(m_TurnQueue.begin(), m_TurnQueue.end(), user),
            m_TurnQueue.end());
    };
    // Set the current user to nullptr so turnqueue knows not to try to
    // modify it, as it may be in an invalid state because the memory it
    // points to (managed by a std::map) may be reallocated as part of an
    // erase
    currentUser = nullptr;
}

void EmulatorController::UserConnected(LetsPlayUser* user) {
    ++usersConnected;
    return;
}

bool EmulatorController::SetPixelFormat(const retro_pixel_format fmt) {
    switch (fmt) {
        // TODO: Find a core that uses this and test it
        case RETRO_PIXEL_FORMAT_0RGB1555:  // 16 bit
            // rrrrrgggggbbbbba
            m_videoFormat.rMask = 0b1111100000000000;
            m_videoFormat.gMask = 0b0000011111000000;
            m_videoFormat.bMask = 0b0000000000111110;
            m_videoFormat.aMask = 0b0000000000000000;

            m_videoFormat.rShift = 10;
            m_videoFormat.gShift = 5;
            m_videoFormat.bShift = 0;
            m_videoFormat.aShift = 15;

            m_videoFormat.bitsPerPel = 16;
            return true;
        // TODO: Find a core that uses this and test it
        case RETRO_PIXEL_FORMAT_XRGB8888:  // 32 bit
            m_videoFormat.rMask = 0xff000000;
            m_videoFormat.gMask = 0x00ff0000;
            m_videoFormat.bMask = 0x0000ff00;
            m_videoFormat.aMask = 0x000000ff;

            m_videoFormat.rShift = 16;
            m_videoFormat.gShift = 8;
            m_videoFormat.bShift = 0;
            m_videoFormat.aShift = 24;

            m_videoFormat.bitsPerPel = 32;
            return true;
        case RETRO_PIXEL_FORMAT_RGB565:  // 16 bit
            // rrrrrggggggbbbbb
            m_videoFormat.rMask = 0b1111100000000000;
            m_videoFormat.gMask = 0b0000011111100000;
            m_videoFormat.bMask = 0b0000000000011111;
            m_videoFormat.aMask = 0b0000000000000000;

            m_videoFormat.rShift = 11;
            m_videoFormat.gShift = 5;
            m_videoFormat.bShift = 0;
            m_videoFormat.aShift = 16;

            m_videoFormat.bitsPerPel = 16;
            return true;
        default:
            return false;
    }
    return false;
}

void EmulatorController::Overlay(Frame& fg, Frame& bg) {
    if (!fg.alphaChannel || bg.alphaChannel) return;

    auto& fgData = std::get<1>(fg.colors);
    auto& bgData = std::get<0>(bg.colors);

    for (std::size_t i = 0; i < fgData.size(); ++i) {
        if (fgData[i].a) {
            bgData[i].r = fgData[i].r;
            bgData[i].g = fgData[i].g;
            bgData[i].b = fgData[i].b;
        }
    }
}

Frame EmulatorController::GetFrame(bool isKeyFrame) {
    std::variant<std::vector<RGBColor>, std::vector<RGBAColor>> outVec;

    if (isKeyFrame)
        outVec = std::vector<RGBColor>();
    else
        outVec = std::vector<RGBAColor>();

    std::unique_lock<std::mutex> lk(m_videoMutex);
    if (m_currentBuffer == nullptr) return Frame{0, 0, {}, false};
    if (m_keyFrame.needsVectorUpdate) {
        m_keyFrame.alphaChannel = false;
        m_keyFrame.colors =
            std::vector<RGBColor>(m_videoFormat.height * m_videoFormat.width);
    }

    const std::uint8_t* i = static_cast<const std::uint8_t*>(m_currentBuffer);
    for (size_t h = 0; h < m_videoFormat.height; ++h) {
        for (size_t w = 0; w < m_videoFormat.width; ++w) {
            std::uint32_t pixel{0};
            // Assuming little endian
            pixel |= *(i++);
            pixel |= *(i++) << 8;

            if (m_videoFormat.bitsPerPel == 32) {
                pixel |= *(i++) << 16;
                pixel |= *(i++) << 24;
            }

            // Calculate the rgb 0 - 255 values
            const std::uint8_t rMax =
                1 << (m_videoFormat.aShift - m_videoFormat.rShift);
            const std::uint8_t gMax =
                1 << (m_videoFormat.rShift - m_videoFormat.gShift);
            const std::uint8_t bMax =
                1 << (m_videoFormat.gShift - m_videoFormat.bShift);

            const std::uint8_t rVal =
                (pixel & m_videoFormat.rMask) >> m_videoFormat.rShift;
            const std::uint8_t gVal =
                (pixel & m_videoFormat.gMask) >> m_videoFormat.gShift;
            const std::uint8_t bVal =
                (pixel & m_videoFormat.bMask) >> m_videoFormat.bShift;

            const std::uint8_t rNormalized = (rVal / (double)rMax) * 255;
            const std::uint8_t gNormalized = (gVal / (double)gMax) * 255;
            const std::uint8_t bNormalized = (bVal / (double)bMax) * 255;

            // If delta frame mode
            if (!isKeyFrame) {
                const auto& i = h * m_videoFormat.height + w;

                // If the current frame's pixel is different from the last
                // frame's
                if ((std::get<0>(m_keyFrame.colors)[i].r != rNormalized) ||
                    (std::get<0>(m_keyFrame.colors)[i].g != gNormalized) ||
                    (std::get<0>(m_keyFrame.colors)[i].b != bNormalized)) {
                    std::get<1>(outVec).push_back(
                        RGBAColor{rNormalized, gNormalized, bNormalized, 255});
                } else {  // No change, 0, 0, 0, 0 it
                    /* TODO?: This will require GetFrame knowing what type of
                     * data its going to eventually output to so its not viable
                     * to do it here, but maybe 0, 0, 0, 0ing it isn't as good
                     * as copying the previous pixel's rgb data and setting a to
                     * 0 because lossless webp stores a cache of recently used
                     * colors and this *may* be a small optimization on image
                     * size which is what we want, requires testing to see if
                     * worth implementing
                     */
                    std::get<1>(outVec).push_back(RGBAColor{0, 0, 0, 0});
                }
            } else {
                std::get<0>(outVec).push_back(
                    RGBColor{rNormalized, gNormalized, bNormalized});
                isKeyFrame = true;
            }
        }
        // Stride is pitch / 2
        i += m_videoFormat.width - (m_videoFormat.pitch / 2);
    }

    Frame frame{m_videoFormat.width, m_videoFormat.height, outVec,
                AlphaChannel{!isKeyFrame}, ShouldUpdateVector{false}};

    if (isKeyFrame)
        m_keyFrame = frame;
    else
        Overlay(frame, /* on top of */ m_keyFrame);

    return frame;

    /*
            std::uint8_t* output{nullptr};
            size_t written = WebPEncodeLosslessRGB(
                outVec.data(), m_videoFormat.width, m_videoFormat.height,
                m_videoFormat.width * 3, &output);

            if (output) {
                std::string payload;
                for (size_t i = 0; i < written; ++i) {
                    payload += static_cast<char>(*(output + i));
                }

                m_server->BroadcastAll(payload,
       websocketpp::frame::opcode::binary); std::ofstream of(
                    std::string("screenshot") + std::to_string(imageNum++) +
                ".webp", std::ios::binary);
                of.write(reinterpret_cast<char*>(output), written);
                WebPFree(output);
            }*/
}
