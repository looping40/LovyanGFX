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
#include "Panel_SSD1322.hpp"
#include "../Bus.hpp"
#include "../platforms/common.hpp"
#include "../misc/pixelcopy.hpp"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace lgfx
{
 inline namespace v1
 {
//----------------------------------------------------------------------------

  static constexpr int8_t Bayer[16] = {-30, 2, -22, 10, 18, -14, 26, -6, -18, 14, -26, 6, 30, -2, 22, -10};

  color_depth_t Panel_SSD1322::setColorDepth(color_depth_t depth)
  {
    _write_depth = color_depth_t::rgb888_3Byte;
    _read_depth = color_depth_t::rgb888_3Byte;
    return color_depth_t::rgb888_3Byte;
  }

  void Panel_SSD1322::waitDisplay(void)
  {
    _bus->wait();
  }

  bool Panel_SSD1322::displayBusy(void)
  {
    return _bus->busy();
  }

  size_t Panel_SSD1322::_get_buffer_length(void) const
  {
    return ((_cfg.panel_width + 1) >> 1) * _cfg.panel_height;
  }

  bool Panel_SSD1322::init(bool use_reset)
  {
    if (!Panel_HasBuffer::init(use_reset))
    {
      return false;
    }

    startWrite(true);

    for (size_t i = 0; auto cmds = getInitCommands(i); i++)
    {
      size_t idx = 0;
      while (cmds[idx] != 0xFF || cmds[idx + 1] != 0xFF) ++idx;
      if (idx) { _bus->writeBytes(cmds, idx, false, true); }
    }

    setInvert(_invert);
    setRotation(_rotation);
    endWrite();

    return true;
  }

  void Panel_SSD1322::setInvert(bool invert)
  {
    _invert = invert;
    startWrite();
    _bus->writeCommand((invert ^ _cfg.invert) ? CMD_INVON : CMD_INVOFF , 8);
    endWrite();
  }

  void Panel_SSD1322::setSleep(bool flg)
  {
    startWrite();
    _bus->writeCommand(flg ? CMD_SLPIN : CMD_SLPOUT, 8);
    endWrite();
  }

  void Panel_SSD1322::writeFillRectPreclipped(uint_fast16_t x, uint_fast16_t y, uint_fast16_t w, uint_fast16_t h, uint32_t rawcolor)
  {
    uint_fast16_t xs = x, xe = x + w - 1;
    uint_fast16_t ys = y, ye = y + h - 1;
    _xs = xs;
    _ys = ys;
    _xe = xe;
    _ye = ye;
    _update_transferred_rect(xs, ys, xe, ye);

    bgr888_t color { rawcolor };
    int32_t sum = (color.R8() + (color.G8() << 1) + color.B8());

    y = ys;
    do
    {
      x = xs;
      auto btbl = &Bayer[(y & 3) << 2];
      auto buf = &_buf[y * ((_cfg.panel_width + 1) >> 1)];
      do
      {
        size_t idx = x >> 1;
        uint_fast8_t shift = (x & 1) ? 0 : 4;
        uint_fast8_t value = (std::min<int32_t>(15, std::max<int32_t>(0, sum + btbl[x & 3]) >> 6) & 0x0F) << shift;
        buf[idx] = (buf[idx] & (0xF0 >> shift)) | value;
      } while (++x <= xe);
    } while (++y <= ye);
  }

  void Panel_SSD1322::writeImage(uint_fast16_t x, uint_fast16_t y, uint_fast16_t w, uint_fast16_t h, pixelcopy_t* param, bool use_dma)
  {
    uint_fast16_t xs = x, xe = x + w - 1;
    uint_fast16_t ys = y, ye = y + h - 1;
    _update_transferred_rect(xs, ys, xe, ye);

    auto readbuf = (bgr888_t*)alloca(w * sizeof(bgr888_t));
    auto sx = param->src_x32;
    h += y;
    do
    {
      uint32_t prev_pos = 0, new_pos = 0;
      do
      {
        new_pos = param->fp_copy(readbuf, prev_pos, w, param);
        if (new_pos != prev_pos)
        {
          do
          {
            auto color = readbuf[prev_pos];
            _draw_pixel(x + prev_pos, y, (color.R8() + (color.G8() << 1) + color.B8()));
          } while (new_pos != ++prev_pos);
        }
      } while (w != new_pos && w != (prev_pos = param->fp_skip(new_pos, w, param)));
      param->src_x32 = sx;
      param->src_y++;
    } while (++y < h);
  }

  void Panel_SSD1322::writePixels(pixelcopy_t* param, uint32_t length, bool use_dma)
  {
    {
      uint_fast16_t xs = _xs;
      uint_fast16_t xe = _xe;
      uint_fast16_t ys = _ys;
      uint_fast16_t ye = _ye;
      _update_transferred_rect(xs, ys, xe, ye);
    }
    uint_fast16_t xs   = _xs  ;
    uint_fast16_t ys   = _ys  ;
    uint_fast16_t xe   = _xe  ;
    uint_fast16_t ye   = _ye  ;
    uint_fast16_t xpos = _xpos;
    uint_fast16_t ypos = _ypos;

    static constexpr uint32_t buflen = 16;
    bgr888_t colors[buflen];
    int bufpos = buflen;
    do
    {
      if (bufpos == buflen) {
        param->fp_copy(colors, 0, std::min(length, buflen), param);
        bufpos = 0;
      }
      auto color = colors[bufpos++];
      _draw_pixel(xpos, ypos, (color.R8() + (color.G8() << 1) + color.B8()));
      if (++xpos > xe)
      {
        xpos = xs;
        if (++ypos > ye)
        {
          ypos = ys;
        }
      }
    } while (--length);
    _xpos = xpos;
    _ypos = ypos;
  }

  void Panel_SSD1322::readRect(uint_fast16_t x, uint_fast16_t y, uint_fast16_t w, uint_fast16_t h, void* dst, pixelcopy_t* param)
  {
    auto readbuf = (bgr888_t*)alloca(w * sizeof(bgr888_t));
    param->src_data = readbuf;
    int32_t readpos = 0;
    h += y;
    do
    {
      uint32_t idx = 0;
      do
      {
        readbuf[idx] = 0x010101u * (_read_pixel(x + idx, y) * 16 + 8);
      } while (++idx != w);
      param->src_x32 = 0;
      readpos = param->fp_copy(dst, readpos, readpos + w, param);
    } while (++y < h);
  }

  void Panel_SSD1322::_draw_pixel(uint_fast16_t x, uint_fast16_t y, uint32_t sum)
  {
    _rotate_pos(x, y);

    auto btbl = &Bayer[(y & 3) << 2];
    size_t idx = (x >> 1) + (y * ((_cfg.panel_width + 1) >> 1));
    uint_fast8_t shift = (x & 1) ? 0 : 4;
    uint_fast8_t value = (std::min<int32_t>(15, std::max<int32_t>(0, sum + btbl[x & 3]) >> 6) & 0x0F) << shift;
    _buf[idx] = (_buf[idx] & (0xF0 >> shift)) | value;
  }

  uint8_t Panel_SSD1322::_read_pixel(uint_fast16_t x, uint_fast16_t y)
  {
    _rotate_pos(x, y);
    size_t idx = (x >> 1) + (y * ((_cfg.panel_width + 1) >> 1));
    return (x & 1)
         ? (_buf[idx] & 0x0F)
         : (_buf[idx] >> 4)
         ;
  }

  void Panel_SSD1322::_update_transferred_rect(uint_fast16_t &xs, uint_fast16_t &ys, uint_fast16_t &xe, uint_fast16_t &ye)
  {
    _rotate_pos(xs, ys, xe, ye);
    _range_mod.left   = std::min<int32_t>(xs, _range_mod.left);
    _range_mod.right  = std::max<int32_t>(xe, _range_mod.right);
    _range_mod.top    = std::min<int32_t>(ys, _range_mod.top);
    _range_mod.bottom = std::max<int32_t>(ye, _range_mod.bottom);
  }

  void Panel_SSD1322::setBrightness(uint8_t brightness)
  {
    startWrite();
    _bus->writeCommand(0xC1 | brightness << 8, 16);
    endWrite();
  }

  void Panel_SSD1322::setPowerSave(bool flg)
  {
    startWrite();
    _bus->writeCommand(flg ? CMD_SLPIN : CMD_SLPOUT, 8);
    endWrite();
  }

  void Panel_SSD1322::display(uint_fast16_t x, uint_fast16_t y, uint_fast16_t w, uint_fast16_t h)
  {
    if (0 < w && 0 < h)
    {
      _range_mod.left   = std::min<int16_t>(_range_mod.left  , x        );
      _range_mod.right  = std::max<int16_t>(_range_mod.right , x + w - 1);
      _range_mod.top    = std::min<int16_t>(_range_mod.top   , y        );
      _range_mod.bottom = std::max<int16_t>(_range_mod.bottom, y + h - 1);
    }

    if (_range_mod.empty()) { return; }

    uint_fast8_t xs = _range_mod.left  >> 1;
    uint_fast8_t xe = _range_mod.right >> 1;
    uint_fast8_t col_ofs = _cfg.offset_x >> 1;

    uint_fast8_t nbytes = xe - xs + 1;

    for (uint_fast16_t row = _range_mod.top; row <= _range_mod.bottom; ++row)
    {
      uint_fast8_t yaddr = static_cast<uint_fast8_t>(row + _cfg.offset_y);
      _bus->writeCommand(CMD_CASET | (uint32_t)(xs + col_ofs) << 8 | (uint32_t)(xe + col_ofs) << 16, 24);
      _bus->writeCommand(CMD_RASET | (uint32_t)yaddr << 8 | (uint32_t)yaddr << 16, 24);
      _bus->writeCommand(CMD_WRITERAM, 8);
      auto buf = &_buf[xs + row * ((_cfg.panel_width + 1) >> 1 )];
      _bus->writeBytes(buf, nbytes, true, true);
    }

    _range_mod.top    = INT16_MAX;
    _range_mod.left   = INT16_MAX;
    _range_mod.right  = 0;
    _range_mod.bottom = 0;
  }

//----------------------------------------------------------------------------
 }
}
