//Copyright 2022-2023(c) John Sullivan

#include "muddersMIMA.h"

//JTS2doLater: use whichever SPI slave mode starts with clock high when CS pulls down

////////////////////////////////////////////////////////////////////////////////////

enum Mode {
    MODE_NONE,
    MODE_OEM,
    MODE_BLENDED,
    MODE_MANUAL_REGEN,
    MODE_OLD
};

Mode receivedMode = MODE_NONE;

void spiToLiBCM_begin() {
  // Configure SPI pins
    pinMode(PIN_SPI_MISO, OUTPUT);
    pinMode(PIN_SPI_MOSI, INPUT);
    pinMode(PIN_SPI_SCK, INPUT);
    pinMode(PIN_SPI_CS, INPUT);  // CS is handled by the master

    // Enable SPI in Slave mode
    SPCR = (1 << SPE);
}

void handleReceivedMode(Mode mode) {
    Serial.print("Received Mode: ");
    switch (mode) {
        case MODE_NONE:
            Serial.println("MODE_NONE");
            break;
        case MODE_OEM:
            Serial.println("MODE_OEM");
            break;
        case MODE_BLENDED:
            Serial.println("MODE_BLENDED");
            break;
        case MODE_MANUAL_REGEN:
            Serial.println("MODE_MANUAL_REGEN");
            break;
        case MODE_OLD:
            Serial.println("MODE_OLD");
            break;
        default:
            Serial.println("Unknown Mode");
            break;
    }
}

void LiBCM_handler() {
    // Wait for reception complete (SPIF flag set)
    if (SPSR & (1 << SPIF)) {
        // Read received data from SPI Data Register
        uint8_t receivedData = SPDR;

        // Cast received data to Mode enum
        receivedMode = static_cast<Mode>(receivedData);

        // Handle received mode
        handleReceivedMode(receivedMode);
    }
}