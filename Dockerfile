# Build CLI for example render
FROM gcc:13 AS renderer
WORKDIR /build
COPY src/raytracer.cpp .
RUN g++ -O3 -std=c++17 -static -o raytracer raytracer.cpp -lm
RUN ./raytracer -w 1920 -h 1080 -s 4 -o render.bmp

# Serve website + downloads
FROM nginx:alpine
COPY web/ /usr/share/nginx/html/
COPY --from=renderer /build/render.bmp /usr/share/nginx/html/images/render.bmp
COPY src/raytracer_engine.h /usr/share/nginx/html/download/src/raytracer_engine.h
COPY src/main_gui.cpp /usr/share/nginx/html/download/src/main_gui.cpp
COPY src/raytracer.cpp /usr/share/nginx/html/download/src/raytracer.cpp

RUN mkdir -p /usr/share/nginx/html/download/src /usr/share/nginx/html/images

RUN printf 'server {\n\
    listen 80;\n\
    root /usr/share/nginx/html;\n\
    index index.html;\n\
    client_max_body_size 200M;\n\
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
