/* PocketSprite storage backend. Never deletes or renames existing files. */
#include "save_io.h"
#include "appfs.h"
#include <stdio.h>
static appfs_handle_t handle=APPFS_INVALID_FD;
int cs_save_io_init(void) {
    handle=appfsOpen("cavestory.state");
    if(handle==APPFS_INVALID_FD) {
        if(appfsExists("cavestory.state")) return 0;
        if(appfsCreateFile("cavestory.state",CS_SAVE_FILE_BYTES,&handle)!=ESP_OK) return 0;
        if(appfsErase(handle,0,CS_SAVE_FILE_BYTES)!=ESP_OK) return 0;
    }
    int size=0;
    appfsEntryInfo(handle,NULL,&size);
    return size==CS_SAVE_FILE_BYTES;
}
int cs_save_io_read(size_t off,void *data,size_t len) {
    return handle!=APPFS_INVALID_FD && appfsRead(handle,off,data,len)==ESP_OK;
}
int cs_save_io_write(size_t off,const void *data,size_t len) {
    return handle!=APPFS_INVALID_FD && appfsWrite(handle,off,(uint8_t*)data,len)==ESP_OK;
}
int cs_save_io_erase(size_t off,size_t len) {
    return handle!=APPFS_INVALID_FD && appfsErase(handle,off,len)==ESP_OK;
}
void cs_save_io_error(void) { printf("Cave Story: save storage failed; previous snapshot retained if valid.\n"); }
