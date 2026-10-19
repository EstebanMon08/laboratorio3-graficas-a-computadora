#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

struct Vec2   { float x, y; };
struct Color  { float r, g, b; };
struct Vertex { Vec2 p; Color c; };


static float edge(const Vec2& a, const Vec2& b, const Vec2& p) {
    return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
}


static bool barycentric(const Vec2& A, const Vec2& B, const Vec2& C, const Vec2& p,
                        float& u, float& v, float& w) {
    float area = edge(A, B, C);
    if (std::fabs(area) < 1e-8f) return false;
    u = edge(B, C, p) / area;
    v = edge(C, A, p) / area;
    w = edge(A, B, p) / area;
    return true;
}


static uint32_t crc32(const uint8_t* d, size_t n, uint32_t crc = 0) {
    crc = ~crc;
    for (size_t i = 0; i < n; i++) {
        crc ^= d[i];
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (-(int32_t)(crc & 1)));
    }
    return ~crc;
}

static void put32(std::vector<uint8_t>& o, uint32_t v) {
    o.push_back(v >> 24); o.push_back(v >> 16); o.push_back(v >> 8); o.push_back(v);
}

static void chunk(FILE* f, const char* type, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> buf;
    put32(buf, (uint32_t)data.size());
    buf.insert(buf.end(), type, type + 4);
    buf.insert(buf.end(), data.begin(), data.end());
    uint32_t crc = crc32(buf.data() + 4, buf.size() - 4);
    put32(buf, crc);
    fwrite(buf.data(), 1, buf.size(), f);
}

static void writePNG(const std::string& name, int w, int h, const std::vector<uint8_t>& rgb) {
    FILE* f = fopen(name.c_str(), "wb");
    if (!f) { perror("fopen"); return; }
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);

    std::vector<uint8_t> ihdr;
    put32(ihdr, w); put32(ihdr, h);
    ihdr.push_back(8); ihdr.push_back(2); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    chunk(f, "IHDR", ihdr);


    std::vector<uint8_t> raw;
    raw.reserve((size_t)h * (w * 3 + 1));
    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        raw.insert(raw.end(), rgb.begin() + (size_t)y * w * 3, rgb.begin() + (size_t)(y + 1) * w * 3);
    }


    std::vector<uint8_t> z = {0x78, 0x01};
    size_t pos = 0;
    while (pos < raw.size()) {
        size_t len = std::min<size_t>(65535, raw.size() - pos);
        bool last = (pos + len == raw.size());
        z.push_back(last ? 1 : 0);
        z.push_back(len & 0xFF); z.push_back(len >> 8);
        z.push_back(~len & 0xFF); z.push_back((~len >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + len);
        pos += len;
    }
    uint32_t a = 1, b = 0;
    for (uint8_t c : raw) { a = (a + c) % 65521; b = (b + a) % 65521; }
    put32(z, (b << 16) | a);
    chunk(f, "IDAT", z);
    chunk(f, "IEND", {});
    fclose(f);
}


static void renderTriangle(const std::string& name, int W, int H,
                           const Vertex& A, const Vertex& B, const Vertex& C) {
    std::vector<uint8_t> img((size_t)W * H * 3, 0);

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            Vec2 p = {x + 0.5f, y + 0.5f};
            float u, v, w;
            if (!barycentric(A.p, B.p, C.p, p, u, v, w)) continue;
            if (u < 0 || v < 0 || w < 0) continue;

            float r = u * A.c.r + v * B.c.r + w * C.c.r;
            float g = u * A.c.g + v * B.c.g + w * C.c.g;
            float b = u * A.c.b + v * B.c.b + w * C.c.b;

            size_t i = ((size_t)y * W + x) * 3;
            img[i + 0] = (uint8_t)std::lround(std::clamp(r, 0.f, 1.f) * 255);
            img[i + 1] = (uint8_t)std::lround(std::clamp(g, 0.f, 1.f) * 255);
            img[i + 2] = (uint8_t)std::lround(std::clamp(b, 0.f, 1.f) * 255);
        }
    }
    writePNG(name, W, H, img);
    printf("Generado %s\n", name.c_str());
}

int main() {
    const int W = 512, H = 512;
    const Color R = {1, 0, 0}, G = {0, 1, 0}, B = {0, 0, 1};

    renderTriangle("triangulo1.png", W, H,
        {{60, 450}, R}, {{60, 60}, G}, {{450, 450}, B});

    renderTriangle("triangulo2.png", W, H,
        {{40, 400}, R}, {{300, 60}, G}, {{480, 300}, B});

    renderTriangle("triangulo3.png", W, H,
        {{450, 40}, R}, {{60, 480}, G}, {{500, 120}, B});

    return 0;
}
