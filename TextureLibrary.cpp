#include <windows.h>

// Undefine Windows min/max macros to prevent conflicts with std::min/std::max
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "TextureLibrary.h"
#include <algorithm>
#include <random>
#include <cmath>
#include <cassert>
#include <sstream>

// Static member initialization
TextureRenderer::BlendMode TextureRenderer::s_blendMode = TextureRenderer::Normal;

//=============================================================================
// Texture Implementation
//=============================================================================

Texture::Texture() : m_filter(TextureFilter::TF_Nearest), m_wrap(TextureWrap::TW_Repeat) {
}

Texture::Texture(const std::string& name, int width, int height, TextureFormat format)
    : m_name(name), m_data(width, height, 
      format == TextureFormat::RGB24 ? 3 : format == TextureFormat::RGBA32 ? 4 : 1, format),
      m_filter(TextureFilter::TF_Nearest), m_wrap(TextureWrap::TW_Repeat) {
}

Texture::~Texture() {
}

bool Texture::LoadFromFile(const std::string& filename) {
    // Simplified implementation - just create a placeholder texture
    m_name = filename;
    CreateSolid(64, 64, RGB(128, 128, 128));
    return true;
}

bool Texture::LoadFromBitmap(HBITMAP hBitmap) {
    if (!hBitmap) return false;
    
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);
    
    m_data.width = bmp.bmWidth;
    m_data.height = bmp.bmHeight;
    m_data.channels = 3;
    m_data.format = TextureFormat::RGB24;
    m_data.pixels.resize(m_data.width * m_data.height * m_data.channels);
    
    // Get bitmap data (improved)
    HDC hdc = CreateCompatibleDC(NULL);
    HBITMAP oldBmp = (HBITMAP)SelectObject(hdc, hBitmap);
    
    for (int y = 0; y < m_data.height; y++) {
        for (int x = 0; x < m_data.width; x++) {
            COLORREF pixel = ::GetPixel(hdc, x, y);  // Use global GetPixel, not class method
            int index = (y * m_data.width + x) * 3;
            m_data.pixels[index] = GetRValue(pixel);
            m_data.pixels[index + 1] = GetGValue(pixel);
            m_data.pixels[index + 2] = GetBValue(pixel);
        }
    }
    
    // Restore DC
    SelectObject(hdc, oldBmp);
    DeleteDC(hdc);
    return true;
}

bool Texture::LoadFromResource(HINSTANCE hInst, int resourceId) {
    HBITMAP hBitmap = LoadBitmap(hInst, MAKEINTRESOURCE(resourceId));
    if (!hBitmap) return false;
    
    bool result = LoadFromBitmap(hBitmap);
    DeleteObject(hBitmap);
    return result;
}

bool Texture::CreateSolid(int width, int height, COLORREF color) {
    assert(width > 0 && height > 0);

    m_data.width = width;
    m_data.height = height;
    m_data.channels = 3;
    m_data.format = TextureFormat::RGB24;
    m_data.pixels.resize(width * height * 3);
    
    // Validate internal buffer
    size_t expectedSize = (size_t)width * (size_t)height * (size_t)m_data.channels;
    assert(m_data.pixels.size() == expectedSize);

    BYTE r = GetRValue(color);
    BYTE g = GetGValue(color);
    BYTE b = GetBValue(color);

    // Entry debug log
    {
        std::ostringstream ss;
        ss << "Texture::CreateSolid ENTER name='" << m_name << "' size=" << width << "x" << height
           << " color=(" << (int)r << "," << (int)g << "," << (int)b << ") expectedSize=" << expectedSize << "\n";
        OutputDebugStringA(ss.str().c_str());
    }
    
    for (int i = 0; i < width * height; i++) {
        m_data.pixels[i * 3] = r;
        m_data.pixels[i * 3 + 1] = g;
        m_data.pixels[i * 3 + 2] = b;
    }
    
    // Exit debug log
    {
        std::ostringstream ss;
        ss << "Texture::CreateSolid EXIT name='" << m_name << "'\n";
        OutputDebugStringA(ss.str().c_str());
    }
    
    return true;
}

bool Texture::CreateGradient(int width, int height, COLORREF color1, COLORREF color2, bool horizontal) {
    assert(width > 0 && height > 0);
    // Avoid division by zero in interpolation
    if (horizontal) {
        assert(width > 1 && "CreateGradient horizontal requires width > 1");
    } else {
        assert(height > 1 && "CreateGradient vertical requires height > 1");
    }

    m_data.width = width;
    m_data.height = height;
    m_data.channels = 3;
    m_data.format = TextureFormat::RGB24;
    m_data.pixels.resize(width * height * 3);

    size_t expectedSize = (size_t)width * (size_t)height * (size_t)m_data.channels;
    assert(m_data.pixels.size() == expectedSize);
    
    BYTE r1 = GetRValue(color1), g1 = GetGValue(color1), b1 = GetBValue(color1);
    BYTE r2 = GetRValue(color2), g2 = GetGValue(color2), b2 = GetBValue(color2);

    // Entry debug log
    {
        std::ostringstream ss;
        ss << "Texture::CreateGradient ENTER name='" << m_name << "' size=" << width << "x" << height
           << " color1=(" << (int)r1 << "," << (int)g1 << "," << (int)b1 << ")"
           << " color2=(" << (int)r2 << "," << (int)g2 << "," << (int)b2 << ") horizontal=" << horizontal << "\n";
        OutputDebugStringA(ss.str().c_str());
    }
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float t = horizontal ? (float)x / (width - 1) : (float)y / (height - 1);
            
            BYTE r = (BYTE)(r1 + t * (r2 - r1));
            BYTE g = (BYTE)(g1 + t * (g2 - g1));
            BYTE b = (BYTE)(b1 + t * (b2 - b1));
            
            int index = (y * width + x) * 3;
            m_data.pixels[index] = r;
            m_data.pixels[index + 1] = g;
            m_data.pixels[index + 2] = b;
        }
    }
    
    // Exit debug log
    {
        std::ostringstream ss;
        ss << "Texture::CreateGradient EXIT name='" << m_name << "'\n";
        OutputDebugStringA(ss.str().c_str());
    }
    
    return true;
}

bool Texture::CreateNoise(int width, int height, int seed) {
    assert(width > 0 && height > 0);

    m_data.width = width;
    m_data.height = height;
    m_data.channels = 3;
    m_data.format = TextureFormat::RGB24;
    m_data.pixels.resize(width * height * 3);
    
    size_t expectedSize = (size_t)width * (size_t)height * (size_t)m_data.channels;
    assert(m_data.pixels.size() == expectedSize);

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> dist(0, 255);

    // Entry debug log
    {
        std::ostringstream ss;
        ss << "Texture::CreateNoise ENTER name='" << m_name << "' size=" << width << "x" << height
           << " seed=" << seed << "\n";
        OutputDebugStringA(ss.str().c_str());
    }
    
    for (int i = 0; i < (int)expectedSize; i++) {
        m_data.pixels[i] = (BYTE)dist(rng);
    }
    
    // Exit debug log
    {
        std::ostringstream ss;
        ss << "Texture::CreateNoise EXIT name='" << m_name << "'\n";
        OutputDebugStringA(ss.str().c_str());
    }
    
    return true;
}

bool Texture::CreateCheckerboard(int width, int height, int checkerSize, COLORREF color1, COLORREF color2) {
    assert(width > 0 && height > 0 && checkerSize > 0);

    m_data.width = width;
    m_data.height = height;
    m_data.channels = 3;
    m_data.format = TextureFormat::RGB24;
    m_data.pixels.resize(width * height * 3);
    
    size_t expectedSize = (size_t)width * (size_t)height * (size_t)m_data.channels;
    assert(m_data.pixels.size() == expectedSize);

    BYTE r1 = GetRValue(color1), g1 = GetGValue(color1), b1 = GetBValue(color1);
    BYTE r2 = GetRValue(color2), g2 = GetGValue(color2), b2 = GetBValue(color2);

    // Entry debug log
    {
        std::ostringstream ss;
        ss << "Texture::CreateCheckerboard ENTER name='" << m_name << "' size=" << width << "x" << height
           << " checkerSize=" << checkerSize << "\n";
        OutputDebugStringA(ss.str().c_str());
    }
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            bool useColor1 = ((x / checkerSize) + (y / checkerSize)) % 2 == 0;
            
            BYTE r = useColor1 ? r1 : r2;
            BYTE g = useColor1 ? g1 : g2;
            BYTE b = useColor1 ? b1 : b2;
            
            int index = (y * width + x) * 3;
            m_data.pixels[index] = r;
            m_data.pixels[index + 1] = g;
            m_data.pixels[index + 2] = b;
        }
    }
    
    // Exit debug log
    {
        std::ostringstream ss;
        ss << "Texture::CreateCheckerboard EXIT name='" << m_name << "'\n";
        OutputDebugStringA(ss.str().c_str());
    }
    
    return true;
}

COLORREF Texture::GetPixel(int x, int y) const {
    if (x < 0 || x >= m_data.width || y < 0 || y >= m_data.height) {
        return RGB(0, 0, 0);
    }
    
    int index = (y * m_data.width + x) * m_data.channels;
    if (m_data.channels >= 3) {
        return RGB(m_data.pixels[index], m_data.pixels[index + 1], m_data.pixels[index + 2]);
    } else {
        BYTE gray = m_data.pixels[index];
        return RGB(gray, gray, gray);
    }
}

void Texture::SetPixel(int x, int y, COLORREF color) {
    if (x < 0 || x >= m_data.width || y < 0 || y >= m_data.height) {
        // Log an out-of-bounds set attempt
        std::ostringstream ss;
        ss << "Texture::SetPixel out-of-bounds name='" << m_name << "' attempted at (" << x << "," << y << ")\n";
        OutputDebugStringA(ss.str().c_str());
        return;
    }

    int index = (y * m_data.width + x) * m_data.channels;

    // Basic invariants
    assert(m_data.channels >= 1);
    assert(index >= 0 && index + (m_data.channels - 1) < (int)m_data.pixels.size());

    if (m_data.channels >= 3) {
        m_data.pixels[index] = GetRValue(color);
        m_data.pixels[index + 1] = GetGValue(color);
        m_data.pixels[index + 2] = GetBValue(color);
    } else {
        BYTE gray = (BYTE)((GetRValue(color) + GetGValue(color) + GetBValue(color)) / 3);
        m_data.pixels[index] = gray;
    }

    // Debug log for successful set
    {
        std::ostringstream ss;
        ss << "Texture::SetPixel name='" << m_name << "' set at (" << x << "," << y << ")\n";
        OutputDebugStringA(ss.str().c_str());
    }
}

COLORREF Texture::Sample(float u, float v) const {
    return m_filter == TextureFilter::TF_Linear ? SampleLinear(u, v) : SampleNearest(u, v);
}

COLORREF Texture::SampleNearest(float u, float v) const {
    WrapCoordinates(u, v);
    int x = ClampCoordinate((int)(u * m_data.width), m_data.width);
    int y = ClampCoordinate((int)(v * m_data.height), m_data.height);
    return GetPixel(x, y);
}

COLORREF Texture::SampleLinear(float u, float v) const {
    WrapCoordinates(u, v);
    
    float fx = u * m_data.width - 0.5f;
    float fy = v * m_data.height - 0.5f;
    
    int x1 = ClampCoordinate((int)floor(fx), m_data.width);
    int y1 = ClampCoordinate((int)floor(fy), m_data.height);
    int x2 = ClampCoordinate(x1 + 1, m_data.width);
    int y2 = ClampCoordinate(y1 + 1, m_data.height);
    
    float tx = fx - floor(fx);
    float ty = fy - floor(fy);
    
    COLORREF c00 = GetPixel(x1, y1);
    COLORREF c10 = GetPixel(x2, y1);
    COLORREF c01 = GetPixel(x1, y2);
    COLORREF c11 = GetPixel(x2, y2);
    
    COLORREF c0 = BlendColors(c00, c10, tx);
    COLORREF c1 = BlendColors(c01, c11, tx);
    
    return BlendColors(c0, c1, ty);
}

void Texture::WrapCoordinates(float& u, float& v) const {
    switch (m_wrap) {
        case TextureWrap::TW_Repeat:
            u = u - floor(u);
            v = v - floor(v);
            break;
        case TextureWrap::TW_Clamp:
            u = (std::max)(0.0f, (std::min)(1.0f, u));
            v = (std::max)(0.0f, (std::min)(1.0f, v));
            break;
        case TextureWrap::TW_Mirror:
            u = u - floor(u);
            v = v - floor(v);
            if ((int)floor(u) % 2 == 1) u = 1.0f - u;
            if ((int)floor(v) % 2 == 1) v = 1.0f - v;
            break;
        default:
            break;
    }
}

int Texture::ClampCoordinate(int coord, int max) const {
    return (std::max)(0, (std::min)(max - 1, coord));
}

COLORREF Texture::BlendColors(COLORREF c1, COLORREF c2, float t) const {
    BYTE r1 = GetRValue(c1), g1 = GetGValue(c1), b1 = GetBValue(c1);
    BYTE r2 = GetRValue(c2), g2 = GetGValue(c2), b2 = GetBValue(c2);
    
    BYTE r = (BYTE)(r1 + t * (r2 - r1));
    BYTE g = (BYTE)(g1 + t * (g2 - g1));
    BYTE b = (BYTE)(b1 + t * (b2 - b1));
    
    return RGB(r, g, b);
}

HBITMAP Texture::CreateHBitmap(HDC hdc) const {
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = m_data.width;
    bmi.bmiHeader.biHeight = -m_data.height; // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;
    
    void* pBits;
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    
    if (hBitmap && pBits) {
        // Destination pitch (scanline) is DWORD aligned
        int srcStride = m_data.width * 3;
        int dstStride = ((srcStride + 3) / 4) * 4;
        BYTE* dst = (BYTE*)pBits;
        const BYTE* src = m_data.pixels.data();
        
        for (int y = 0; y < m_data.height; ++y) {
            // Copy one scanline from source (RGB) into destination.
            // The CreateDIBSection for 24bpp expects BGR order per pixel, but since we store RGB order,
            // we need to swap channels per pixel when copying.
            BYTE* dstLine = dst + y * dstStride;
            const BYTE* srcLine = src + y * srcStride;
            for (int x = 0; x < m_data.width; ++x) {
                BYTE r = srcLine[x * 3 + 0];
                BYTE g = srcLine[x * 3 + 1];
                BYTE b = srcLine[x * 3 + 2];
                // DIB 24-bit expects BGR byte order
                dstLine[x * 3 + 0] = b;
                dstLine[x * 3 + 1] = g;
                dstLine[x * 3 + 2] = r;
            }
            // If dstStride > srcStride, the remaining bytes are left unchanged (usually padding)
        }
    }
    
    return hBitmap;
}

//=============================================================================
// TextureAtlas Implementation
//=============================================================================

TextureAtlas::TextureAtlas(int width, int height) 
    : m_width(width), m_height(height), m_built(false) {
    m_atlasTexture.CreateSolid(width, height, RGB(0, 0, 0));
}

TextureAtlas::~TextureAtlas() {
}

bool TextureAtlas::AddTexture(const std::string& name, const Texture& texture) {
    if (m_built) return false;
    m_textures.emplace_back(name, texture);
    return true;
}

bool TextureAtlas::AddTexture(const std::string& name, const std::string& filename) {
    if (m_built) return false;
    Texture texture;
    if (texture.LoadFromFile(filename)) {
        m_textures.emplace_back(name, texture);
        return true;
    }
    return false;
}

const TextureAtlas::AtlasEntry* TextureAtlas::GetEntry(const std::string& name) const {
    auto it = m_entries.find(name);
    return it != m_entries.end() ? &it->second : nullptr;
}

bool TextureAtlas::Build() {
    if (m_built) return true;
    
    m_built = PackTextures();
    return m_built;
}

void TextureAtlas::Clear() {
    m_textures.clear();
    m_entries.clear();
    m_built = false;
    m_atlasTexture.CreateSolid(m_width, m_height, RGB(0, 0, 0));
}

bool TextureAtlas::PackTextures() {
    // Simple packing algorithm - just place textures in rows
    int currentX = 0, currentY = 0;
    int rowHeight = 0;
    
    for (const auto& pair : m_textures) {
        const std::string& name = pair.first;
        const Texture& texture = pair.second;
        
        int w = texture.GetWidth();
        int h = texture.GetHeight();
        
        // Check if we need to move to next row
        if (currentX + w > m_width) {
            currentX = 0;
            currentY += rowHeight;
            rowHeight = 0;
        }
        
        // Check if texture fits
        if (currentY + h > m_height) {
            return false; // Atlas too small
        }
        
        // Add entry
        AtlasEntry entry;
        entry.name = name;
        entry.x = currentX;
        entry.y = currentY;
        entry.width = w;
        entry.height = h;
        entry.u1 = (float)currentX / m_width;
        entry.v1 = (float)currentY / m_height;
        entry.u2 = (float)(currentX + w) / m_width;
        entry.v2 = (float)(currentY + h) / m_height;
        
        m_entries[name] = entry;
        
        // Copy texture data to atlas (simplified)
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                COLORREF pixel = texture.GetPixel(x, y);
                m_atlasTexture.SetPixel(currentX + x, currentY + y, pixel);
            }
        }
        
        currentX += w;
        rowHeight = (std::max)(rowHeight, h);
    }
    
    return true;
}

void TextureAtlas::ConvertUV(const std::string& textureName, float& u, float& v) const {
    const AtlasEntry* entry = GetEntry(textureName);
    if (entry) {
        u = entry->u1 + u * (entry->u2 - entry->u1);
        v = entry->v1 + v * (entry->v2 - entry->v1);
    }
}

//=============================================================================
// TextureLibrary Implementation
//=============================================================================

TextureLibrary::TextureLibrary() : m_maxCacheSize(100 * 1024 * 1024) { // 100MB default
}

TextureLibrary::~TextureLibrary() {
    UnloadAll();
}

TextureLibrary& TextureLibrary::Instance() {
    static TextureLibrary instance;
    return instance;
}

bool TextureLibrary::LoadTexture(const std::string& name, const std::string& filename) {
    auto texture = std::make_unique<Texture>();
    if (texture->LoadFromFile(filename)) {
        m_textures[name] = std::move(texture);
        CheckCacheLimit();
        return true;
    }
    return false;
}

bool TextureLibrary::LoadTextureFromResource(const std::string& name, HINSTANCE hInst, int resourceId) {
    auto texture = std::make_unique<Texture>();
    if (texture->LoadFromResource(hInst, resourceId)) {
        m_textures[name] = std::move(texture);
        CheckCacheLimit();
        return true;
    }
    return false;
}

bool TextureLibrary::CreateProceduralTexture(const std::string& name, int width, int height, 
                                            const std::string& type, const std::vector<COLORREF>& params) {
    auto texture = std::make_unique<Texture>();
    
    if (type == "solid" && params.size() >= 1) {
        texture->CreateSolid(width, height, params[0]);
    } else if (type == "gradient" && params.size() >= 2) {
        texture->CreateGradient(width, height, params[0], params[1]);
    } else if (type == "noise") {
        texture->CreateNoise(width, height);
    } else if (type == "checkerboard" && params.size() >= 2) {
        int checkerSize = params.size() >= 3 ? GetRValue(params[2]) : 16;
        texture->CreateCheckerboard(width, height, checkerSize, params[0], params[1]);
    } else {
        return false;
    }
    
    m_textures[name] = std::move(texture);
    CheckCacheLimit();
    return true;
}

Texture* TextureLibrary::GetTexture(const std::string& name) {
    auto it = m_textures.find(name);
    return it != m_textures.end() ? it->second.get() : nullptr;
}

const Texture* TextureLibrary::GetTexture(const std::string& name) const {
    auto it = m_textures.find(name);
    return it != m_textures.end() ? it->second.get() : nullptr;
}

void TextureLibrary::SetTexture(const std::string& name, std::unique_ptr<Texture> texture) {
    m_textures[name] = std::move(texture);
    CheckCacheLimit();
}

void TextureLibrary::UnloadTexture(const std::string& name) {
    m_textures.erase(name);
}

void TextureLibrary::UnloadAll() {
    m_textures.clear();
    m_atlases.clear();
}

TextureAtlas* TextureLibrary::CreateAtlas(const std::string& name, int width, int height) {
    auto atlas = std::make_unique<TextureAtlas>(width, height);
    TextureAtlas* ptr = atlas.get();
    m_atlases[name] = std::move(atlas);
    return ptr;
}

TextureAtlas* TextureLibrary::GetAtlas(const std::string& name) {
    auto it = m_atlases.find(name);
    return it != m_atlases.end() ? it->second.get() : nullptr;
}

void TextureLibrary::DestroyAtlas(const std::string& name) {
    m_atlases.erase(name);
}

std::vector<std::string> TextureLibrary::GetLoadedTextureNames() const {
    std::vector<std::string> names;
    names.reserve(m_textures.size());
    for (const auto& pair : m_textures) {
        names.push_back(pair.first);
    }
    return names;
}

size_t TextureLibrary::GetMemoryUsage() const {
    size_t total = 0;
    for (const auto& pair : m_textures) {
        total += CalculateTextureSize(*pair.second);
    }
    return total;
}

void TextureLibrary::CreateBuiltInTextures() {
    // Create some basic built-in textures
    CreateProceduralTexture("white", 64, 64, "solid", {RGB(255, 255, 255)});
    CreateProceduralTexture("black", 64, 64, "solid", {RGB(0, 0, 0)});
    CreateProceduralTexture("red", 64, 64, "solid", {RGB(255, 0, 0)});
    CreateProceduralTexture("green", 64, 64, "solid", {RGB(0, 255, 0)});
    CreateProceduralTexture("blue", 64, 64, "solid", {RGB(0, 0, 255)});
    CreateProceduralTexture("checkerboard", 128, 128, "checkerboard", {RGB(255, 255, 255), RGB(0, 0, 0), RGB(16, 0, 0)});
    CreateProceduralTexture("noise", 128, 128, "noise", {});
}

void TextureLibrary::CheckCacheLimit() {
    if (GetMemoryUsage() > m_maxCacheSize) {
        FlushCache();
    }
}

void TextureLibrary::FlushCache() {
    // Simple implementation - just clear all non-essential textures
    // In a real implementation, you'd use LRU or similar
    auto it = m_textures.begin();
    while (it != m_textures.end() && GetMemoryUsage() > m_maxCacheSize / 2) {
        if (it->first.find("builtin_") != 0) { // Don't remove built-in textures
            it = m_textures.erase(it);
        } else {
            ++it;
        }
    }
}

size_t TextureLibrary::CalculateTextureSize(const Texture& texture) const {
    return texture.GetWidth() * texture.GetHeight() * texture.GetChannels();
}

//=============================================================================
// TextureRenderer Implementation
//=============================================================================

void TextureRenderer::DrawTexture(HDC hdc, const Texture& texture, int x, int y) {
    DrawTexture(hdc, texture, x, y, texture.GetWidth(), texture.GetHeight());
}

void TextureRenderer::DrawTexture(HDC hdc, const Texture& texture, int x, int y, int width, int height) {
    int srcW = texture.GetWidth();
    int srcH = texture.GetHeight();
    if (srcW <= 0 || srcH <= 0) return;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = srcW;
    bmi.bmiHeader.biHeight = -srcH; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    int srcStride = srcW * 3;
    int dstStride = ((srcStride + 3) / 4) * 4;
    std::vector<BYTE> dib(dstStride * srcH);

    for (int yy = 0; yy < srcH; ++yy) {
        BYTE* line = dib.data() + yy * dstStride;
        for (int xx = 0; xx < srcW; ++xx) {
            COLORREF c = texture.GetPixel(xx, yy);
            line[xx * 3 + 0] = GetBValue(c);
            line[xx * 3 + 1] = GetGValue(c);
            line[xx * 3 + 2] = GetRValue(c);
        }
    }

    // Use StretchDIBits to draw and scale
    StretchDIBits(hdc, x, y, width, height,
                  0, 0, srcW, srcH,
                  dib.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
}

void TextureRenderer::DrawTextureEx(HDC hdc, const Texture& texture, const RECT& dest, const RECT& src) {
    int srcW = texture.GetWidth();
    int srcH = texture.GetHeight();
    if (srcW <= 0 || srcH <= 0) return;

    int copyW = src.right - src.left;
    int copyH = src.bottom - src.top;
    if (copyW <= 0 || copyH <= 0) return;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = copyW;
    bmi.bmiHeader.biHeight = -copyH; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    int srcStride = copyW * 3;
    int dstStride = ((srcStride + 3) / 4) * 4;
    std::vector<BYTE> dib(dstStride * copyH);

    for (int yy = 0; yy < copyH; ++yy) {
        BYTE* line = dib.data() + yy * dstStride;
        for (int xx = 0; xx < copyW; ++xx) {
            int sx = src.left + xx;
            int sy = src.top + yy;
            COLORREF c = texture.GetPixel(sx, sy);
            line[xx * 3 + 0] = GetBValue(c);
            line[xx * 3 + 1] = GetGValue(c);
            line[xx * 3 + 2] = GetRValue(c);
        }
    }

    StretchDIBits(hdc, dest.left, dest.top, dest.right - dest.left, dest.bottom - dest.top,
                  0, 0, copyW, copyH,
                  dib.data(), &bmi, DIB_RGB_COLORS, SRCCOPY);
}

void TextureRenderer::DrawTiledTexture(HDC hdc, const Texture& texture, const RECT& area, 
                                     int tileWidth, int tileHeight) {
    int texWidth = texture.GetWidth();
    int texHeight = texture.GetHeight();
    if (texWidth <= 0 || texHeight <= 0) return;

    for (int y = area.top; y < area.bottom; y += tileHeight) {
        for (int x = area.left; x < area.right; x += tileWidth) {
            int drawWidth = (std::min)(tileWidth, (int)(area.right - x));
            int drawHeight = (std::min)(tileHeight, (int)(area.bottom - y));

            // Draw portion of the texture scaled to the tile size
            DrawTexture(hdc, texture, x, y, drawWidth, drawHeight);
        }
    }
}

COLORREF TextureRenderer::BlendPixels(COLORREF src, COLORREF dest, BlendMode mode, float alpha) {
    BYTE sr = GetRValue(src), sg = GetGValue(src), sb = GetBValue(src);
    BYTE dr = GetRValue(dest), dg = GetGValue(dest), db = GetBValue(dest);
    
    BYTE r, g, b;
    
    switch (mode) {
        case Normal:
            r = (BYTE)(sr * alpha + dr * (1.0f - alpha));
            g = (BYTE)(sg * alpha + dg * (1.0f - alpha));
            b = (BYTE)(sb * alpha + db * (1.0f - alpha));
            break;
            
        case Additive:
            r = (BYTE)(std::min)(255, (int)(sr + dr));
            g = (BYTE)(std::min)(255, (int)(sg + dg));
            b = (BYTE)(std::min)(255, (int)(sb + db));
            break;
            
        case Multiply:
            r = (BYTE)((sr * dr) / 255);
            g = (BYTE)((sg * dg) / 255);
            b = (BYTE)((sb * db) / 255);
            break;
            
        case Screen:
            r = (BYTE)(255 - ((255 - sr) * (255 - dr)) / 255);
            g = (BYTE)(255 - ((255 - sg) * (255 - dg)) / 255);
            b = (BYTE)(255 - ((255 - sb) * (255 - db)) / 255);
            break;
            
        default:
            r = sr; g = sg; b = sb;
            break;
    }
    
    return RGB(r, g, b);
}