#!/usr/bin/env python3
"""Extract the SDK's native 4x6 ASCII font without linking its full GUI toolkit."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parent.parent
source=(ROOT/'upstream/8bkc-sdk/8bkc-components/ugui/UGUI/ugui.c').read_text()
table=source.split('font_4x6[256][6]={',1)[1].split('};',1)[0]
rows=re.findall(r'\{([^}]+)\}',table)
assert len(rows)==256
header='''/* Generated from PocketSprite SDK µGUI font_4x6.
 * Copyright (C) 2015 Achim Doebler. See licenses/ugui.txt.
 * Reproduce with tools/generate_ui_font.py. */
#ifndef CS_FONT4X6_H
#define CS_FONT4X6_H
#include <stdint.h>
static const uint8_t cs_font4x6[96][6]={
'''
header+='\n'.join('{'+row.strip()+'},' for row in rows[32:128])
header+='\n};\n#endif\n'
(ROOT/'platform/font4x6.h').write_text(header)
