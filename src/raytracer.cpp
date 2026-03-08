#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <vector>
#include <string>
#include <cstdlib>

// ─── Vector3 ──────────────────────────────────────────────
struct Vec3 {
    double x, y, z;
    Vec3(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
    Vec3 operator+(const Vec3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    Vec3 operator-(const Vec3& v) const { return {x - v.x, y - v.y, z - v.z}; }
    Vec3 operator*(double t) const { return {x * t, y * t, z * t}; }
    Vec3 operator*(const Vec3& v) const { return {x * v.x, y * v.y, z * v.z}; }
    Vec3 operator/(double t) const { return {x / t, y / t, z / t}; }
    double dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }
    Vec3 cross(const Vec3& v) const {
        return {y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x};
    }
    double length() const { return std::sqrt(x * x + y * y + z * z); }
    Vec3 normalized() const { double l = length(); return l > 0 ? *this / l : Vec3(); }
    Vec3 reflect(const Vec3& n) const { return *this - n * 2.0 * this->dot(n); }
};

// ─── Ray ──────────────────────────────────────────────────
struct Ray {
    Vec3 origin, direction;
};

// ─── Material ─────────────────────────────────────────────
struct Material {
    Vec3 color;
    double ambient   = 0.1;
    double diffuse   = 0.7;
    double specular  = 0.5;
    double shininess = 32.0;
    double reflectivity = 0.0;
};

// ─── Hit Record ───────────────────────────────────────────
struct Hit {
    bool hit = false;
    double t = 1e18;
    Vec3 point, normal;
    Material material;
};

// ─── Sphere ───────────────────────────────────────────────
struct Sphere {
    Vec3 center;
    double radius;
    Material material;

    Hit intersect(const Ray& ray) const {
        Hit h;
        Vec3 oc = ray.origin - center;
        double a = ray.direction.dot(ray.direction);
        double b = 2.0 * oc.dot(ray.direction);
        double c = oc.dot(oc) - radius * radius;
        double disc = b * b - 4 * a * c;
        if (disc < 0) return h;
        double sqrtDisc = std::sqrt(disc);
        double t1 = (-b - sqrtDisc) / (2.0 * a);
        double t2 = (-b + sqrtDisc) / (2.0 * a);
        double t = (t1 > 0.001) ? t1 : ((t2 > 0.001) ? t2 : -1);
        if (t < 0) return h;
        h.hit = true;
        h.t = t;
        h.point = ray.origin + ray.direction * t;
        h.normal = (h.point - center).normalized();
        h.material = material;
        return h;
    }
};

// ─── Plane (checkerboard floor) ───────────────────────────
struct Plane {
    Vec3 point, normal;
    Material mat1, mat2;
    double scale;

    Plane(Vec3 p, Vec3 n, Material m1, Material m2, double s = 1.0)
        : point(p), normal(n.normalized()), mat1(m1), mat2(m2), scale(s) {}

    Hit intersect(const Ray& ray) const {
        Hit h;
        double denom = normal.dot(ray.direction);
        if (std::abs(denom) < 1e-6) return h;
        double t = (point - ray.origin).dot(normal) / denom;
        if (t < 0.001) return h;
        h.hit = true;
        h.t = t;
        h.point = ray.origin + ray.direction * t;
        h.normal = normal;
        if (denom > 0) h.normal = h.normal * -1;

        // Checkerboard pattern
        int fx = (int)std::floor(h.point.x * scale);
        int fz = (int)std::floor(h.point.z * scale);
        if (h.point.x < 0) fx--;
        if (h.point.z < 0) fz--;
        h.material = ((fx + fz) & 1) ? mat2 : mat1;
        return h;
    }
};

// ─── Light ────────────────────────────────────────────────
struct Light {
    Vec3 position;
    Vec3 color;
    double intensity;
};

// ─── Scene ────────────────────────────────────────────────
struct Scene {
    std::vector<Sphere> spheres;
    std::vector<Plane> planes;
    std::vector<Light> lights;
    Vec3 ambient_color = {0.05, 0.05, 0.1};

    Hit trace(const Ray& ray) const {
        Hit closest;
        for (auto& s : spheres) {
            Hit h = s.intersect(ray);
            if (h.hit && h.t < closest.t) closest = h;
        }
        for (auto& p : planes) {
            Hit h = p.intersect(ray);
            if (h.hit && h.t < closest.t) closest = h;
        }
        return closest;
    }

    bool inShadow(const Vec3& point, const Vec3& lightDir, double lightDist) const {
        Ray shadowRay = {point, lightDir};
        for (auto& s : spheres) {
            Hit h = s.intersect(shadowRay);
            if (h.hit && h.t < lightDist) return true;
        }
        for (auto& p : planes) {
            Hit h = p.intersect(shadowRay);
            if (h.hit && h.t < lightDist) return true;
        }
        return false;
    }

    Vec3 shade(const Ray& ray, int depth = 0) const {
        if (depth > 5) return ambient_color;

        Hit hit = trace(ray);
        if (!hit.hit) {
            // Sky gradient
            double t = 0.5 * (ray.direction.normalized().y + 1.0);
            return Vec3(0.02, 0.02, 0.05) * (1.0 - t) + Vec3(0.1, 0.1, 0.2) * t;
        }

        Vec3 color = hit.material.color * ambient_color * hit.material.ambient;

        for (auto& light : lights) {
            Vec3 toLight = light.position - hit.point;
            double lightDist = toLight.length();
            Vec3 lightDir = toLight.normalized();

            if (inShadow(hit.point, lightDir, lightDist)) continue;

            // Diffuse
            double diff = std::max(0.0, hit.normal.dot(lightDir));
            color = color + hit.material.color * light.color * (diff * hit.material.diffuse * light.intensity);

            // Specular (Blinn-Phong)
            Vec3 viewDir = (ray.origin - hit.point).normalized();
            Vec3 halfDir = (lightDir + viewDir).normalized();
            double spec = std::pow(std::max(0.0, hit.normal.dot(halfDir)), hit.material.shininess);
            color = color + light.color * (spec * hit.material.specular * light.intensity);
        }

        // Reflection
        if (hit.material.reflectivity > 0 && depth < 5) {
            Vec3 reflDir = ray.direction.normalized().reflect(hit.normal);
            Ray reflRay = {hit.point + hit.normal * 0.001, reflDir};
            Vec3 reflColor = shade(reflRay, depth + 1);
            color = color * (1.0 - hit.material.reflectivity) + reflColor * hit.material.reflectivity;
        }

        return color;
    }
};

// ─── BMP Writer ───────────────────────────────────────────
void writeBMP(const char* filename, const std::vector<uint8_t>& pixels, int w, int h) {
    int rowSize = ((w * 3 + 3) / 4) * 4;
    int imageSize = rowSize * h;
    int fileSize = 54 + imageSize;

    uint8_t header[54] = {};
    header[0] = 'B'; header[1] = 'M';
    std::memcpy(header + 2, &fileSize, 4);
    int offset = 54;
    std::memcpy(header + 10, &offset, 4);
    int infoSize = 40;
    std::memcpy(header + 14, &infoSize, 4);
    std::memcpy(header + 18, &w, 4);
    std::memcpy(header + 22, &h, 4);
    int16_t planes = 1;
    std::memcpy(header + 26, &planes, 2);
    int16_t bpp = 24;
    std::memcpy(header + 28, &bpp, 2);
    std::memcpy(header + 34, &imageSize, 4);

    FILE* f = std::fopen(filename, "wb");
    if (!f) { std::fprintf(stderr, "Error: Cannot open %s for writing\n", filename); return; }
    std::fwrite(header, 1, 54, f);

    std::vector<uint8_t> row(rowSize, 0);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int srcIdx = (y * w + x) * 3;
            row[x * 3 + 0] = pixels[srcIdx + 2]; // B
            row[x * 3 + 1] = pixels[srcIdx + 1]; // G
            row[x * 3 + 2] = pixels[srcIdx + 0]; // R
        }
        std::fwrite(row.data(), 1, rowSize, f);
    }
    std::fclose(f);
}

// ─── Tone mapping & gamma ─────────────────────────────────
uint8_t tonemap(double v) {
    v = v / (v + 1.0); // Reinhard
    v = std::pow(v, 1.0 / 2.2); // Gamma
    return (uint8_t)(std::min(std::max(v, 0.0), 1.0) * 255.0);
}

// ─── Main ─────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    int width = 1920;
    int height = 1080;
    int samples = 4; // AA samples per axis
    std::string output = "render.bmp";

    // Parse args
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "-w" && i + 1 < argc) width = std::atoi(argv[++i]);
        else if (std::string(argv[i]) == "-h" && i + 1 < argc) height = std::atoi(argv[++i]);
        else if (std::string(argv[i]) == "-s" && i + 1 < argc) samples = std::atoi(argv[++i]);
        else if (std::string(argv[i]) == "-o" && i + 1 < argc) output = argv[++i];
        else if (std::string(argv[i]) == "--help") {
            std::printf("Usage: raytracer [-w width] [-h height] [-s samples] [-o output.bmp]\n");
            std::printf("  -w  Image width  (default: 1920)\n");
            std::printf("  -h  Image height (default: 1080)\n");
            std::printf("  -s  AA samples per axis, total = s*s (default: 4)\n");
            std::printf("  -o  Output file  (default: render.bmp)\n");
            return 0;
        }
    }

    std::printf("Ray Tracer v1.0\n");
    std::printf("Rendering %dx%d with %dx AA...\n", width, height, samples * samples);

    // ─── Build scene ──────────────────────────────────────
    Scene scene;

    // Floor
    Material floorWhite = {{0.9, 0.9, 0.9}, 0.15, 0.6, 0.2, 16, 0.15};
    Material floorBlack = {{0.15, 0.15, 0.18}, 0.15, 0.6, 0.2, 16, 0.15};
    scene.planes.push_back(Plane({0, -1, 0}, {0, 1, 0}, floorWhite, floorBlack, 0.5));

    // Big reflective sphere (center)
    scene.spheres.push_back({
        {0, 0.5, 0}, 1.5,
        {{0.95, 0.95, 0.97}, 0.05, 0.2, 1.0, 128, 0.85}
    });

    // Red sphere
    scene.spheres.push_back({
        {-3.0, 0.0, -1.0}, 1.0,
        {{0.9, 0.1, 0.1}, 0.1, 0.8, 0.6, 64, 0.15}
    });

    // Green sphere
    scene.spheres.push_back({
        {2.5, -0.3, -1.5}, 0.7,
        {{0.1, 0.8, 0.2}, 0.1, 0.8, 0.5, 48, 0.1}
    });

    // Blue sphere
    scene.spheres.push_back({
        {1.5, -0.5, 2.0}, 0.5,
        {{0.15, 0.3, 0.9}, 0.1, 0.7, 0.8, 96, 0.25}
    });

    // Gold sphere
    scene.spheres.push_back({
        {-1.5, -0.6, 2.5}, 0.4,
        {{0.9, 0.75, 0.2}, 0.1, 0.6, 0.9, 128, 0.4}
    });

    // Purple sphere (back)
    scene.spheres.push_back({
        {3.5, 0.2, 1.0}, 1.2,
        {{0.6, 0.15, 0.7}, 0.1, 0.7, 0.5, 48, 0.2}
    });

    // Small white sphere
    scene.spheres.push_back({
        {-0.5, -0.75, 3.0}, 0.25,
        {{1.0, 1.0, 1.0}, 0.1, 0.6, 0.9, 128, 0.6}
    });

    // Lights
    scene.lights.push_back({{-5, 8, -3}, {1.0, 0.95, 0.9}, 0.8});
    scene.lights.push_back({{6, 6, 5}, {0.9, 0.92, 1.0}, 0.5});
    scene.lights.push_back({{0, 3, 8}, {0.8, 0.85, 1.0}, 0.3});

    // ─── Camera ───────────────────────────────────────────
    Vec3 camPos = {0, 3, 8};
    Vec3 camTarget = {0, 0, 0};
    Vec3 camUp = {0, 1, 0};
    double fov = 60.0;

    Vec3 forward = (camTarget - camPos).normalized();
    Vec3 right = forward.cross(camUp).normalized();
    Vec3 up = right.cross(forward);
    double aspect = (double)width / height;
    double fovRad = fov * M_PI / 180.0;
    double halfH = std::tan(fovRad / 2.0);
    double halfW = halfH * aspect;

    // ─── Render ───────────────────────────────────────────
    std::vector<uint8_t> pixels(width * height * 3);
    int lastPct = -1;

    for (int y = 0; y < height; y++) {
        int pct = (y * 100) / height;
        if (pct != lastPct) {
            std::printf("\rProgress: %3d%%", pct);
            std::fflush(stdout);
            lastPct = pct;
        }

        for (int x = 0; x < width; x++) {
            Vec3 color;

            for (int sy = 0; sy < samples; sy++) {
                for (int sx = 0; sx < samples; sx++) {
                    double u = ((x + (sx + 0.5) / samples) / width) * 2.0 - 1.0;
                    double v = ((y + (sy + 0.5) / samples) / height) * 2.0 - 1.0;

                    Vec3 dir = (forward + right * (u * halfW) + up * (v * halfH)).normalized();
                    Ray ray = {camPos, dir};
                    color = color + scene.shade(ray);
                }
            }

            double invSamples = 1.0 / (samples * samples);
            color = color * invSamples;

            int idx = (y * width + x) * 3;
            pixels[idx + 0] = tonemap(color.x);
            pixels[idx + 1] = tonemap(color.y);
            pixels[idx + 2] = tonemap(color.z);
        }
    }

    std::printf("\rProgress: 100%%\n");
    writeBMP(output.c_str(), pixels, width, height);
    std::printf("Saved to %s\n", output.c_str());

    return 0;
}
