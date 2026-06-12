/**
 * @file OtaWebService.h
 * @brief HTTP server for OTA firmware update + SD card file management over WiFi.
 *
 * Endpoints (port 80):
 *   GET  /                       — web UI (firmware upload form + SD file manager)
 *   POST /update                 — multipart firmware .bin -> Update flash, then reboot
 *   GET  /sd/list?dir=/path      — JSON listing of an SD directory
 *   POST /sd/upload?dir=/path    — multipart file -> written to SD under dir
 *   GET  /sd/download?path=/f    — stream a file from SD
 *   POST /sd/delete?path=/f      — delete a file (or empty directory)
 *   POST /sd/mkdir?path=/d       — create a directory
 *
 * Started only after WiFi connects; loop() must be pumped from the main loop.
 */
#ifndef NET_OTA_WEB_SERVICE_H
#define NET_OTA_WEB_SERVICE_H

#include <FS.h>
#include <WebServer.h>

namespace net {

class OtaWebService {
public:
    /** Start the HTTP server (call once, after WiFi is connected). */
    void begin();

    /** Service pending HTTP clients (call from the main loop). */
    void loop();

    bool started() const { return started_; }

private:
    void handleRoot();
    void handleUpdatePost();
    void handleUpdateUpload();
    void handleSdList();
    void handleSdUpload();
    void handleSdDownload();
    void handleSdDelete();
    void handleSdMkdir();

    bool ensureSd();

    WebServer server_{80};
    fs::File  uploadFile_;        /* SD file currently being written */
    bool      started_     = false;
    bool      otaOk_       = false;
    bool      sdUploadOk_  = false;
    bool      rebootAt_    = false;
    uint32_t  rebootMs_    = 0;
};

}  // namespace net

#endif /* NET_OTA_WEB_SERVICE_H */
