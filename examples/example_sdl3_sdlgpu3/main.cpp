// Dear ImGui: standalone example application for SDL3 + SDL_GPU
// (Cleaned version with audio visualizer)
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

// Important note to the reader who wish to integrate imgui_impl_sdlgpu3.cpp/.h in their own engine/app.
// - Unlike other backends, the user must call the function ImGui_ImplSDLGPU_PrepareDrawData() BEFORE issuing a SDL_GPURenderPass containing ImGui_ImplSDLGPU_RenderDrawData.
//   Calling the function is MANDATORY, otherwise the ImGui will not upload neither the vertex nor the index buffer for the GPU. See imgui_impl_sdlgpu3.cpp for more info.

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include <SDL3/SDL.h>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <cmath>
#include <stdio.h>
#include "../chessnative2/EngineBase.h"
#include "../chessnative2/ChessEngine1.hpp"
#include "../chessnative2/ChessEngine2.hpp"
#include <unordered_map>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <sstream>
#include "../Controller/Control.hpp"

namespace {
    struct Adapter1 : controller::IChessEngine {
        engine::ChessEngine1 e;
        std::vector<std::string> legal_moves(const std::string& fen) override { return e.legal_moves_uci(fen); }
        std::string choose_move(const std::string& fen, int depth) override { return e.choose_move(fen, depth); }
        std::vector<std::pair<std::string,int>> root_search_scores(const std::string& fen, int depth) override { return e.root_search_scores(fen, depth); }
    };
    struct Adapter2 : controller::IChessEngine {
        engine::ChessEngine2 e2;
        std::vector<std::string> legal_moves(const std::string& fen) override { return e2.legal_moves_uci(fen); }
        std::string choose_move(const std::string& fen, int depth) override { return e2.choose_move(fen, depth); }
        std::vector<std::pair<std::string,int>> root_search_scores(const std::string& fen, int depth) override { return e2.root_search_scores(fen, depth); }
    };
}

// #ifdef _DEBUG
//#pragma comment(lib, "..\\..\\x64\\Debug\\chessnative2.lib")
// #else
//#pragma comment(lib, "..\\..\\x64\\Release\\chessnative2.lib")
// #endif

// Added missing piece texture container types.
struct PieceTex { SDL_GPUTexture* tex = nullptr; int w = 0; int h = 0; };
static std::unordered_map<std::string, PieceTex> piece_textures;

// Piece mapping from FEN char to texture key
static std::string PieceKey(char c) {
    switch (c) {
    case 'P': return "white-pawn"; case 'N': return "white-knight"; case 'B': return "white-bishop"; case 'R': return "white-rook"; case 'Q': return "white-queen"; case 'K': return "white-king";
    case 'p': return "black-pawn"; case 'n': return "black-knight"; case 'b': return "black-bishop"; case 'r': return "black-rook"; case 'q': return "black-queen"; case 'k': return "black-king";
    default: return "";
    }
}

// Load a PNG as an SDL_GPUTexture and upload via copy pass
static SDL_GPUTexture* LoadPNGTexture(SDL_GPUDevice* dev, const char* path)
{
    SDL_Surface* surf = SDL_LoadPNG(path);
    if (!surf) { printf("[Chess] Load fail %s: %s\n", path, SDL_GetError()); return nullptr; }

    SDL_GPUTextureFormat gpu_fmt = SDL_GetGPUTextureFormatFromPixelFormat(surf->format);
    if (gpu_fmt == SDL_GPU_TEXTUREFORMAT_INVALID)
    {
        printf("[Chess] Unsupported surface format for %s\n", path);
        SDL_DestroySurface(surf);
        return nullptr;
    }

    SDL_GPUTextureCreateInfo tci{};
    tci.type = SDL_GPU_TEXTURETYPE_2D;
    tci.format = gpu_fmt;
    tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tci.width = (Uint32)surf->w;
    tci.height = (Uint32)surf->h;
    tci.layer_count_or_depth = 1;
    tci.num_levels = 1;
    tci.sample_count = SDL_GPU_SAMPLECOUNT_1;
    SDL_GPUTexture* tex = SDL_CreateGPUTexture(dev, &tci);
    if (!tex) { printf("[Chess] SDL_CreateGPUTexture failed: %s\n", SDL_GetError()); SDL_DestroySurface(surf); return nullptr; }

    Uint32 upload_size = (Uint32)(surf->pitch * surf->h);
    SDL_GPUTransferBufferCreateInfo tbci{}; tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD; tbci.size = upload_size;
    SDL_GPUTransferBuffer* tbuf = SDL_CreateGPUTransferBuffer(dev, &tbci);
    if (!tbuf) { printf("[Chess] Create transfer buffer failed: %s\n", SDL_GetError()); SDL_ReleaseGPUTexture(dev, tex); SDL_DestroySurface(surf); return nullptr; }

    void* dst = SDL_MapGPUTransferBuffer(dev, tbuf, true);
    if (!dst) { printf("[Chess] Map transfer buffer failed: %s\n", SDL_GetError()); SDL_ReleaseGPUTransferBuffer(dev, tbuf); SDL_ReleaseGPUTexture(dev, tex); SDL_DestroySurface(surf); return nullptr; }
    std::memcpy(dst, surf->pixels, upload_size);
    SDL_UnmapGPUTransferBuffer(dev, tbuf);

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(dev);
    SDL_GPUCopyPass* cpass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTextureTransferInfo src{}; src.transfer_buffer = tbuf; src.offset = 0; src.pixels_per_row = 0; src.rows_per_layer = 0; // tightly packed
    SDL_GPUTextureRegion dstReg{}; dstReg.texture = tex; dstReg.mip_level = 0; dstReg.layer = 0; dstReg.x = 0; dstReg.y = 0; dstReg.z = 0; dstReg.w = (Uint32)surf->w; dstReg.h = (Uint32)surf->h; dstReg.d = 1;
    SDL_UploadToGPUTexture(cpass, &src, &dstReg, true);

    SDL_EndGPUCopyPass(cpass);
    SDL_SubmitGPUCommandBuffer(cmd);

    SDL_ReleaseGPUTransferBuffer(dev, tbuf);
    SDL_DestroySurface(surf);
    return tex;
}

// Parse board portion of FEN into 64 indices
static void ParseBoard(const char* fen, char out[64])
{
    for (int i = 0; i < 64; ++i) out[i] = '.';
    const char* p = fen; int sq = 56; // start rank8 file a = index 56
    while (*p && *p != ' ') {
        if (*p == '/') { sq -= 16; p++; continue; }
        if (*p >= '1' && *p <= '8') { sq += (*p - '0'); p++; continue; }
        out[sq++] = *p; p++;
    }
}

static int AlgebraicToIndex(const char* s)
{
    if (!s || s[0] < 'a' || s[0]>'h' || s[1] < '1' || s[1]>'8') return -1;
    int file = s[0] - 'a';
    int rank = s[1] - '1'; // 0 bottom, 7 top
    return rank * 8 + file;
}

static void ApplyUCIMoveToBoardSquares(const std::string& uci, char board[64])
{
    if (uci.size() < 4) return;
    int from = AlgebraicToIndex(uci.c_str());
    int to = AlgebraicToIndex(uci.c_str() + 2);
    if (from < 0 || to < 0) return;
    char piece = board[from]; if (piece == '.') return;
    // Clear source
    board[from] = '.';
    // Promotion?
    if (uci.size() >= 5) {
        char p = uci[4];
        if (piece >= 'A' && piece <= 'Z') p = (char)toupper((unsigned char)p); // white
        board[to] = p;
    }
    else {
        board[to] = piece;
    }
    // Handle simple castling rook move
    if (piece == 'K') {
        // white short e1g1, long e1c1
        if (!strcmp(uci.c_str(), "e1g1")) { board[5] = 'R'; board[7] = '.'; }
        else if (!strcmp(uci.c_str(), "e1c1")) { board[3] = 'R'; board[0] = '.'; }
    }
    else if (piece == 'k') {
        if (!strcmp(uci.c_str(), "e8g8")) { board[61] = 'r'; board[63] = '.'; }
        else if (!strcmp(uci.c_str(), "e8c8")) { board[59] = 'r'; board[56] = '.'; }
    }
}

static std::string IndexToAlg(int idx) { int f = idx % 8; int r = idx / 8; return std::string(1, char('a' + f)) + char('1' + r); }

// Build minimal FEN from board array and side to move
static std::string BuildFEN(const char board[64], bool whiteToMove, int fullmove) {
    std::string s; for (int rank = 7; rank >= 0; --rank) { int empty = 0; for (int file = 0; file < 8; ++file) { int idx = rank * 8 + file; char pc = board[idx]; if (pc == '.') { empty++; } else { if (empty) { s.push_back(char('0' + empty)); empty = 0; } s.push_back(pc); } } if (empty) s.push_back(char('0' + empty)); if (rank) s.push_back('/'); }
    s += whiteToMove ? " w " : " b "; s += "KQkq - 0 "; s += std::to_string(fullmove); return s;
}
// Selection state
struct PendingMove { int from = -1; };

int main(int, char**)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return 1;
    }

    float dpi_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_Window* window = SDL_CreateWindow("ImGui SDL3 + SDL_GPU Visualizer", (int)(1280 * dpi_scale), (int)(800 * dpi_scale), SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) { printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError()); return 1; }
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    SDL_GPUDevice* gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB, true, nullptr);
    if (!gpu_device || !SDL_ClaimWindowForGPUDevice(gpu_device, window)) { printf("Error: GPU init: %s\n", SDL_GetError()); return 1; }
    SDL_SetGPUSwapchainParameters(gpu_device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(dpi_scale);
    style.FontScaleDpi = dpi_scale;

    ImGui_ImplSDL3_InitForSDLGPU(window);
    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device = gpu_device;
    init_info.ColorTargetFormat = SDL_GetGPUSwapchainTextureFormat(gpu_device, window);
    init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
    init_info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
    ImGui_ImplSDLGPU3_Init(&init_info);

    // Load chess piece textures
    {
        const char* names[] = { "white-pawn","white-knight","white-bishop","white-rook","white-queen","white-king","black-pawn","black-knight","black-bishop","black-rook","black-queen","black-king" };
        for (auto key : names)
        {
            std::string path = std::string("..\\example_sdl3_sdlgpu3\\content\\img\\") + key + ".png";
            SDL_GPUTexture* tex = LoadPNGTexture(gpu_device, path.c_str());
            if (tex) piece_textures[key] = { tex, 0, 0 };
        }
    }

    // Load WAV
    const char* wav_path = "content/NewYearsEveWaltz.wav";
    SDL_AudioSpec src_spec{}; Uint8* src_buf = nullptr; Uint32 src_len = 0;
    bool wav_ok = SDL_LoadWAV(wav_path, &src_spec, &src_buf, &src_len);
    if (!wav_ok) { printf("Warning: SDL_LoadWAV failed: %s\n", SDL_GetError()); }
    else { printf("Loaded WAV: %s (freq=%d channels=%d bytes=%u format=%u)\n", wav_path, src_spec.freq, src_spec.channels, src_len, (unsigned)src_spec.format); }

    // Desired playback/visualization format (mono float 48k)
    SDL_AudioSpec vis_spec{}; vis_spec.freq = 48000; vis_spec.format = SDL_AUDIO_F32; vis_spec.channels = 1;
    // Convert entire WAV into memory buffer for easier chunking
    std::vector<float> pcm; // converted mono float 48k
    if (wav_ok)
    {
        SDL_AudioStream* convert = SDL_CreateAudioStream(&src_spec, &vis_spec);
        if (!convert) { printf("Error: SDL_CreateAudioStream: %s\n", SDL_GetError()); }
        else {
            SDL_PutAudioStreamData(convert, src_buf, (int)src_len);
            SDL_FlushAudioStream(convert); // ensure conversion finishes
            int avail = SDL_GetAudioStreamAvailable(convert);
            pcm.resize(avail / sizeof(float));
            int got = SDL_GetAudioStreamData(convert, pcm.data(), avail);
            if (got != avail) pcm.resize(got / sizeof(float));
            SDL_DestroyAudioStream(convert);
            printf("Converted PCM samples: %zu\n", pcm.size());
        }
    }

    // Enumerate playback devices
    std::vector<SDL_AudioDeviceID> dev_ids; std::vector<std::string> dev_names;
    int dev_count = 0; SDL_AudioDeviceID* list = SDL_GetAudioPlaybackDevices(&dev_count);
    if (list && dev_count > 0)
    {
        for (int i = 0; i < dev_count; ++i) { dev_ids.push_back(list[i]); const char* n = SDL_GetAudioDeviceName(list[i]); dev_names.emplace_back(n ? n : "(unknown)"); }
        SDL_free(list);
    }
    else { printf("No playback devices found.\n"); SDL_free(list); }

    int current_dev_index = dev_ids.empty() ? -1 : 0;
    SDL_AudioStream* play_stream = nullptr; // device-backed stream

    auto OpenPlayback = [&](int idx) {
        if (play_stream) { SDL_PauseAudioStreamDevice(play_stream); SDL_DestroyAudioStream(play_stream); play_stream = nullptr; }
        if (idx < 0 || idx >= (int)dev_ids.size()) return;
        // Open device stream with desired playback spec (vis_spec)
        play_stream = SDL_OpenAudioDeviceStream(dev_ids[idx], &vis_spec, nullptr, nullptr);
        if (!play_stream) { printf("Failed to open device '%s': %s\n", dev_names[idx].c_str(), SDL_GetError()); return; }
        SDL_ResumeAudioStreamDevice(play_stream);
        printf("Opened playback device %d: %s (freq=%d channels=%d)\n", idx, dev_names[idx].c_str(), vis_spec.freq, vis_spec.channels);
        };
    if (current_dev_index >= 0) OpenPlayback(current_dev_index);

    // Playback state
    bool audio_playing = false;
    size_t play_cursor = 0; // index in pcm
    const float seconds_per_sample = (vis_spec.freq > 0) ? (1.0f / vis_spec.freq) : 0.0f;
    float audio_time = 0.0f;

    // Visualization state
    const int kFreqBins = 512; std::vector<float> freqAmps(kFreqBins, 0.0f); std::vector<float> phase(kFreqBins);
    { std::mt19937 rng{ 12345 }; std::uniform_real_distribution<float> dist(0.f, 6.28318f); for (int i = 0; i < kFreqBins; ++i) phase[i] = dist(rng); }

    ImVec4 clear_color = ImVec4(0.05f, 0.06f, 0.10f, 1.0f);
    auto last_tp = std::chrono::high_resolution_clock::now();

    bool done = false;
    bool show_chess_board = true;

    // Engine selection state
    Adapter1 adapter1; Adapter2 adapter2; int selectedEngine = 2; controller::IChessEngine* activeAdapter = &adapter2;
    controller::GameController game(*activeAdapter);

    int ply_depth = 4;
    bool engineWhite = true;
    PendingMove pending; // defined earlier
    std::string status_msg;
    static std::vector<std::string> activityLog; static int lastLoggedHumanFullmove = -1; static int lastLoggedEngineFullmove = -1;

    while (!done)
    {
        SDL_Event ev; while (SDL_PollEvent(&ev)) { ImGui_ImplSDL3_ProcessEvent(&ev); if (ev.type == SDL_EVENT_QUIT) done = true; if (ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && ev.window.windowID == SDL_GetWindowID(window)) done = true; }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) { SDL_Delay(10); continue; }
        auto now_tp = std::chrono::high_resolution_clock::now(); float frame_dt = std::chrono::duration<float>(now_tp - last_tp).count(); last_tp = now_tp; ImGui::GetIO().DeltaTime = frame_dt;

        // Feed playback device
        if (audio_playing && play_stream && !pcm.empty())
        {
            int queued_bytes = SDL_GetAudioStreamQueued(play_stream); // bytes currently queued in device stream
            int target_bytes = (int)(0.25f * vis_spec.freq * sizeof(float)); // keep ~250ms buffered
            while (queued_bytes < target_bytes)
            {
                const int chunk_samples = vis_spec.freq / 20; // 50ms chunk
                int remaining = (int)(pcm.size() - play_cursor);
                if (remaining <= 0)
                {
                    // Loop
                    play_cursor = 0; remaining = (int)(pcm.size()); printf("[Audio] Loop restart\n");
                }
                int send = (remaining < chunk_samples) ? remaining : chunk_samples;
                SDL_PutAudioStreamData(play_stream, &pcm[play_cursor], send * sizeof(float));
                play_cursor += send;
                queued_bytes = SDL_GetAudioStreamQueued(play_stream);
                audio_time += send * seconds_per_sample;
            }
        }

        // Update visualization amplitudes
        if (audio_playing && !pcm.empty())
        {
            // Sample a window of recent audio (approximate using play_cursor) for frequency energy.
            int window_samples = vis_spec.freq / 40; // 25ms snapshot
            int start = (int)play_cursor - window_samples; if (start < 0) start = 0;
            for (int i = 0; i < kFreqBins; ++i)
            {
                int idx = start + (i * window_samples) / kFreqBins;
                if (idx >= (int)pcm.size()) idx = (int)pcm.size() - 1;
                float s = pcm[idx]; float env = fabsf(s);
                freqAmps[i] = freqAmps[i] * 0.88f + env * 1.8f; // smooth
            }
        }
        else { for (float& v : freqAmps) v *= 0.92f; }

        ImGui_ImplSDLGPU3_NewFrame(); ImGui_ImplSDL3_NewFrame(); ImGui::NewFrame();

        if (show_chess_board)
        {
            ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, io.DisplaySize.y - 10), ImGuiCond_Always);
            if (ImGui::Begin("Chess Board", &show_chess_board, ImGuiWindowFlags_NoCollapse))
            {
                ImGui::Text("Engine (White) vs Human (Black)");
                // Engine dropdown and depth control
                const char* engineNames[] = { "Engine1", "Engine2" };
                ImGui::Combo("Engine", &selectedEngine, engineNames, IM_ARRAYSIZE(engineNames));
                if((selectedEngine==0 && activeAdapter!=&adapter1) || (selectedEngine==1 && activeAdapter!=&adapter2)){
                    activeAdapter = (selectedEngine==0)? (controller::IChessEngine*)&adapter1 : (controller::IChessEngine*)&adapter2;
                    game.set_engine(*activeAdapter); // keep current position
                }
                ImGui::InputInt("Ply Depth", &ply_depth); if (ply_depth < 1) ply_depth = 1; if (ply_depth > 10) ply_depth = 10;
                ImGui::SameLine(); if (ImGui::Button("New Game")) { game.reset(); pending.from = -1; activityLog.clear(); lastLoggedHumanFullmove = lastLoggedEngineFullmove = -1; status_msg = "New game"; }
                ImGui::SameLine(); if (ImGui::Button("Undo") && game.fen_history().size() > 1) { if (game.undo()) { pending.from = -1; status_msg = "Undo"; } }
                static char fenInput[256]; std::strncpy(fenInput, game.current_fen().c_str(), sizeof(fenInput)); fenInput[sizeof(fenInput) - 1] = '\0';
                ImGui::InputText("Load FEN", fenInput, sizeof(fenInput));
                ImGui::SameLine(); if (ImGui::Button("Apply FEN")) { if (!game.load_fen(fenInput)) status_msg = "Invalid FEN format"; else { pending.from = -1; activityLog.clear(); lastLoggedHumanFullmove = lastLoggedEngineFullmove = -1; status_msg = std::string("Loaded FEN (") + (game.white_to_move() ? "white" : "black") + " to move)"; } }
                ImGui::Separator();
                { char fenBuf[128]; std::strncpy(fenBuf, game.current_fen().c_str(), sizeof(fenBuf)); fenBuf[sizeof(fenBuf) - 1] = '\0'; ImGui::InputText("##fen", fenBuf, sizeof(fenBuf), ImGuiInputTextFlags_ReadOnly); }
                if (!status_msg.empty()) ImGui::TextWrapped("%s", status_msg.c_str());
                // Engine move & logging
                if (engineWhite && game.white_to_move()) {
                    if (lastLoggedEngineFullmove != game.fullmove_number()) {
                        auto movesList = game.legal_moves(); std::string line = std::string("Turn ") + std::to_string(game.fullmove_number()) + " White legal:";
                        if (ply_depth % 2 == 0) { auto scored = activeAdapter->root_search_scores(game.current_fen(), ply_depth); std::unordered_map<std::string, int> sm; for (auto& pr : scored) sm[pr.first.substr(0, 4)] = pr.second; for (auto& mv : movesList) { auto key = mv.substr(0, 4); auto it = sm.find(key); if (it != sm.end()) line += ' ' + mv + '(' + std::to_string(it->second) + ')'; else line += ' ' + mv; } }
                        else { for (auto& mv : movesList) line += ' ' + mv; }
                        activityLog.push_back(line); lastLoggedEngineFullmove = game.fullmove_number(); std::string chosen = game.engine_move(ply_depth); status_msg = chosen.empty() ? "Engine has no move" : std::string("Engine moves ") + chosen;
                    }
                }
                // Board layout sizing
                float availW = ImGui::GetContentRegionAvail().x; float availH = ImGui::GetContentRegionAvail().y; float squareSize = floorf((availH * 0.9f) / 8.0f); if (squareSize < 36) squareSize = 36; if (squareSize > 96) squareSize = 96; float boardPixel = squareSize * 8.0f; float spacing = 8.0f; float sideW = availW - boardPixel - spacing; if (sideW < 260) sideW = 260; if (boardPixel + spacing + sideW > availW) { sideW = availW - boardPixel - spacing; if (sideW < 200) sideW = 200; } float boardHeight = squareSize * 8.0f;

                ImGui::BeginGroup(); ImDrawList* dlBoard = ImGui::GetWindowDrawList(); ImVec2 boardPos = ImGui::GetCursorScreenPos();
                // Mouse input
                ImVec2 mouse = ImGui::GetIO().MousePos; bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left); int hoverSq = -1; if (mouse.x >= boardPos.x && mouse.y >= boardPos.y && mouse.x < boardPos.x + boardPixel && mouse.y < boardPos.y + boardHeight) { int file = int((mouse.x - boardPos.x) / squareSize); int rInv = int((mouse.y - boardPos.y) / squareSize); int rank = 7 - rInv; hoverSq = rank * 8 + file; }
                if (!game.white_to_move() && mouseClicked && hoverSq >= 0) { char pc = game.piece_at(hoverSq); if (pending.from < 0) { if (pc >= 'a' && pc <= 'z') { pending.from = hoverSq; status_msg = std::string("Selected ") + IndexToAlg(pending.from); } } else { if (pc >= 'a' && pc <= 'z') { pending.from = hoverSq; status_msg = std::string("Reselect ") + IndexToAlg(pending.from); } else { std::string tryUci = IndexToAlg(pending.from) + IndexToAlg(hoverSq); auto moves = game.legal_moves(); std::string legal; for (auto& m : moves) { if (m.rfind(tryUci, 0) == 0) { legal = m; break; } } if (!legal.empty()) { game.apply_human_move(legal); pending.from = -1; status_msg = std::string("Human moves ") + legal; } else { status_msg = "Illegal move"; pending.from = -1; } } } }
                if (!game.white_to_move() && lastLoggedHumanFullmove != game.fullmove_number()) { auto movesList = game.legal_moves(); std::string line = std::string("Turn ") + std::to_string(game.fullmove_number()) + " Black legal:"; if (ply_depth % 2 == 0) { auto scored = activeAdapter->root_search_scores(game.current_fen(), ply_depth); std::unordered_map<std::string, int> sm; for (auto& pr : scored) sm[pr.first.substr(0, 4)] = pr.second; for (auto& mv : movesList) { auto key = mv.substr(0, 4); auto it = sm.find(key); if (it != sm.end()) line += ' ' + mv + '(' + std::to_string(it->second) + ')'; else line += ' ' + mv; } } else { for (auto& mv : movesList) line += ' ' + mv; } activityLog.push_back(line); lastLoggedHumanFullmove = game.fullmove_number(); }
                const char* boardPtr = game.board();
                // Draw squares & pieces
                for (int rank = 7; rank >= 0; --rank) {
                    for (int file = 0; file < 8; ++file) {
                        int idx = rank * 8 + file; bool light = ((rank + file) & 1) == 0; ImU32 col = light ? IM_COL32(235, 235, 235, 255) : IM_COL32(90, 90, 110, 255);
                        ImVec2 p0(boardPos.x + file * squareSize, boardPos.y + (7 - rank) * squareSize);
                        ImVec2 p1(p0.x + squareSize, p0.y + squareSize);
                        dlBoard->AddRectFilled(p0, p1, col);
                        if (idx == pending.from) dlBoard->AddRect(p0, p1, IM_COL32(255, 215, 0, 255), 3.0f); else dlBoard->AddRect(p0, p1, IM_COL32(0, 0, 0, 80));
                        char pc = boardPtr[idx];
                        if (pc != '.') {
                            std::string key = PieceKey(pc); auto it = piece_textures.find(key);
                            if (it != piece_textures.end() && it->second.tex) {
                                ImTextureID tex_id = (ImTextureID)it->second.tex;
                                dlBoard->AddImage(tex_id, p0, p1, ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 255));
                            } else {
                                char buf[2] = { pc,0 }; ImVec2 ts = ImGui::CalcTextSize(buf); ImVec2 c((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f); ImVec2 pos(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f);
                                ImU32 tint = (pc >= 'A' && pc <= 'Z') ? IM_COL32(255, 255, 255, 255) : IM_COL32(30, 30, 30, 255);
                                dlBoard->AddText(ImVec2(pos.x + 1, pos.y + 1), IM_COL32(0, 0, 0, 120), buf);
                                dlBoard->AddText(pos, tint, buf);
                            }
                        }
                    }
                }
                ImGui::Dummy(ImVec2(boardPixel, boardHeight + 8.0f));
                ImGui::EndGroup(); // end board group

                // Side panel (single implementation)
                ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::PushItemWidth(sideW);
                ImGui::Text("PGN");
                static std::vector<char> pgnBufVec; {
                    std::string pgn = game.pgn(); if (pgn.size() > 4000) pgn.resize(4000);
                    pgnBufVec.assign(pgn.begin(), pgn.end()); pgnBufVec.push_back('\0');
                    ImGui::InputTextMultiline("##pgn", pgnBufVec.data(), pgnBufVec.size(), ImVec2(sideW, 100), ImGuiInputTextFlags_ReadOnly);
                }
                ImGui::Separator();
                ImGui::Text("Activity Log");
                float logHeight = boardHeight - 120.0f; if (logHeight < 120) logHeight = 120; if (logHeight > boardHeight) logHeight = boardHeight;
                static std::vector<char> logBuf; {
                    size_t totalLen = 0; for (auto &l : activityLog) totalLen += l.size() + 1; std::string combined; combined.reserve(totalLen + 1);
                    for (auto &l : activityLog) { combined += l; combined += '\n'; }
                    if (combined.empty()) combined = "(no entries)\n";
                    logBuf.assign(combined.begin(), combined.end()); logBuf.push_back('\0');
                    ImGui::InputTextMultiline("##activity", logBuf.data(), logBuf.size(), ImVec2(sideW, logHeight), ImGuiInputTextFlags_ReadOnly);
                }
                ImGui::Separator();
                ImGui::Text("Frame dt: %.3f ms (%.1f FPS)", frame_dt * 1000.0f, frame_dt > 0 ? 1.0f / frame_dt : 0.0f);
                ImGui::PopItemWidth();
                ImGui::EndGroup(); // end side panel
                ImGui::End(); // end window
            }
        }
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData(); bool minimized = (draw_data->DisplaySize.x <= 0 || draw_data->DisplaySize.y <= 0);
        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(gpu_device);
        SDL_GPUTexture* swap_tex; SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window, &swap_tex, nullptr, nullptr);
        if (swap_tex && !minimized) {
            ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmd);
            SDL_GPUColorTargetInfo target = {}; target.texture = swap_tex; target.clear_color = SDL_FColor{ clear_color.x, clear_color.y, clear_color.w }; target.load_op = SDL_GPU_LOADOP_CLEAR; target.store_op = SDL_GPU_STOREOP_STORE;
            SDL_GPURenderPass* rp = SDL_BeginGPURenderPass(cmd, &target, 1, nullptr);
            ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmd, rp);
            SDL_EndGPURenderPass(rp);
        }
        SDL_SubmitGPUCommandBuffer(cmd);
    }
    SDL_WaitForGPUIdle(gpu_device);
    if (play_stream) { SDL_PauseAudioStreamDevice(play_stream); SDL_DestroyAudioStream(play_stream); }
    if (src_buf) SDL_free(src_buf);
    for (auto &kv : piece_textures) if (kv.second.tex) SDL_ReleaseGPUTexture(gpu_device, kv.second.tex);
    ImGui_ImplSDL3_Shutdown(); ImGui_ImplSDLGPU3_Shutdown(); ImGui::DestroyContext();
    SDL_ReleaseWindowFromGPUDevice(gpu_device, window); SDL_DestroyGPUDevice(gpu_device); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}
