#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <vector>
#include <bit>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <chrono>
#include <cstring>

float deltaTime = 0.0f;

namespace UTILS {
    char* numToString(char* out, uint32_t value) {
        if (value >= 100) {
            *out++ = (char)('0' + value / 100);
            value %= 100;
            *out++ = (char)('0' + value / 10);
            *out++ = (char)('0' + value % 10);
        } else if (value >= 10) {
            *out++ = (char)('0' + value / 10);
            *out++ = (char)('0' + value % 10);
        } else {
            *out++ = (char)('0' + value);
        } return out;
    }

    struct Vec3 { union { struct { float x, y, z; }; struct { float r, g, b; }; }; };
    const Vec3 terminalColors[] = {
        {0.154340f,  0.0f,       0.0f     }, // 0 Black
        {0.447614f, -0.026941f, -0.245971f}, // 1 Blue
        {0.616153f, -0.161618f,  0.124454f}, // 2 Green
        {0.651219f, -0.056342f, -0.124394f}, // 3 Aqua
        {0.522715f,  0.185680f,  0.089961f}, // 4 Red
        {0.459891f,  0.159223f, -0.123754f}, // 5 Purple
        {0.706233f, -0.004368f,  0.144370f}, // 6 Yellow
        {0.845222f,  0.0f,       0.0f     }, // 7 White
        {0.565836f,  0.0f,       0.0f     }, // 8 Gray
        {0.609303f, -0.024907f, -0.209631f}, // 9 Light Blue
        {0.717741f, -0.189895f,  0.146706f}, // 10 Light Green
        {0.809440f, -0.101591f, -0.027742f}, // 11 Light Aqua
        {0.630367f,  0.183025f,  0.066354f}, // 12 Light Red
        {0.527196f,  0.212667f, -0.097986f}, // 13 Light Purple
        {0.947207f, -0.021460f,  0.093477f}, // 14 Light Yellow
        {0.961151f,  0.0f,       0.0f     }, // 15 Bright White
    };

    struct NumText { char text[3]; uint8_t size; };
    struct ColorLine { Vec3 ab; float invLen; uint8_t color; };
    float terminalInvLen[16];
    float srgbToLinear[256];
    NumText numText[256];
    ColorLine colorLines[101];

    float distSq(const Vec3& a, const Vec3& b, float t = 1.0f) {
        float dr = a.r - b.r * t, dg = a.g - b.g * t, db = a.b - b.b * t;
        return dr * dr + dg * dg + db * db;
    }

    Vec3 linearToOKLab(const Vec3& c) {
        float r = cbrtf(0.4122214708f * c.r + 0.5363325363f * c.g + 0.0514459929f * c.b);
        float g = cbrtf(0.2119034982f * c.r + 0.6806995451f * c.g + 0.1073969566f * c.b);
        float b = cbrtf(0.0883024619f * c.r + 0.2817188376f * c.g + 0.6299787005f * c.b);
        return {
            0.2104542553f * r + 0.7936177850f * g - 0.0040720468f * b,
            1.9779984951f * r - 2.4285922050f * g + 0.4505937099f * b,
            0.0259040371f * r + 0.7827717662f * g - 0.8086757660f * b
        };
    }

    void initLUT() {
        float len[16];
        int colorLineCount = 0;
        for (int i = 0; i < 16; ++i) {
            len[i] = terminalColors[i].g * terminalColors[i].g + terminalColors[i].b * terminalColors[i].b;
            terminalInvLen[i] = 1.0f / distSq({0.0f, 0.0f, 0.0f}, terminalColors[i]);
        } for (int i = 0; i < 15; ++i) for (int j = i + 1; j < 16; ++j) {
            const Vec3& a = terminalColors[i], &b = terminalColors[j];
            float dot = a.g * b.g + a.b * b.b;
            if ((i == 0 && (j == 7 || j == 15)) || (i == 8 && j == 15) || dot < 0.0f && dot * dot > len[i] * len[j] * 0.4225f) continue;
            colorLines[colorLineCount++] = {{b.r - a.r, b.g - a.g, b.b - a.b}, 1.0f / distSq(a, b), (uint8_t)(i | (j << 4))};
        } for (int i = 0; i < 256; ++i) {
            float c = (float)i / 255.0f;
            srgbToLinear[i] = c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
            numText[i].size = (uint8_t)(numToString(numText[i].text, i == 127 ? 255 : i * 2) - numText[i].text);
        }
    }

    float cellPixelColor(const Vec3 c, char& color) {
        float bestDist = FLT_MAX;
        const ColorLine* best = colorLines;
        for (int i = 0; i < 101; ++i) {
            const ColorLine& p = colorLines[i];
            const Vec3& a = terminalColors[p.color & 15];
            Vec3 ac = {c.r - a.r, c.g - a.g, c.b - a.b};
            float t = std::clamp((ac.r * p.ab.r + ac.g * p.ab.g + ac.b * p.ab.b) * p.invLen, 0.0f, 1.0f);
            float dist = distSq(ac, p.ab, t);
            if (dist < bestDist) bestDist = dist, best = &p;
        }
        const Vec3& a = terminalColors[best->color & 15];
        Vec3 ac = {c.r - a.r, c.g - a.g, c.b - a.b};
        float t = (ac.r * best->ab.r + ac.g * best->ab.g + ac.b * best->ab.b) * best->invLen - 0.5f;
        color = t < 0 ? (best->color >> 4) | (best->color << 4) : best->color;
        return fabsf(t * 0.5f);
    }

    float charPixelColor(const Vec3 c, char& color) {
        const Vec3& a = terminalColors[0];
        float bestDist = FLT_MAX, bestT = 0.0f;
        for (int i = 1; i < 16; ++i) {
            const Vec3& b = terminalColors[i];
            Vec3 ab = {b.r - a.r, b.g - a.g, b.b - a.b};
            Vec3 ac = {c.r - a.r, c.g - a.g, c.b - a.b};
            float t = std::clamp((ac.r * ab.r + ac.g * ab.g + ac.b * ab.b) / distSq(a, b), 0.0f, 1.0f);
            float dist = distSq(ac, ab, t);
            if (dist < bestDist) bestDist = dist, bestT = t, color = i;
        } return (1.0f - bestT) * 0.25f;
    }
    
    float charPixel(const Vec3 c, char& color) {
        color = 0; return (1.0f - std::clamp((c.r - terminalColors[0].r) / (1.0f - terminalColors[0].r), 0.0f, 1.0f)) * 0.25f;
    }
    
    Vec3 averageColor(const uint8_t* frame, int area) {
        Vec3 c = {0.0f, 0.0f, 0.0f};
        for (int i = 0; i < area; ++i) {
            c.r += srgbToLinear[frame[i*4+0]];
            c.g += srgbToLinear[frame[i*4+1]];
            c.b += srgbToLinear[frame[i*4+2]];
        }
        c.r /= area; c.g /= area; c.b /= area;
        float maxC = std::max(c.r, std::max(c.g, c.b));
        if (maxC > 0.0f) c.r /= maxC, c.g /= maxC, c.b /= maxC;
        c.r = c.r <= 0.0031308f ? 12.92f * c.r : 1.055f * powf(c.r, 1.0f / 2.4f) - 0.055f;
        c.g = c.g <= 0.0031308f ? 12.92f * c.g : 1.055f * powf(c.g, 1.0f / 2.4f) - 0.055f;
        c.b = c.b <= 0.0031308f ? 12.92f * c.b : 1.055f * powf(c.b, 1.0f / 2.4f) - 0.055f;
        c.r = std::max(0.0f, 2.0f * c.r - 1.0f);
        c.g = std::max(0.0f, 2.0f * c.g - 1.0f);
        c.b = std::max(0.0f, 2.0f * c.b - 1.0f);
        return c;
    }
}

namespace System {
    constexpr uint32_t COLOR_DEPTH = 128;
    constexpr uint32_t COLOR_BITS = std::bit_width(COLOR_DEPTH - 1);

    struct WCObuffer {
        uint8_t modeID = 0, modeStride = 0;
        char* colorspace = nullptr;
        COORD frameSize = {0};
        uint8_t* frame = nullptr;
        std::vector<char> output;
        SMALL_RECT cells = { 1, 1, 1, 1 };
    };

    struct Console {
        HWND hwnd = nullptr;
        HANDLE hIn = nullptr, hOut = nullptr;
        bool isFocused = true, wasResized = true;
        COORD pos = {0}, size = {0};
        WCObuffer buffer;
    };

    struct Input {
        uint8_t key[256] = {0};
        COORD globalCursor, localCursor;
    };

    System::Console console; System::Input input;
    const std::string asciiList = " .:,~=+*xoV#$@";
    typedef float (*pPixelFunc)(const UTILS::Vec3 c, char& color);
    struct ColorModeData { char outType; pPixelFunc Space; uint8_t ID, Stride, HeightM;};
    static const ColorModeData modeData[5] = {
        { 'A', UTILS::charPixel,      0,  1, 1 },
        { 'W', UTILS::charPixelColor, 1,  2, 1 },
        { 'W', UTILS::cellPixelColor, 2,  2, 1 },
        { 'A', nullptr,               3, 11, 1 },
        { 'A', nullptr,               4, 11, 2 },
    };

    void generateColorspace(int direction) {
        static int colorspace = 4, pStride = 0, pScale = 0;
        static pPixelFunc prevSpace = nullptr;
        WCObuffer& buffer = console.buffer;
        colorspace = (colorspace + direction + 5) % 5;
        const ColorModeData& data = modeData[colorspace];
        pPixelFunc Space = data.Space;
        buffer.modeStride = data.Stride;
        buffer.frameSize = {buffer.cells.Right, (short)(buffer.cells.Bottom * data.HeightM)};
        buffer.modeID = data.ID;
        if (data.HeightM != pScale) {
            pScale = data.HeightM; delete[] buffer.frame;
            buffer.frame = new uint8_t[buffer.frameSize.X * buffer.frameSize.Y * 4]{};
        } if (buffer.modeStride != pStride) {
            pStride = buffer.modeStride; delete[] buffer.colorspace;
            buffer.colorspace = new char[COLOR_DEPTH * COLOR_DEPTH * COLOR_DEPTH * buffer.modeStride];
        } if (prevSpace != Space) {
            const bool useColor = (buffer.modeID != 0);
            const int asciiCount = asciiList.size();
            for (uint32_t R = 0; R < COLOR_DEPTH; ++R)
            for (uint32_t G = 0; G < COLOR_DEPTH; ++G)
            for (uint32_t B = 0; B < COLOR_DEPTH; ++B) {
                uint32_t index = (R * COLOR_DEPTH + G) * COLOR_DEPTH + B;
                if (Space) {
                    UTILS::Vec3 c = UTILS::linearToOKLab({UTILS::srgbToLinear[(R<<1)|(R>>6)], UTILS::srgbToLinear[(G<<1)|(G>>6)], UTILS::srgbToLinear[(B<<1)|(B>>6)]});
                    char color = 0;
                    int charIndex = (int)((1.0f - Space(c, color) * 4.0f) * (float)(asciiCount - 1) + 0.5f);
                    int outIndex = index * buffer.modeStride;
                    buffer.colorspace[outIndex] = asciiList[std::clamp(charIndex, 0, asciiCount - 1)];
                    if (useColor) buffer.colorspace[outIndex + 1] = color;
                } else {
                    char* start = buffer.colorspace + index * buffer.modeStride;
                    char* out = start;
                    const UTILS::NumText& A = UTILS::numText[R], &C = UTILS::numText[G], &D = UTILS::numText[B];
                    memcpy(out, A.text, A.size); out += A.size; *out++ = ';';
                    memcpy(out, C.text, C.size); out += C.size; *out++ = ';';
                    memcpy(out, D.text, D.size); out += D.size;
                    int size = (int)(out - start);
                    if (size < 11) start[10] = (char)(0x80 | size);
                }
            }
        } prevSpace = Space;
    }
    
    void appendBaseColor(WCObuffer& buffer) {
        UTILS::Vec3 c = UTILS::averageColor(buffer.frame, buffer.frameSize.X * buffer.frameSize.Y);
        char color[20] = "\033[38;2;";
        char* out = color + 7;
        out = UTILS::numToString(out, (uint32_t)(c.r * 255.0f + 0.5f)); *out++ = ';';
        out = UTILS::numToString(out, (uint32_t)(c.g * 255.0f + 0.5f)); *out++ = ';';
        out = UTILS::numToString(out, (uint32_t)(c.b * 255.0f + 0.5f)); *out++ = 'm';
        buffer.output.insert(buffer.output.end(), color, out);
    }

    void appendANSIColor(WCObuffer& buffer, const char* layerText, uint32_t& cacheColor, uint32_t textColor, uint32_t color, int padding = 0) {
        const char* colorText = buffer.colorspace + textColor;
        int size = ((colorText[10] & 0x80) ? (colorText[10] & 0x7F) : 11) + 8;
        int oldSize = buffer.output.size();
        buffer.output.resize(oldSize + size + padding);
        char* out = buffer.output.data() + oldSize;
        memcpy(out, layerText, size);
        memcpy(out + 7, colorText, size - 8);
        if (padding) memset(out + size, ' ', padding);
        cacheColor = color;
    }

    uint32_t getColor(const WCObuffer& buffer, int index, int x, int y) {
        uint32_t R = buffer.frame[index * 4 + 0];
        uint32_t G = buffer.frame[index * 4 + 1];
        uint32_t B = buffer.frame[index * 4 + 2];
        if (buffer.modeID <= 2) {
            static const int8_t dither[4] = {-4, 0, 0, 4};
            int bias = dither[((y & 1) << 1) | (x & 1)];
            int light = std::max((int)R, std::max((int)G, (int)B));
            bias = bias * std::min(light, 255 - light) / 128;
            R = std::clamp((int)R + bias, 0, 255);
            G = std::clamp((int)G + bias, 0, 255);
            B = std::clamp((int)B + bias, 0, 255);
        }
        R >>= 8 - COLOR_BITS;
        G >>= 8 - COLOR_BITS;
        B >>= 8 - COLOR_BITS;
        return ((R << (COLOR_BITS * 2)) | (G << COLOR_BITS) | B) * buffer.modeStride;
    }

    void printConsoleOutput() {
        WCObuffer& buffer = console.buffer;
        SMALL_RECT region = buffer.cells;
        if (modeData[buffer.modeID].outType == 'A') std::cout.write(buffer.output.data(), buffer.output.size());
        else WriteConsoleOutputW(console.hOut, (CHAR_INFO*)buffer.output.data(), {region.Right, region.Bottom}, { 0, 0 }, &region);
    }
    
    void buildConsoleOutput() {
        WCObuffer& buffer = console.buffer;
        const uint8_t mode = buffer.modeID;
        const int area = buffer.frameSize.X * buffer.frameSize.Y;
        const char prefix[] = "\033[f\033[0m";
        const char layer[] = "\033[38;2;mmmmmmmmmmmm\033[48;2;mmmmmmmmmmmm";
        const char glyph[] = {
            3, (char)0xE2, (char)0x96, (char)0x88, // 0: slotA=0 same = █
            3, (char)0xE2, (char)0x96, (char)0x80, // 1: slotA=0 diff = ▀
            1,        ' ',          0,          0, // 2: slotA=1 same = space
            3, (char)0xE2, (char)0x96, (char)0x84, // 3: slotA=1 diff = ▄
        };
        uint32_t cacheColor[2] = { UINT32_MAX, UINT32_MAX };
        uint32_t currColor[2] = { UINT32_MAX, UINT32_MAX };
        int run = 0;
    
        const bool WCO_output = modeData[mode].outType == 'W';
        if (!WCO_output) {
            buffer.output.assign(prefix, prefix + sizeof(prefix) - 1);
            if (mode == 0) appendBaseColor(buffer);
        } else if (buffer.output.size() != area * 4) buffer.output.assign(area * 4, 0);
    
        for (int y = 0; y < buffer.frameSize.Y; y += 1 + (mode == 4))
        for (int x = 0; x < buffer.frameSize.X; x += 1) {
            int index = y * buffer.frameSize.X + x;
            currColor[0] = getColor(buffer, index, x, y);
            if (mode == 0) {
                buffer.output.push_back(buffer.colorspace[currColor[0]]);
            } if (WCO_output) {
                buffer.output[index * 4] = buffer.colorspace[currColor[0] + 0];
                buffer.output[index * 4 + 2] = buffer.colorspace[currColor[0] + 1];
            } if (mode == 3) {
                if (currColor[0] != cacheColor[0] || index == area - 1) {
                    if (index == area - 1) run++;
                    appendANSIColor(buffer, layer + 19, cacheColor[0], index > 0 ? cacheColor[0] : currColor[0], currColor[0], run);
                    run = 0;
                } ++run;
            } if (mode == 4) {
                currColor[1] = getColor(buffer, index + buffer.frameSize.X, x, y + 1);
                int slot[2];
                slot[0] = -1 + (cacheColor[0] == currColor[0]) + (cacheColor[1] == currColor[0]) * 2;
                slot[1] = -1 + (cacheColor[0] == currColor[1]) + (cacheColor[1] == currColor[1]) * 2;
                if (currColor[0] == currColor[1]) slot[1] = slot[0];
                for (int j = 0; j < 2; ++j) if (slot[j] < 0) {
                    int other = j ^ 1;
                    slot[j] = (slot[other] >= 0) ? (slot[other] ^ 1) : 1;
                    slot[other] = (currColor[0] == currColor[1]) ? slot[j] : slot[other];
                    appendANSIColor(buffer, layer + 19 * slot[j], cacheColor[slot[j]], currColor[j], currColor[j]);
                }
                int glyphIndex = ((currColor[0] != currColor[1]) | (slot[0] << 1)) << 2;
                int oldSize = buffer.output.size();
                int size = glyph[glyphIndex];
                buffer.output.resize(oldSize + size);
                memcpy(buffer.output.data() + oldSize, glyph + glyphIndex + 1, size);
            }
        }
        printConsoleOutput();
    }
    
    void getDimensions() {
        WCObuffer& buffer = console.buffer;
        RECT rect;
        GetWindowRect(console.hwnd, &rect);
        COORD size = {(short)(rect.right - rect.left), (short)(rect.bottom - rect.top)};
        console.pos = {(short)rect.left, (short)rect.top};
        if (!input.key[VK_LCONTROL] && !input.key[VK_F3] && console.size.X == size.X && console.size.Y == size.Y) return;
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(console.hOut, &csbi);
        short newX = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        short newY = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        console.wasResized = newX != buffer.cells.Right || newY != buffer.cells.Bottom;
        if (!console.wasResized) return;
        console.size = size;
        system("cls");
        buffer.frameSize = { newX, (short)(newY * modeData[buffer.modeID].HeightM) };
        delete[] buffer.frame; buffer.frame = new uint8_t[buffer.frameSize.X * buffer.frameSize.Y * 4]{};
        buffer.cells = { 0, 0, newX, newY };
    }
    
    void getInput() {
        for (int i = 0; i < 256; ++i) input.key[i] = (GetAsyncKeyState(i) < 0) * (input.key[i] + (input.key[i] < 2));
        POINT cursor;
        GetCursorPos(&cursor);
        input.globalCursor = { (short)cursor.x, (short)cursor.y };
        const short o[4] = { 14, 46, 50, 62 };
        short x = (cursor.x - console.pos.X - o[0]) * console.buffer.cells.Right;
        short y = (cursor.y - console.pos.Y - o[2]) * console.buffer.cells.Bottom;
        input.localCursor = { (short)(x / (console.size.X - o[1]) - (x < 0)), (short)(y / (console.size.Y - o[3]) - (y < 0)) };
    }

    void update() {
        static auto previous = std::chrono::high_resolution_clock::now();
        static float titleTimer = 0.0f;
        auto current = std::chrono::high_resolution_clock::now();
        deltaTime = std::chrono::duration<float>(current - previous).count();
        previous = current;
        titleTimer += deltaTime;
        if (titleTimer >= 0.5f) {
            char title[32];
            sprintf_s(title, "CMD Prompt - %d fps", (int)(1.0f / deltaTime + 0.5f));
            SetConsoleTitleA(title);
            titleTimer = 0.0f;
        }
        console.isFocused = (console.hwnd == GetForegroundWindow());
        getInput();
        getDimensions();
        if (input.key[VK_F3] == 1) generateColorspace(input.key[VK_CONTROL] ? -1 : 1);
    }
        
    bool init() {
        SetConsoleTitleA("CMD Prompt");
        ULONGLONG s = GetTickCount64();
        while (!(console.hwnd = FindWindowA(nullptr, "CMD Prompt"))) { if (GetTickCount64() - s > 3000) return false; Sleep(5); }
        console.hIn = GetStdHandle(STD_INPUT_HANDLE);
        console.hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (console.hIn == INVALID_HANDLE_VALUE || console.hOut == INVALID_HANDLE_VALUE) return false;
        DWORD inMode = 0, outMode = 0;
        GetConsoleMode(console.hIn, &inMode);
        GetConsoleMode(console.hOut, &outMode);
        inMode |= (ENABLE_QUICK_EDIT_MODE | ENABLE_LINE_INPUT | ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_VIRTUAL_TERMINAL_INPUT);
        inMode &= ~(ENABLE_WINDOW_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_INSERT_MODE | ENABLE_AUTO_POSITION);
        outMode |= (ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT);
        outMode &= ~(ENABLE_LVB_GRID_WORLDWIDE | DISABLE_NEWLINE_AUTO_RETURN);
        SetConsoleMode(console.hIn, inMode);
        SetConsoleMode(console.hOut, outMode);
        CONSOLE_CURSOR_INFO cursorInfo;
        GetConsoleCursorInfo(console.hOut, &cursorInfo);
        cursorInfo.bVisible = FALSE;
        SetConsoleCursorInfo(console.hOut, &cursorInfo);
        std::ios::sync_with_stdio(false);
        std::cout.tie(nullptr);
        SetConsoleOutputCP(CP_UTF8);
        UTILS::initLUT();
        getDimensions();
        generateColorspace(1);
        return true;
    }
}

namespace WindowMirror {
    HWND hwnd = nullptr;
    HDC sourceDC = nullptr, captureDC = nullptr, windowDC = nullptr;
    HBITMAP bitmap = nullptr, oldBitmap = nullptr, windowBitmap = nullptr, oldWindowBitmap = nullptr;
    uint8_t* pixels = nullptr;
    COORD size = {0};
    SIZE windowSize = {0};
    bool sync = false;

    void clearCapture() {
        if (windowBitmap) {
            SelectObject(windowDC, oldWindowBitmap);
            DeleteObject(windowBitmap);
        } if (bitmap) {
            SelectObject(captureDC, oldBitmap);
            DeleteObject(bitmap);
        }
        if (windowDC) DeleteDC(windowDC);
        if (captureDC) DeleteDC(captureDC);
        if (sourceDC && hwnd) ReleaseDC(hwnd, sourceDC);
        windowBitmap = nullptr;
        oldWindowBitmap = nullptr;
        bitmap = nullptr;
        oldBitmap = nullptr;
        windowDC = nullptr;
        captureDC = nullptr;
        sourceDC = nullptr;
        pixels = nullptr;
        windowSize = {0};
        size = {0};
    }

    void resizeCapture() {
        System::WCObuffer& buffer = System::console.buffer;
        if (size.X == buffer.frameSize.X && size.Y == buffer.frameSize.Y) return;
        if (buffer.frameSize.X <= 0 || buffer.frameSize.Y <= 0) return;
        if (bitmap) {
            SelectObject(captureDC, oldBitmap);
            DeleteObject(bitmap);
        }
        struct DIBInfo {
            BITMAPINFOHEADER header;
            DWORD mask[3];
        } bmi = {};
        bmi.header.biSize = sizeof(BITMAPINFOHEADER);
        bmi.header.biWidth = buffer.frameSize.X;
        bmi.header.biHeight = -buffer.frameSize.Y;
        bmi.header.biPlanes = 1;
        bmi.header.biBitCount = 32;
        bmi.header.biCompression = BI_BITFIELDS;
        bmi.mask[0] = 0x000000FF;
        bmi.mask[1] = 0x0000FF00;
        bmi.mask[2] = 0x00FF0000;
        bitmap = CreateDIBSection(captureDC, (BITMAPINFO*)&bmi, DIB_RGB_COLORS, (void**)&pixels, nullptr, 0);
        oldBitmap = (HBITMAP)SelectObject(captureDC, bitmap);
        size = buffer.frameSize;
    }

    void resizeWindowCapture(int width, int height) {
        if (width <= 0 || height <= 0 || windowSize.cx == width && windowSize.cy == height) return;
        if (windowBitmap) {
            SelectObject(windowDC, oldWindowBitmap);
            DeleteObject(windowBitmap);
        }
        windowBitmap = CreateCompatibleBitmap(sourceDC, width, height);
        oldWindowBitmap = (HBITMAP)SelectObject(windowDC, windowBitmap);
        windowSize = {width, height};
    }

    bool resizeWindowCapture() {
        RECT client;
        if (!GetClientRect(hwnd, &client)) return false;
        resizeWindowCapture(client.right, client.bottom);
        return true;
    }

    void syncWindow() {
        System::WCObuffer& buffer = System::console.buffer;
        if (buffer.frameSize.X <= 0 || buffer.frameSize.Y <= 0) return;
        RECT console;
        if (!GetWindowRect(System::console.hwnd, &console)) return;
        const short o[4] = {14, 46, 50, 62};
        int displayW = console.right - console.left - o[1];
        int displayH = console.bottom - console.top - o[3];
        if (displayW <= 0 || displayH <= 0) return;
        float scale = std::max(
            (float)buffer.frameSize.X / displayW,
            (float)buffer.frameSize.Y / displayH
        );
        int width = (int)(displayW * scale + 0.5f);
        int height = (int)(displayH * scale + 0.5f);
        RECT client;
        if (!GetClientRect(hwnd, &client)) return;
        if (client.right != width || client.bottom != height) {
            RECT rect = {0, 0, width, height};
            DWORD style = (DWORD)GetWindowLongPtr(hwnd, GWL_STYLE);
            DWORD exStyle = (DWORD)GetWindowLongPtr(hwnd, GWL_EXSTYLE);
            AdjustWindowRectEx(&rect, style, FALSE, exStyle);
            SetWindowPos(hwnd, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
        }
        resizeWindowCapture(width, height);
    }

    void mouseInput() {
        static bool active = false;
        static POINT origin = {0};
        if (active) {
            if (System::input.key[VK_LBUTTON]) return;
            SetCursorPos(origin.x, origin.y);
            active = false;
            return;
        }
        if (System::input.key[VK_LBUTTON] != 1) return;
        RECT console, client;
        if (!GetWindowRect(System::console.hwnd, &console)) return;
        if (!GetClientRect(hwnd, &client) || client.right <= 0 || client.bottom <= 0) return;
        POINT cursor;
        GetCursorPos(&cursor);
        const short o[4] = {14, 46, 50, 62};
        int x = cursor.x - console.left - o[0];
        int y = cursor.y - console.top - o[2];
        int width = console.right - console.left - o[1];
        int height = console.bottom - console.top - o[3];
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        POINT target = {
            x * client.right / width,
            y * client.bottom / height
        };
        if (!ClientToScreen(hwnd, &target)) return;
        origin = cursor;
        active = true;
        SetForegroundWindow(hwnd);
        SetCursorPos(target.x, target.y);
    }

    void capture() {
        resizeCapture();
        if (!sync && !resizeWindowCapture()) return;
        if (!windowBitmap || !pixels) return;
        if (!PrintWindow(hwnd, windowDC, PW_CLIENTONLY | PW_RENDERFULLCONTENT)) return;
        StretchBlt(captureDC, 0, 0, size.X, size.Y, windowDC, 0, 0, windowSize.cx, windowSize.cy, SRCCOPY);
        memcpy(System::console.buffer.frame, pixels, size.X * size.Y * 4);
    }

    bool link(bool useSync) {
        HWND target = GetForegroundWindow();
        if (!target || target == System::console.hwnd) return false;
        sync = useSync;
        if (target == hwnd) {
            if (sync) syncWindow();
            else resizeWindowCapture();
            return true;
        }
        clearCapture();
        hwnd = target;
        sourceDC = GetDC(hwnd);
        if (!sourceDC) return false;
        captureDC = CreateCompatibleDC(sourceDC);
        windowDC = CreateCompatibleDC(sourceDC);
        if (!captureDC || !windowDC) {
            clearCapture();
            hwnd = nullptr;
            return false;
        }
        SetStretchBltMode(captureDC, COLORONCOLOR);
        if (sync) syncWindow();
        else if (!resizeWindowCapture()) {
            clearCapture();
            hwnd = nullptr;
            return false;
        }
        resizeCapture();
        return bitmap && windowBitmap;
    }

    void update() {
        if (System::input.key[VK_F4] == 1) link(System::input.key[VK_CONTROL]);
        if (!hwnd || !IsWindow(hwnd)) return;
        if (sync) syncWindow();
        mouseInput();
        capture();
    }

    bool init() {
        LONG_PTR style = GetWindowLongPtr(System::console.hwnd, GWL_EXSTYLE);
        SetWindowLongPtr(System::console.hwnd, GWL_EXSTYLE, (style | WS_EX_NOACTIVATE | WS_EX_APPWINDOW) & ~WS_EX_TOOLWINDOW);
        SetWindowPos(System::console.hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        return true;
    }

    void printInstructions() {
        static const char text[] =
            "\033[2J\033[H\033[92m"
            "F3  :  Cycle color mode (Ctrl = reverse)\n"
            "F4  :  Link focused window to terminal (Ctrl = Sync resolution. Faster, but may break some apps)\n";
        std::cout.write(text, sizeof(text) - 1);
    }
}

int main() {
    if (!System::init() || !WindowMirror::init()) return 1;
    WindowMirror::printInstructions();
    while (IsWindow(System::console.hwnd)) {
        System::update();
        WindowMirror::update();
        if (WindowMirror::hwnd) System::buildConsoleOutput();
    }

    WindowMirror::clearCapture(); WindowMirror::hwnd = nullptr;
    delete[] System::console.buffer.frame; System::console.buffer.frame = nullptr;
    delete[] System::console.buffer.colorspace; System::console.buffer.colorspace = nullptr;
    return 0;
}