# Stage 1: Build the GUI app
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ make wget ca-certificates \
    libsdl2-dev libgl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

# Download Dear ImGui v1.90.1
RUN wget -q https://raw.githubusercontent.com/ocornut/imgui/v1.90.1/imgui.cpp \
    && wget -q https://raw.githubusercontent.com/ocornut/imgui/v1.90.1/imgui.h \
    && wget -q https://raw.githubusercontent.com/ocornut/imgui/v1.90.1/imgui_draw.cpp \
    && wget -q https://raw.githubusercontent.com/ocornut/imgui/v1.90.1/imgui_tables.cpp \
    && wget -q https://raw.githubusercontent.com/ocornut/imgui/v1.90.1/imgui_widgets.cpp \
    && wget -q https://raw.githubusercontent.com/ocornut/imgui/v1.90.1/imgui_internal.h \
    && wget -q https://raw.githubusercontent.com/ocornut/imgui/v1.90.1/imstb_rectpack.h \
    && wget -q https://raw.githubusercontent.com/ocornut/imgui/v1.90.1/imstb_textedit.h \
    && wget -q https://raw.githubusercontent.com/ocornut/imgui/v1.90.1/imstb_truetype.h \
    && wget -q https://raw.githubusercontent.com/ocornut/imgui/v1.90.1/imconfig.h \
    && wget -q https://raw.githubusercontent.com/ocornut/imgui/v1.90.1/backends/imgui_impl_sdl2.cpp \
    && wget -q https://raw.githubusercontent.com/ocornut/imgui/v1.90.1/backends/imgui_impl_sdl2.h \
    && wget -q https://raw.githubusercontent.com/ocornut/imgui/v1.90.1/backends/imgui_impl_opengl3.cpp \
    && wget -q https://raw.githubusercontent.com/ocornut/imgui/v1.90.1/backends/imgui_impl_opengl3.h \
    && wget -q https://raw.githubusercontent.com/ocornut/imgui/v1.90.1/backends/imgui_impl_opengl3_loader.h

COPY src/raytracer_engine.h .
COPY src/main_gui.cpp .
COPY src/raytracer.cpp .

# Build GUI version
RUN g++ -O3 -std=c++17 -I. \
    main_gui.cpp \
    imgui.cpp imgui_draw.cpp imgui_tables.cpp imgui_widgets.cpp \
    imgui_impl_sdl2.cpp imgui_impl_opengl3.cpp \
    -o raytracer-gui \
    $(sdl2-config --cflags --libs) -lGL -lm -lpthread

# Build CLI version (static)
RUN g++ -O3 -std=c++17 -static -o raytracer-cli raytracer.cpp -lm

# Generate example render with CLI
RUN ./raytracer-cli -w 1920 -h 1080 -s 4 -o render.bmp

# Stage 2: Serve website + binaries
FROM nginx:alpine
COPY web/ /usr/share/nginx/html/
COPY --from=builder /build/raytracer-gui /usr/share/nginx/html/download/raytracer-gui
COPY --from=builder /build/raytracer-cli /usr/share/nginx/html/download/raytracer-cli
COPY --from=builder /build/render.bmp /usr/share/nginx/html/images/render.bmp
COPY src/raytracer_engine.h /usr/share/nginx/html/download/src/raytracer_engine.h
COPY src/main_gui.cpp /usr/share/nginx/html/download/src/main_gui.cpp
COPY src/raytracer.cpp /usr/share/nginx/html/download/src/raytracer.cpp

RUN printf 'server {\n\
    listen 80;\n\
    root /usr/share/nginx/html;\n\
    index index.html;\n\
\n\
    location /download/ {\n\
        add_header Content-Disposition "attachment";\n\
        types { application/octet-stream ""; }\n\
        default_type application/octet-stream;\n\
    }\n\
\n\
    location /download/src/ {\n\
        add_header Content-Disposition "attachment";\n\
        default_type text/plain;\n\
    }\n\
}\n' > /etc/nginx/conf.d/default.conf

RUN mkdir -p /usr/share/nginx/html/download/src /usr/share/nginx/html/images
