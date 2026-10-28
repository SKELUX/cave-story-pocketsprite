#!/usr/bin/env python3
"""Fix unaligned AppFS mappings that cross a 64 KiB flash page boundary."""
from pathlib import Path
root=Path(__file__).resolve().parent.parent
path=root/'upstream/8bkc-sdk/8bkc-components/appfs/appfs.c'
s=path.read_text()
old='int *pages=alloca(sizeof(int)*((len/APPFS_SECTOR_SZ)+1));'
new='''/* Include the first-page offset: a short read can span two pages. */
\tsize_t span=offset+len;
\tint *pages=alloca(sizeof(int)*((span+APPFS_SECTOR_SZ-1)/APPFS_SECTOR_SZ));'''
if old in s:
    start=s.index('esp_err_t appfsMmap(');end=s.index('\nvoid appfsMunmap',start)
    function=s[start:end]
    assert function.count(old)==1 and function.count('while(len>mappedlen)')==1
    function=function.replace(old,new).replace('while(len>mappedlen)','while(span>mappedlen)')
    path.write_text(s[:start]+function+s[end:])
    print('Fixed SDK AppFS mapping span for reads across flash pages.')
elif new not in s:
    raise SystemExit('Unexpected SDK AppFS mapper; refusing to patch')
