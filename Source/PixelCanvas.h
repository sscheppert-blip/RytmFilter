#pragma once

#include <array>
#include <cstdlib>
#include <cstring>
#include <string>

// 128x64 1-Bit-Framebuffer mit eigener 5x7-Pixelschrift.
class PixelCanvas
{
public:
    static constexpr int W = 128, H = 64;

    void clear() { px.fill (0); }

    bool get (int x, int y) const { return x >= 0 && y >= 0 && x < W && y < H && px[(size_t) (y * W + x)] != 0; }

    void set (int x, int y, bool on = true)
    {
        if (x >= 0 && y >= 0 && x < W && y < H)
            px[(size_t) (y * W + x)] = on ? 1 : 0;
    }

    void invert (int x, int y)
    {
        if (x >= 0 && y >= 0 && x < W && y < H)
            px[(size_t) (y * W + x)] ^= 1;
    }

    void hline (int x0, int x1, int y, bool on = true) { for (int x = x0; x <= x1; ++x) set (x, y, on); }
    void vline (int x, int y0, int y1, bool on = true) { for (int y = y0; y <= y1; ++y) set (x, y, on); }

    void dottedV (int x, int y0, int y1) { for (int y = y0; y <= y1; y += 2) set (x, y); }
    void dottedH (int x0, int x1, int y)  { for (int x = x0; x <= x1; x += 2) set (x, y); }

    void rect (int x, int y, int w, int h)
    {
        hline (x, x + w - 1, y); hline (x, x + w - 1, y + h - 1);
        vline (x, y, y + h - 1); vline (x + w - 1, y, y + h - 1);
    }

    void fillRect (int x, int y, int w, int h, bool on = true)
    {
        for (int yy = y; yy < y + h; ++yy)
            hline (x, x + w - 1, yy, on);
    }

    void invertRect (int x, int y, int w, int h)
    {
        for (int yy = y; yy < y + h; ++yy)
            for (int xx = x; xx < x + w; ++xx)
                invert (xx, yy);
    }

    void line (int x0, int y0, int x1, int y1)
    {
        const int dx = std::abs (x1 - x0), sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs (y1 - y0), sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        for (;;)
        {
            set (x0, y0);
            if (x0 == x1 && y0 == y1) break;
            const int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    void circle (int cx, int cy, int r)
    {
        int x = r, y = 0, err = 1 - r;
        while (x >= y)
        {
            set (cx + x, cy + y); set (cx + y, cy + x); set (cx - y, cy + x); set (cx - x, cy + y);
            set (cx - x, cy - y); set (cx - y, cy - x); set (cx + y, cy - x); set (cx + x, cy - y);
            ++y;
            if (err < 0) err += 2 * y + 1;
            else { --x; err += 2 * (y - x) + 1; }
        }
    }

    // --- Text ---
    static int glyphAdvance (char c)
    {
        if (c == '.' || c == ':') return 2;
        if (c == ' ') return 4;
        return 6;
    }

    static int textWidth (const std::string& s)
    {
        int w = 0;
        for (char c : s) w += glyphAdvance (c);
        return w > 0 ? w - 1 : 0;
    }

    // Zeichnet Text; on=false loescht (fuer invertierte Flaechen)
    void text (int x, int y, const std::string& s, bool on = true)
    {
        for (char c : s)
        {
            const auto* g = glyph (c);
            if (c == '.' || c == ':')
            {
                for (int row = 0; row < 7; ++row)
                    if (g[row] & 0x04) set (x, y + row, on);
            }
            else
            {
                for (int row = 0; row < 7; ++row)
                    for (int col = 0; col < 5; ++col)
                        if (g[row] & (0x10 >> col))
                            set (x + col, y + row, on);
            }
            x += glyphAdvance (c);
        }
    }

    void textCentred (int cx, int y, const std::string& s, bool on = true) { text (cx - textWidth (s) / 2, y, s, on); }
    void textRight (int rx, int y, const std::string& s, bool on = true)   { text (rx - textWidth (s) + 1, y, s, on); }

private:
    // Zeilenweise, 5 Bit (MSB = links)
    static const unsigned char* glyph (char c)
    {
        static const unsigned char blank[7] = { 0 };
        static const struct { char ch; unsigned char rows[7]; } font[] = {
            { 'A', { 0x0E,0x11,0x11,0x1F,0x11,0x11,0x11 } }, { 'B', { 0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E } },
            { 'C', { 0x0E,0x11,0x10,0x10,0x10,0x11,0x0E } }, { 'D', { 0x1C,0x12,0x11,0x11,0x11,0x12,0x1C } },
            { 'E', { 0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F } }, { 'F', { 0x1F,0x10,0x10,0x1E,0x10,0x10,0x10 } },
            { 'G', { 0x0E,0x11,0x10,0x17,0x11,0x11,0x0F } }, { 'H', { 0x11,0x11,0x11,0x1F,0x11,0x11,0x11 } },
            { 'I', { 0x0E,0x04,0x04,0x04,0x04,0x04,0x0E } }, { 'J', { 0x07,0x02,0x02,0x02,0x02,0x12,0x0C } },
            { 'K', { 0x11,0x12,0x14,0x18,0x14,0x12,0x11 } }, { 'L', { 0x10,0x10,0x10,0x10,0x10,0x10,0x1F } },
            { 'M', { 0x11,0x1B,0x15,0x15,0x11,0x11,0x11 } }, { 'N', { 0x11,0x11,0x19,0x15,0x13,0x11,0x11 } },
            { 'O', { 0x0E,0x11,0x11,0x11,0x11,0x11,0x0E } }, { 'P', { 0x1E,0x11,0x11,0x1E,0x10,0x10,0x10 } },
            { 'Q', { 0x0E,0x11,0x11,0x11,0x15,0x12,0x0D } }, { 'R', { 0x1E,0x11,0x11,0x1E,0x14,0x12,0x11 } },
            { 'S', { 0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E } }, { 'T', { 0x1F,0x04,0x04,0x04,0x04,0x04,0x04 } },
            { 'U', { 0x11,0x11,0x11,0x11,0x11,0x11,0x0E } }, { 'V', { 0x11,0x11,0x11,0x11,0x11,0x0A,0x04 } },
            { 'W', { 0x11,0x11,0x11,0x15,0x15,0x15,0x0A } }, { 'X', { 0x11,0x11,0x0A,0x04,0x0A,0x11,0x11 } },
            { 'Y', { 0x11,0x11,0x0A,0x04,0x04,0x04,0x04 } }, { 'Z', { 0x1F,0x01,0x02,0x04,0x08,0x10,0x1F } },
            { '0', { 0x0E,0x11,0x13,0x15,0x19,0x11,0x0E } }, { '1', { 0x04,0x0C,0x04,0x04,0x04,0x04,0x0E } },
            { '2', { 0x0E,0x11,0x01,0x02,0x04,0x08,0x1F } }, { '3', { 0x1F,0x02,0x04,0x02,0x01,0x11,0x0E } },
            { '4', { 0x02,0x06,0x0A,0x12,0x1F,0x02,0x02 } }, { '5', { 0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E } },
            { '6', { 0x06,0x08,0x10,0x1E,0x11,0x11,0x0E } }, { '7', { 0x1F,0x01,0x02,0x04,0x08,0x08,0x08 } },
            { '8', { 0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E } }, { '9', { 0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C } },
            { '.', { 0x00,0x00,0x00,0x00,0x00,0x00,0x04 } }, { ':', { 0x00,0x04,0x04,0x00,0x04,0x04,0x00 } },
            { '-', { 0x00,0x00,0x00,0x0E,0x00,0x00,0x00 } }, { '+', { 0x00,0x04,0x04,0x1F,0x04,0x04,0x00 } },
            { '/', { 0x01,0x01,0x02,0x04,0x08,0x10,0x10 } }, { '%', { 0x19,0x19,0x02,0x04,0x08,0x13,0x13 } },
            { '=', { 0x00,0x00,0x1F,0x00,0x1F,0x00,0x00 } }, { '>', { 0x08,0x04,0x02,0x01,0x02,0x04,0x08 } },
            { '<', { 0x02,0x04,0x08,0x10,0x08,0x04,0x02 } }, { '_', { 0x00,0x00,0x00,0x00,0x00,0x00,0x1F } },
        };

        if (c >= 'a' && c <= 'z')
            c = (char) (c - 'a' + 'A');
        for (const auto& f : font)
            if (f.ch == c)
                return f.rows;
        return blank;
    }

    std::array<unsigned char, (size_t) (W * H)> px {};
};
