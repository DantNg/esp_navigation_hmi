#include "net/OtaWebService.h"

#include <Arduino.h>
#include <SD.h>
#include <Update.h>

#include "map/SdCardMount.h"

namespace net {

/* ------------------------------------------------------------------ */
/* Web UI (single page, no external assets)                            */
/* ------------------------------------------------------------------ */
static const char kIndexHtml[] PROGMEM = R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ground Station — OTA & SD</title>
<style>
 body{font-family:sans-serif;max-width:720px;margin:20px auto;padding:0 12px;background:#111;color:#eee}
 h2{border-bottom:1px solid #444;padding-bottom:4px}
 fieldset{border:1px solid #444;border-radius:6px;margin-bottom:18px}
 button,input[type=submit]{background:#2a6;border:0;color:#fff;padding:6px 14px;border-radius:4px;cursor:pointer}
 button.del{background:#a33}
 table{width:100%;border-collapse:collapse}
 td,th{padding:4px 6px;border-bottom:1px solid #333;text-align:left;font-size:14px}
 a{color:#6cf}
 #prog{width:100%;display:none}
 input[type=text]{background:#222;color:#eee;border:1px solid #444;padding:4px;border-radius:4px}
</style></head><body>
<h1>Lite Ground Station</h1>

<fieldset><legend><h2>Firmware OTA</h2></legend>
<form id="fwform">
  <input type="file" name="firmware" accept=".bin" required>
  <input type="submit" value="Flash firmware">
</form>
<progress id="prog" max="100" value="0"></progress>
<p id="fwmsg"></p>
</fieldset>

<fieldset><legend><h2>SD Card Files</h2></legend>
<p>Dir: <input type="text" id="dir" value="/" size="30">
   <button onclick="list()">Refresh</button>
   <button onclick="mkdir()">New folder</button></p>
<form id="upform">
  <input type="file" name="file" required>
  <input type="submit" value="Upload to dir">
</form>
<form id="dirform" style="margin-top:8px">
  <input type="file" id="dirpick" webkitdirectory multiple required>
  <input type="submit" value="Upload folder to dir">
</form>
<progress id="upprog" max="100" value="0" style="width:100%;display:none"></progress>
<p id="sdmsg"></p>
<table id="files"><tr><th>Name</th><th>Size</th><th></th></tr></table>
</fieldset>

<script>
const $=id=>document.getElementById(id);
function fmt(n){return n>1048576?(n/1048576).toFixed(1)+' MB':n>1024?(n/1024).toFixed(1)+' KB':n+' B'}
function list(){
 fetch('/sd/list?dir='+encodeURIComponent($('dir').value)).then(r=>r.json()).then(j=>{
  let h='<tr><th>Name</th><th>Size</th><th></th></tr>';
  for(const f of j.files){
   const p=(j.dir==='/'?'':j.dir)+'/'+f.name;
   h+='<tr><td>'+(f.dir?'&#128193; <a href="#" onclick="$(\'dir\').value=\''+p+'\';list();return false">'+f.name+'</a>'
                       :'<a href="/sd/download?path='+encodeURIComponent(p)+'">'+f.name+'</a>')+'</td>'+
      '<td>'+(f.dir?'':fmt(f.size))+'</td>'+
      '<td><button class="del" onclick="del(\''+p+'\')">delete</button></td></tr>';
  }
  $('files').innerHTML=h; $('sdmsg').textContent='';
 }).catch(e=>$('sdmsg').textContent='List failed: '+e);
}
function del(p){
 if(!confirm('Delete '+p+'?'))return;
 fetch('/sd/delete?path='+encodeURIComponent(p),{method:'POST'}).then(r=>r.text()).then(t=>{$('sdmsg').textContent=t;list();});
}
function mkdir(){
 const n=prompt('Folder name:'); if(!n)return;
 const d=$('dir').value;
 fetch('/sd/mkdir?path='+encodeURIComponent((d==='/'?'':d)+'/'+n),{method:'POST'}).then(r=>r.text()).then(t=>{$('sdmsg').textContent=t;list();});
}
function xhrUpload(url,form,prog,msg,done){
 const x=new XMLHttpRequest();
 x.open('POST',url);
 x.upload.onprogress=e=>{prog.style.display='block';prog.value=e.loaded/e.total*100};
 x.onload=()=>{msg.textContent=x.responseText;done&&done(x.status)};
 x.onerror=()=>msg.textContent='Upload failed';
 x.send(new FormData(form));
}
$('fwform').onsubmit=e=>{e.preventDefault();
 $('fwmsg').textContent='Uploading firmware… do not power off.';
 xhrUpload('/update',e.target,$('prog'),$('fwmsg'),s=>{if(s==200)$('fwmsg').textContent+=' Rebooting…'});
};
$('upform').onsubmit=e=>{e.preventDefault();
 xhrUpload('/sd/upload?dir='+encodeURIComponent($('dir').value),e.target,$('upprog'),$('sdmsg'),()=>list());
};
$('dirform').onsubmit=async e=>{e.preventDefault();
 const fs=$('dirpick').files; if(!fs.length)return;
 const d=$('dir').value, prog=$('upprog');
 prog.style.display='block'; prog.value=0;
 for(let i=0;i<fs.length;i++){
  const f=fs[i], rel=f.webkitRelativePath||f.name;
  $('sdmsg').textContent='Uploading '+(i+1)+'/'+fs.length+': '+rel;
  const fd=new FormData(); fd.append('file',f);
  let ok=false;
  for(let retry=0;retry<3&&!ok;retry++){
   try{const r=await fetch('/sd/upload?dir='+encodeURIComponent(d)+'&path='+encodeURIComponent(rel),{method:'POST',body:fd});ok=r.ok}
   catch(_){}
  }
  if(!ok){$('sdmsg').textContent='Failed at '+rel+' ('+i+'/'+fs.length+' uploaded)';list();return}
  prog.value=(i+1)/fs.length*100;
 }
 $('sdmsg').textContent='Folder upload done: '+fs.length+' files';
 list();
};
list();
</script></body></html>)HTML";

/* ------------------------------------------------------------------ */

static String normPath(String p) {
    if (p.isEmpty() || p[0] != '/') p = "/" + p;
    while (p.length() > 1 && p.endsWith("/")) p.remove(p.length() - 1);
    /* Reject traversal attempts */
    if (p.indexOf("..") >= 0) p = "/";
    return p;
}

bool OtaWebService::ensureSd() {
    if (gmap::sdMount()) return true;
    server_.send(503, "text/plain", "SD card not available");
    return false;
}

void OtaWebService::begin() {
    if (started_) return;

    server_.on("/", HTTP_GET, [this] { handleRoot(); });
    server_.on("/update", HTTP_POST,
               [this] { handleUpdatePost(); },
               [this] { handleUpdateUpload(); });
    server_.on("/sd/list", HTTP_GET, [this] { handleSdList(); });
    server_.on("/sd/upload", HTTP_POST,
               [this] { server_.send(sdUploadOk_ ? 200 : 500, "text/plain",
                                     sdUploadOk_ ? "Upload done" : "Upload failed"); },
               [this] { handleSdUpload(); });
    server_.on("/sd/download", HTTP_GET, [this] { handleSdDownload(); });
    server_.on("/sd/delete", HTTP_POST, [this] { handleSdDelete(); });
    server_.on("/sd/mkdir", HTTP_POST, [this] { handleSdMkdir(); });
    server_.onNotFound([this] { server_.send(404, "text/plain", "Not found"); });

    server_.begin();
    started_ = true;
    Serial.println("[OTA] web server started on port 80");
}

void OtaWebService::loop() {
    if (!started_) return;
    server_.handleClient();

    /* Reboot a moment after the OTA response has been flushed to the client. */
    if (rebootAt_ && (int32_t)(millis() - rebootMs_) >= 0) {
        Serial.println("[OTA] rebooting into new firmware");
        Serial.flush();
        ESP.restart();
    }
}

void OtaWebService::handleRoot() {
    server_.send_P(200, "text/html", kIndexHtml);
}

/* ---------------- firmware OTA ---------------- */

void OtaWebService::handleUpdateUpload() {
    HTTPUpload& up = server_.upload();
    if (up.status == UPLOAD_FILE_START) {
        otaOk_ = false;
        Serial.printf("[OTA] firmware upload start: %s\n", up.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (Update.write(up.buf, up.currentSize) != up.currentSize) {
            Update.printError(Serial);
        }
    } else if (up.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            otaOk_ = true;
            Serial.printf("[OTA] success, %u bytes\n", up.totalSize);
        } else {
            Update.printError(Serial);
        }
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        Update.abort();
        Serial.println("[OTA] upload aborted");
    }
}

void OtaWebService::handleUpdatePost() {
    if (otaOk_) {
        server_.send(200, "text/plain", "OK — flashed, rebooting");
        rebootAt_ = true;
        rebootMs_ = millis() + 500;
    } else {
        server_.send(500, "text/plain",
                     String("Update failed: ") + Update.errorString());
    }
}

/* ---------------- SD file manager ---------------- */

void OtaWebService::handleSdList() {
    if (!ensureSd()) return;
    String dir = normPath(server_.arg("dir"));

    File d = SD.open(dir);
    if (!d || !d.isDirectory()) {
        server_.send(404, "text/plain", "No such directory");
        return;
    }

    String json = "{\"dir\":\"" + dir + "\",\"files\":[";
    bool first = true;
    for (File f = d.openNextFile(); f; f = d.openNextFile()) {
        if (!first) json += ',';
        first = false;
        String name = f.name();
        int slash = name.lastIndexOf('/');
        if (slash >= 0) name = name.substring(slash + 1);
        json += "{\"name\":\"" + name + "\",\"dir\":" +
                (f.isDirectory() ? "true" : "false") +
                ",\"size\":" + String((uint32_t)f.size()) + "}";
        f.close();
    }
    json += "]}";
    d.close();
    server_.send(200, "application/json", json);
}

/* Create all intermediate directories of a file path (mkdir on an existing
 * directory fails harmlessly). */
static void mkdirsFor(const String& filePath) {
    for (int i = 1; (i = filePath.indexOf('/', i)) > 0; ++i)
        SD.mkdir(filePath.substring(0, i));
}

void OtaWebService::handleSdUpload() {
    HTTPUpload& up = server_.upload();
    if (up.status == UPLOAD_FILE_START) {
        sdUploadOk_ = false;
        if (!gmap::sdMount()) return;
        String dir = normPath(server_.arg("dir"));
        /* Folder uploads pass the file's relative path (sub/dir/file.ext) so
         * the directory tree is recreated on the SD card. */
        String rel = server_.arg("path");
        if (rel.isEmpty()) rel = up.filename;
        String path = normPath((dir == "/" ? "" : dir) + "/" + rel);
        mkdirsFor(path);
        SD.remove(path);
        uploadFile_ = SD.open(path, FILE_WRITE);
        Serial.printf("[SD] upload start: %s -> %s\n",
                      up.filename.c_str(), uploadFile_ ? "ok" : "OPEN FAILED");
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (uploadFile_) uploadFile_.write(up.buf, up.currentSize);
    } else if (up.status == UPLOAD_FILE_END) {
        if (uploadFile_) {
            uploadFile_.close();
            sdUploadOk_ = true;
            Serial.printf("[SD] upload done: %u bytes\n", up.totalSize);
        }
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        if (uploadFile_) uploadFile_.close();
        Serial.println("[SD] upload aborted");
    }
}

void OtaWebService::handleSdDownload() {
    if (!ensureSd()) return;
    String path = normPath(server_.arg("path"));
    File f = SD.open(path, FILE_READ);
    if (!f || f.isDirectory()) {
        server_.send(404, "text/plain", "No such file");
        return;
    }
    server_.sendHeader("Content-Disposition",
                       "attachment; filename=\"" +
                       path.substring(path.lastIndexOf('/') + 1) + "\"");
    server_.streamFile(f, "application/octet-stream");
    f.close();
}

/* SD.rmdir() only works on empty directories — delete contents first. */
static bool deleteRecursive(const String& path) {
    File f = SD.open(path);
    if (!f) return false;
    if (!f.isDirectory()) {
        f.close();
        return SD.remove(path);
    }
    File entry;
    while ((entry = f.openNextFile())) {
        String child = path + (path.endsWith("/") ? "" : "/") + entry.name();
        bool isDir = entry.isDirectory();
        entry.close();
        bool ok = isDir ? deleteRecursive(child) : SD.remove(child);
        if (!ok) {
            f.close();
            return false;
        }
        yield();   /* keep WiFi/watchdog alive while wiping large trees */
    }
    f.close();
    return SD.rmdir(path);
}

void OtaWebService::handleSdDelete() {
    if (!ensureSd()) return;
    String path = normPath(server_.arg("path"));
    if (path == "/") {
        server_.send(400, "text/plain", "Refusing to delete root");
        return;
    }
    bool ok = deleteRecursive(path);
    server_.send(ok ? 200 : 500, "text/plain",
                 ok ? "Deleted " + path : "Delete failed: " + path);
}

void OtaWebService::handleSdMkdir() {
    if (!ensureSd()) return;
    String path = normPath(server_.arg("path"));
    bool ok = SD.mkdir(path);
    server_.send(ok ? 200 : 500, "text/plain",
                 ok ? "Created " + path : "mkdir failed: " + path);
}

}  // namespace net
