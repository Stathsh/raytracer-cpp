#include "raytracer_engine.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"

#include <SDL.h>
#include <SDL_opengl.h>

#include <thread>
#include <memory>
#include <cstdio>

// ─── Globals ──────────────────────────────────────────────
static Scene g_scene;
static std::unique_ptr<RenderJob> g_job;
static std::thread g_renderThread;
static GLuint g_texture = 0;
static bool g_rendering = false;
static bool g_textureNeedsUpdate = false;
static int g_renderWidth = 800;
static int g_renderHeight = 450;
static int g_aaSamples = 2;
static int g_selectedSphere = 0;
static char g_saveFilename[256] = "render.bmp";
static bool g_saved = false;
static const char* g_presets[] = {"640x360", "800x450", "1280x720", "1920x1080", "2560x1440", "3840x2160"};
static int g_presetWidths[]  = {640, 800, 1280, 1920, 2560, 3840};
static int g_presetHeights[] = {360, 450,  720, 1080, 1440, 2160};
static int g_presetIdx = 1;

static const char* g_sphereNames[] = {
    "Mirror Sphere", "Red Sphere", "Green Sphere",
    "Blue Sphere", "Gold Sphere", "Purple Sphere", "White Sphere"
};

// ─── ImGui styling ────────────────────────────────────────
void setupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.TabRounding = 6.0f;
    style.WindowPadding = ImVec2(12, 12);
    style.FramePadding = ImVec2(8, 5);
    style.ItemSpacing = ImVec2(8, 8);

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg]          = ImVec4(0.08f, 0.08f, 0.12f, 1.0f);
    c[ImGuiCol_ChildBg]           = ImVec4(0.06f, 0.06f, 0.09f, 1.0f);
    c[ImGuiCol_PopupBg]           = ImVec4(0.10f, 0.10f, 0.15f, 0.95f);
    c[ImGuiCol_Border]            = ImVec4(0.20f, 0.20f, 0.30f, 0.5f);
    c[ImGuiCol_FrameBg]           = ImVec4(0.12f, 0.12f, 0.18f, 1.0f);
    c[ImGuiCol_FrameBgHovered]    = ImVec4(0.18f, 0.18f, 0.28f, 1.0f);
    c[ImGuiCol_FrameBgActive]     = ImVec4(0.22f, 0.22f, 0.35f, 1.0f);
    c[ImGuiCol_TitleBg]           = ImVec4(0.06f, 0.06f, 0.10f, 1.0f);
    c[ImGuiCol_TitleBgActive]     = ImVec4(0.10f, 0.10f, 0.18f, 1.0f);
    c[ImGuiCol_Tab]               = ImVec4(0.12f, 0.12f, 0.20f, 1.0f);
    c[ImGuiCol_TabHovered]        = ImVec4(0.28f, 0.28f, 0.50f, 1.0f);
    c[ImGuiCol_TabActive]         = ImVec4(0.22f, 0.22f, 0.40f, 1.0f);
    c[ImGuiCol_Button]            = ImVec4(0.25f, 0.22f, 0.45f, 1.0f);
    c[ImGuiCol_ButtonHovered]     = ImVec4(0.35f, 0.30f, 0.60f, 1.0f);
    c[ImGuiCol_ButtonActive]      = ImVec4(0.40f, 0.35f, 0.70f, 1.0f);
    c[ImGuiCol_Header]            = ImVec4(0.18f, 0.18f, 0.30f, 1.0f);
    c[ImGuiCol_HeaderHovered]     = ImVec4(0.25f, 0.25f, 0.42f, 1.0f);
    c[ImGuiCol_HeaderActive]      = ImVec4(0.30f, 0.30f, 0.50f, 1.0f);
    c[ImGuiCol_SliderGrab]        = ImVec4(0.40f, 0.35f, 0.70f, 1.0f);
    c[ImGuiCol_SliderGrabActive]  = ImVec4(0.50f, 0.45f, 0.80f, 1.0f);
    c[ImGuiCol_CheckMark]         = ImVec4(0.55f, 0.50f, 0.90f, 1.0f);
    c[ImGuiCol_Separator]         = ImVec4(0.20f, 0.20f, 0.30f, 0.5f);
    c[ImGuiCol_PlotHistogram]     = ImVec4(0.45f, 0.40f, 0.80f, 1.0f);
    c[ImGuiCol_Text]              = ImVec4(0.85f, 0.85f, 0.92f, 1.0f);
    c[ImGuiCol_TextDisabled]      = ImVec4(0.45f, 0.45f, 0.55f, 1.0f);
}

void startRender() {
    if (g_rendering) {
        if (g_job) g_job->cancel = true;
        if (g_renderThread.joinable()) g_renderThread.join();
    }

    g_renderWidth = g_presetWidths[g_presetIdx];
    g_renderHeight = g_presetHeights[g_presetIdx];

    g_job = std::make_unique<RenderJob>();
    g_job->scene = &g_scene;
    g_job->width = g_renderWidth;
    g_job->height = g_renderHeight;
    g_job->samples = g_aaSamples;
    g_rendering = true;
    g_saved = false;

    g_renderThread = std::thread([&]() {
        g_job->render();
        g_rendering = false;
    });
}

void updateTexture() {
    if (!g_job || g_job->pixels.empty()) return;

    if (g_texture == 0) {
        glGenTextures(1, &g_texture);
    }
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_job->width, g_job->height,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, g_job->pixels.data());
}

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow(
        "Ray Tracer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 800,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    setupStyle();

    // Load default scene
    g_scene = createDefaultScene();

    // Camera params for UI
    float camPosF[3] = {(float)g_scene.camPos.x, (float)g_scene.camPos.y, (float)g_scene.camPos.z};
    float camTgtF[3] = {(float)g_scene.camTarget.x, (float)g_scene.camTarget.y, (float)g_scene.camTarget.z};
    float fovF = (float)g_scene.fov;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)
                running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // ─── Full-window layout ──────────────────────────
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("##Main", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoBringToFrontOnFocus);

        float panelWidth = 320.0f;
        float availW = ImGui::GetContentRegionAvail().x;
        float availH = ImGui::GetContentRegionAvail().y;

        // ─── Left panel ──────────────────────────────────
        ImGui::BeginChild("##LeftPanel", ImVec2(panelWidth, availH), true);

        ImGui::TextColored(ImVec4(0.7f, 0.65f, 1.0f, 1.0f), "RAY TRACER");
        ImGui::Separator();
        ImGui::Spacing();

        // Render settings
        if (ImGui::CollapsingHeader("Render Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Combo("Resolution", &g_presetIdx, g_presets, IM_ARRAYSIZE(g_presets));
            ImGui::SliderInt("AA Samples", &g_aaSamples, 1, 8, "%dx%d");
            int totalSamples = g_aaSamples * g_aaSamples;
            ImGui::TextDisabled("  %d samples/pixel", totalSamples);
            ImGui::Spacing();

            if (g_rendering) {
                if (ImGui::Button("Cancel", ImVec2(-1, 36))) {
                    if (g_job) g_job->cancel = true;
                }
                int prog = g_job ? (int)g_job->progress : 0;
                ImGui::ProgressBar(prog / 100.0f, ImVec2(-1, 0));
                ImGui::TextDisabled("Rendering row %d / %d",
                    g_job ? (int)g_job->currentRow : 0,
                    g_presetHeights[g_presetIdx]);
            } else {
                ImVec4 greenBtn(0.15f, 0.55f, 0.25f, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, greenBtn);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.65f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.75f, 0.35f, 1.0f));
                if (ImGui::Button("Render", ImVec2(-1, 40))) {
                    startRender();
                }
                ImGui::PopStyleColor(3);
            }
        }

        ImGui::Spacing();

        // Camera
        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool camChanged = false;
            camChanged |= ImGui::DragFloat3("Position", camPosF, 0.1f);
            camChanged |= ImGui::DragFloat3("Target", camTgtF, 0.1f);
            camChanged |= ImGui::SliderFloat("FOV", &fovF, 20.0f, 120.0f);
            if (camChanged) {
                g_scene.camPos = {camPosF[0], camPosF[1], camPosF[2]};
                g_scene.camTarget = {camTgtF[0], camTgtF[1], camTgtF[2]};
                g_scene.fov = fovF;
            }
        }

        ImGui::Spacing();

        // Spheres
        if (ImGui::CollapsingHeader("Spheres", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Combo("##SphereSelect", &g_selectedSphere, g_sphereNames, IM_ARRAYSIZE(g_sphereNames));

            if (g_selectedSphere >= 0 && g_selectedSphere < (int)g_scene.spheres.size()) {
                Sphere& s = g_scene.spheres[g_selectedSphere];
                float pos[3] = {(float)s.center.x, (float)s.center.y, (float)s.center.z};
                float rad = (float)s.radius;
                float col[3] = {(float)s.material.color.x, (float)s.material.color.y, (float)s.material.color.z};
                float refl = (float)s.material.reflectivity;
                float shin = (float)s.material.shininess;

                ImGui::DragFloat3("Pos", pos, 0.1f);
                ImGui::DragFloat("Radius", &rad, 0.05f, 0.1f, 5.0f);
                ImGui::ColorEdit3("Color", col);
                ImGui::SliderFloat("Reflect", &refl, 0.0f, 1.0f);
                ImGui::SliderFloat("Shiny", &shin, 4.0f, 256.0f);
                ImGui::Checkbox("Enabled", &s.enabled);

                s.center = {pos[0], pos[1], pos[2]};
                s.radius = rad;
                s.material.color = {col[0], col[1], col[2]};
                s.material.reflectivity = refl;
                s.material.shininess = shin;
            }
        }

        ImGui::Spacing();

        // Save
        if (ImGui::CollapsingHeader("Export", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::InputText("Filename", g_saveFilename, sizeof(g_saveFilename));
            bool canSave = g_job && g_job->done && !g_job->pixels.empty();
            if (!canSave) ImGui::BeginDisabled();
            if (ImGui::Button("Save BMP", ImVec2(-1, 32))) {
                if (writeBMP(g_saveFilename, g_job->pixels.data(), g_job->width, g_job->height)) {
                    g_saved = true;
                }
            }
            if (!canSave) ImGui::EndDisabled();
            if (g_saved) {
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "Saved!");
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Drag values to edit. Click Render");
        ImGui::TextDisabled("to see changes. Ctrl+Q to quit.");

        ImGui::EndChild();

        ImGui::SameLine();

        // ─── Right panel: render preview ─────────────────
        float previewW = availW - panelWidth - ImGui::GetStyle().ItemSpacing.x;
        ImGui::BeginChild("##Preview", ImVec2(previewW, availH), true);

        if (g_job && !g_job->pixels.empty()) {
            updateTexture();
            if (g_texture) {
                // Fit image in available space maintaining aspect ratio
                float imgAspect = (float)g_job->width / g_job->height;
                float regionW = ImGui::GetContentRegionAvail().x;
                float regionH = ImGui::GetContentRegionAvail().y;
                float regionAspect = regionW / regionH;

                float drawW, drawH;
                if (imgAspect > regionAspect) {
                    drawW = regionW;
                    drawH = regionW / imgAspect;
                } else {
                    drawH = regionH;
                    drawW = regionH * imgAspect;
                }

                // Center
                float offsetX = (regionW - drawW) * 0.5f;
                float offsetY = (regionH - drawH) * 0.5f;
                ImVec2 cursorPos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(ImVec2(cursorPos.x + offsetX, cursorPos.y + offsetY));
                ImGui::Image((ImTextureID)(intptr_t)g_texture, ImVec2(drawW, drawH));
            }
        } else {
            float regionW = ImGui::GetContentRegionAvail().x;
            float regionH = ImGui::GetContentRegionAvail().y;
            ImVec2 textSize = ImGui::CalcTextSize("Click 'Render' to start");
            ImGui::SetCursorPos(ImVec2(
                (regionW - textSize.x) * 0.5f,
                (regionH - textSize.y) * 0.5f
            ));
            ImGui::TextDisabled("Click 'Render' to start");
        }

        ImGui::EndChild();
        ImGui::End();

        // Keyboard shortcut: Ctrl+Q to quit
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Q)) running = false;

        // Render
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.06f, 0.06f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    if (g_job) g_job->cancel = true;
    if (g_renderThread.joinable()) g_renderThread.join();
    if (g_texture) glDeleteTextures(1, &g_texture);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
