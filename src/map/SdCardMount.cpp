#include "map/SdCardMount.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#include "board/BoardPins.h"

namespace gmap {

static bool s_mounted = false;
static SPIClass s_spi(HSPI);   /* ONE instance, shared by all providers */

bool sdMount() {
    if (s_mounted) return true;

    Serial.flush();   /* flush before SD init so partial lines don't hide crash point */

    s_spi.begin(board::kPinSdSck, board::kPinSdMiso,
                board::kPinSdMosi, board::kPinSdCs);

    Serial.println("[SD] SPI bus started");
    Serial.flush();

    /* 25 MHz: a 128 KB tile reads in ~50 ms instead of ~330 ms at 4 MHz.
     * Slow reads made the first map fetch hog core 0 for ~8 s and trip the
     * idle-task watchdog → continuous panic/reboot. */
    if (!SD.begin(board::kPinSdCs, s_spi, 25000000)) {
        Serial.println("[SD] SD.begin() failed — check card / wiring");
        Serial.flush();
        return false;
    }

    s_mounted = true;
    Serial.printf("[SD] mounted, type=%d, size=%llu MB\n",
                  (int)SD.cardType(), SD.cardSize() / (1024 * 1024));
    Serial.flush();
    return true;
}

void sdUnmount() {
    if (!s_mounted) return;
    SD.end();
    s_mounted = false;
}

bool sdMounted() { return s_mounted; }

}  // namespace gmap
