/*----------------------------------------------------------------------------/
  Lovyan GFX - Graphics library for embedded devices.

Original Source:
 https://github.com/lovyan03/LovyanGFX/

Licence:
 [FreeBSD](https://github.com/lovyan03/LovyanGFX/blob/master/license.txt)

Author:
 [lovyan03](https://twitter.com/lovyan03)

Contributors:
 [ciniml](https://github.com/ciniml)
 [mongonta0716](https://github.com/mongonta0716)
 [tobozo](https://github.com/tobozo)
/----------------------------------------------------------------------------*/
#pragma once

#include "Panel_HasBuffer.hpp"
#include "../misc/range.hpp"

namespace lgfx
{
 inline namespace v1
 {
//----------------------------------------------------------------------------

  /// SSD1322 4bpp grayscale OLED (e.g. NHD 256x64). 2 pixels per byte, 4 bits per pixel.
  struct Panel_SSD1322 : public Panel_HasBuffer
  {
  public:
    Panel_SSD1322(void) : Panel_HasBuffer()
    {
      _cfg.memory_width  = _cfg.panel_width  = 256;
      _cfg.memory_height = _cfg.panel_height = 64;
      // NHD 256x64: RAM column offset (see U8g2 u8x8_ssd1322_256x64_display_info default_x_offset 0x1c)
      _cfg.offset_x = 112;
    }

    bool init(bool use_reset) override;

    void waitDisplay(void) override;
    bool displayBusy(void) override;
    color_depth_t setColorDepth(color_depth_t depth) override;

    void setBrightness(uint8_t brightness) override;
    void setInvert(bool invert) override;
    void setSleep(bool flg) override;
    void setPowerSave(bool flg) override;

    void display(uint_fast16_t x, uint_fast16_t y, uint_fast16_t w, uint_fast16_t h) override;

    void writeFillRectPreclipped(uint_fast16_t x, uint_fast16_t y, uint_fast16_t w, uint_fast16_t h, uint32_t rawcolor) override;
    void writeImage(uint_fast16_t x, uint_fast16_t y, uint_fast16_t w, uint_fast16_t h, pixelcopy_t* param, bool use_dma) override;
    void writePixels(pixelcopy_t* param, uint32_t len, bool use_dma) override;

    uint32_t readCommand(uint_fast16_t, uint_fast8_t, uint_fast8_t) override { return 0; }
    uint32_t readData(uint_fast8_t, uint_fast8_t) override { return 0; }

    void readRect(uint_fast16_t x, uint_fast16_t y, uint_fast16_t w, uint_fast16_t h, void* dst, pixelcopy_t* param) override;

  protected:

    static constexpr uint8_t CMD_NOP       = 0xE3;
    static constexpr uint8_t CMD_SLPIN       = 0xAE;
    static constexpr uint8_t CMD_SLPOUT      = 0xAF;
    static constexpr uint8_t CMD_INVOFF      = 0xA6;
    static constexpr uint8_t CMD_INVON       = 0xA7;
    static constexpr uint8_t CMD_CASET       = 0x15;
    static constexpr uint8_t CMD_RASET       = 0x75;
    static constexpr uint8_t CMD_WRITERAM    = 0x5C;
    static constexpr uint8_t CMD_CMDLOCK     = 0xFD;

    const uint8_t* getInitCommands(uint8_t listno) const override
    {
      // NHD 256x64 style init (from U8g2 u8x8_d_ssd1322_256x64_init_seq) + display on
      static constexpr uint8_t list0[] =
      {
        CMD_CMDLOCK , 1, 0x12,   // Commande 0xFD, 1 argument : 0x12
        CMD_SLPIN   , 0,         // Commande 0xAE, 0 argument
        0xB3        , 1, 0x91,   // Commande 0xB3, 1 argument : 0x91
        0xCA        , 1, 0x3F,   // Commande 0xCA, 1 argument : 0x3F
        0xA2        , 1, 0x00,   // Commande 0xA2, 1 argument : 0x00
        0xA1        , 1, 0x00,   // Commande 0xA1, 1 argument : 0x00
        0xA0        , 2, 0x06, 0x11, // Commande 0xA0, 2 arguments : 0x06, 0x01
        0xAB        , 1, 0x01,   // Commande 0xAB, 1 argument : 0x01
        0xB4        , 2, 0xA0, 0xFD, // Commande 0xB4, 2 arguments : 0xA0, 0xFD
        0xC1        , 1, 0x9F,   // Commande 0xC1, 1 argument : 0x9F
        0xC7        , 1, 0x0F,   // Commande 0xC7, 1 argument : 0x0F
        0xB9        , 0,          // Commande 0xB9, 0 argument
        0xB1        , 1, 0xE2,   // Commande 0xB1, 1 argument : 0xE2
        0xD1        , 2, 0xA2, 0x20, // Commande 0xD1, 2 arguments : 0xA2, 0x20
        0xBB        , 1, 0x1F,   // Commande 0xBB, 1 argument : 0x1F
        0xB6        , 1, 0x08,   // Commande 0xB6, 1 argument : 0x08
        0xBE        , 1, 0x07,   // Commande 0xBE, 1 argument : 0x07
        CMD_INVOFF  , 0,         // Commande 0xA6, 0 argument
        0xA9        , 0,         // Commande 0xA9, 0 argument
        CMD_SLPOUT  , 0,         // Commande 0xAF, 0 argument
        0xFF        , 0xFF,      // Marqueur de fin obligatoire pour command_list
      };
      switch (listno)
      {
      case 0: return list0;
      default: return nullptr;
      }
    }

    size_t _get_buffer_length(void) const override;
    uint8_t _read_pixel(uint_fast16_t x, uint_fast16_t y);
    void _draw_pixel(uint_fast16_t x, uint_fast16_t y, uint32_t value);
    void _update_transferred_rect(uint_fast16_t &xs, uint_fast16_t &ys, uint_fast16_t &xe, uint_fast16_t &ye);

  };

//----------------------------------------------------------------------------
 }
}
