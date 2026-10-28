#!/usr/bin/env python3
"""Keep SDK panic output in PocketSprite's existing RTC log (Wi-Fi readable)."""
from pathlib import Path
root=Path(__file__).resolve().parent.parent
path=root/'upstream/8bkc-sdk/esp-idf/components/esp32/panic.c'
source=path.read_text()
marker='/* Cave Story: preserve panic output in the PocketSprite RTC log. */'
if marker not in source:
    needle='static void panicPutChar(char c)\n{\n'
    if source.count(needle)!=1:
        raise SystemExit('Unexpected SDK panic handler; refusing to patch')
    replacement='''/* Cave Story: preserve panic output in the PocketSprite RTC log. */
/* This file is linked in IRAM/DRAM. Do not call VFS, allocate, take a lock,
 * or touch flash here: panic may have interrupted any of those operations.
 * The normal panic handler stalls the other CPU before printing. */
static void panicRtcChar(char c)
{
    volatile unsigned *header = (volatile unsigned *)0x50000000;
    volatile unsigned char *buffer = (volatile unsigned char *)0x50000010;
    if (header[0] != 0x0dead109 || header[1] != 8176) return;
    unsigned pos = header[2];
    if (pos > 8176) return;
    if (pos == 8176) {pos = 0; header[3] = 1;}
    buffer[pos] = (unsigned char)c;
    header[2] = pos + 1;
}

static void panicPutChar(char c)
{
    panicRtcChar(c);
'''
    path.write_text(source.replace(needle,replacement))
    print('SDK panic output now also goes to the Wi-Fi-readable RTC log.')
