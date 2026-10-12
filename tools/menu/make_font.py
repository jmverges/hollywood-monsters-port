#!/usr/bin/env python3
"""Generate platform/win32/menu_font.h: the port menu's font (platform/win32/menu.c).

usage: make_font.py [DejaVuSans.ttf] [out.h]      (needs Pillow)

Latin-1 (32..255), proportional, 8-bit coverage per pixel, in two sizes: body text and titles. The glyphs are
rendered from DejaVu Sans, whose license allows this; its notice goes into the header.
"""
import sys
from PIL import Image, ImageDraw, ImageFont

SIZES = (('menu_font_body', 15), ('menu_font_title', 21))
NOTICE = """/* Glyphs rendered from DejaVu Sans by tools/menu/make_font.py. Do not edit. The DejaVu fonts' license:
 * Copyright: Copyright (c) 2003 by Bitstream, Inc. All Rights Reserved. 
 * Bitstream Vera is a trademark of Bitstream, Inc.
 * DejaVu changes are in public domain.
 * License: bitstream-vera
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of the fonts accompanying this license ("Fonts") and associated
 * documentation files (the "Font Software"), to reproduce and distribute the
 * Font Software, including without limitation the rights to use, copy, merge,
 * publish, distribute, and/or sell copies of the Font Software, and to permit
 * persons to whom the Font Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright and trademark notices and this permission notice shall
 * be included in all copies of one or more of the Font Software typefaces.
 *
 * The Font Software may be modified, altered, or added to, and in particular
 * the designs of glyphs or characters in the Fonts may be modified and
 * additional glyphs or characters may be added to the Fonts, only if the fonts
 * are renamed to names not containing either the words "Bitstream" or the word
 * "Vera".
 *
 * This License becomes null and void to the extent applicable to Fonts or Font
 * Software that has been modified and is distributed under the "Bitstream
 * Vera" names.
 *
 * The Font Software may be sold as part of a larger software package but no
 * copy of one or more of the Font Software typefaces may be sold by itself.
 *
 * THE FONT SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF COPYRIGHT, PATENT,
 * TRADEMARK, OR OTHER RIGHT. IN NO EVENT SHALL BITSTREAM OR THE GNOME
 * FOUNDATION BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, INCLUDING
 * ANY GENERAL, SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * THE USE OR INABILITY TO USE THE FONT SOFTWARE OR FROM OTHER DEALINGS IN THE
 * FONT SOFTWARE.
 *
 * Except as contained in this notice, the names of Gnome, the Gnome
 * Foundation, and Bitstream Inc., shall not be used in advertising or
 * otherwise to promote the sale, use or other dealings in this Font Software
 * without prior written authorization from the Gnome Foundation or Bitstream
 * Inc., respectively. For further information, contact: fonts at gnome dot
 * org.
 */
"""


def build(path, name, size):
    font = ImageFont.truetype(path, size)
    ascent, descent = font.getmetrics()
    height = ascent + descent
    widths, offsets, data = [], [], bytearray()
    for code in range(32, 256):
        ch = chr(code)
        adv = max(1, round(font.getlength(ch))) if code != 0xA0 else round(font.getlength(' '))
        if code < 0xA0 and code >= 0x7F:
            adv = 0                                  # control codes: nothing
        img = Image.new('L', (max(adv, 1), height), 0)
        if adv:
            ImageDraw.Draw(img).text((0, ascent), ch, font=font, fill=255, anchor='ls')
        widths.append(adv)
        offsets.append(len(data))
        data += img.tobytes() if adv else b''
    out = ['static const unsigned char %s_bits[%d] = {' % (name, len(data))]
    for i in range(0, len(data), 32):
        out.append('    ' + ','.join(str(b) for b in data[i:i + 32]) + ',')
    out.append('};')
    out.append('static const unsigned char %s_w[224] = { %s };' % (name, ','.join(map(str, widths))))
    out.append('static const unsigned %s_off[224] = { %s };' % (name, ','.join(map(str, offsets))))
    out.append('static const struct menu_font %s = { %d, %d, %s_w, %s_off, %s_bits };'
               % (name, height, ascent, name, name, name))
    return '\n'.join(out)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'
    dst = sys.argv[2] if len(sys.argv) > 2 else 'platform/win32/menu_font.h'
    parts = [NOTICE, '#pragma once',
             '/* Latin-1 32..255: width (advance) and offset of each glyph; glyphs are width x height coverage bytes */',
             'struct menu_font { int height, ascent; const unsigned char *w; const unsigned *off; const unsigned char *bits; };']
    for name, size in SIZES:
        parts.append(build(path, name, size))
    with open(dst, 'w') as f:
        f.write('\n'.join(parts) + '\n')
    print(dst)


if __name__ == '__main__':
    main()
