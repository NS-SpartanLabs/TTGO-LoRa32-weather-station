/*
    OTAA auth core code taken from otaa example
    - Reads sensor values from BME280 and sends it to TTN
    - ...
    
    author: Nicholas Singleton, SpartanLabs
    date:   June 2022
*/

#include <Arduino.h>
#include "lmic.h"
#include <hal/hal.h>
#include <SPI.h>
#include <SSD1306.h>
#include "device_config.h" /* Replace by device_config.h and add TTN keys to it */

#include "Bme280.h"

/* Blue LED on TTGO LILYGO -> To signal TX-ing*/
#define LEDPIN 2

/* OLED Display I2C pins and Reset */
#define OLED_I2C_ADDR 0x3C
#define OLED_RESET 16
#define OLED_SDA 4
#define OLED_SCL 15

/* OLED display instance */
SSD1306 display (OLED_I2C_ADDR, OLED_SDA, OLED_SCL);

/* Bme280 object */
Bme280 bme280;

#define NBR_MEAS            (3) /* Number or meas to send over LoRa to TTN */
#define LEN_MEAS_BYTES      (2) /* number of bytes it takes each sensor meas*/
#define LEN_PAYLOAD         ( NBR_MEAS * LEN_MEAS_BYTES )

/* Aux vars */
uint32_t counter    = 0;
uint8_t  cnt_mroda  = 0;
uint8_t addr = 0;
float_t sensor_val[5] = {0.0, 0.1, 0.2, 0.3, 0.4};

char TTN_response[30];


/* Get TTN credentials */
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8);}
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8);}
void os_getDevKey (u1_t* buf) { memcpy_P(buf, APPKEY, 16);}

static osjob_t sendjob;

// Schedule TX every this many seconds (might become longer due to duty
// cycle limitations).
const unsigned TX_INTERVAL = 60;

// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = 18,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = 14,
    .dio = {26, 33, 32}  /* Pins for the Heltec ESP32 Lora board/ TTGO Lora32 with 3D metal antenna */
};


float read_sensor(uint8_t addr)
{
    return sensor_val[addr];
}

void send_uptime_OLED(unsigned long time)
{
    display.drawString(0, 40, "Up Time in [min]: ");
    display.drawString(80, 40, String(time));
}

void send_sensor_val_OLED(uint8_t addr)
{
    display.drawString(0, 30, "Sensor Val : ");
    display.drawString(80, 30, String(read_sensor(addr)));
}

/*! @brief Returns the upTime for the uController
 *
 *  @param[out] scaled_time     upTime in minutes
 */
uint32_t get_time(void)
{
    uint32_t scaled_time = 0;
    scaled_time = (uint32_t) (millis() / 1000) / 60; // in minutes

    return scaled_time;
}

/*! @brief  Returns a pointer to the payload data to be sent over LoRa
 *
 */
uint8_t* gen_TX_msg(uint8_t incr)
{
    /* LoRa payload array message */
    static uint8_t payload_lora[LEN_PAYLOAD] = {0};

    /* Get sensor values */
    float temperature = bme280.getTemperature() + incr;
    float humidity    = bme280.getHumidity() + incr;
    float pressure    = bme280.getPressure() + incr;

    int16_t celciusInt  = temperature * 100; // convert to signed 16 bits integer
    payload_lora[0]     = celciusInt >> 8;
    payload_lora[1]     = celciusInt;

    int16_t humidityInt = humidity * 100;
    payload_lora[2]     = humidityInt >> 8;
    payload_lora[3]     = humidityInt;

    int16_t pressureInt = pressure    * 100;
    payload_lora[4]     = pressureInt >> 8;
    payload_lora[5]     = pressureInt;
    
    return payload_lora;
}


void do_send(osjob_t* j){

    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        /* Prepare upstream data transmission at the next possible time */
        
        // LMIC_setTxData2(1, (uint8_t*)&temperature, sizeof(float), 0);
        LMIC_setTxData2(1, gen_TX_msg(cnt_mroda), LEN_PAYLOAD, 0);
        cnt_mroda++;
        if(cnt_mroda == 20)
        {
            cnt_mroda = 0;
        }
        
        Serial.println(F("Sending uplink packet..."));
        digitalWrite(LEDPIN, HIGH);
        display.clear();
        display.drawString (0, 0, "Sending uplink packet...");
        display.drawString (0, 50, String (++counter));
        display.display ();
    }
    // Next TX is scheduled after TX_COMPLETE event.
}

void onEvent (ev_t event) {
    Serial.print(os_getTime());
    Serial.print(": ");
    switch(event)
    {
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            display.clear();
            display.drawString (0, 0, "TX Done ...");

            if (LMIC.txrxFlags & TXRX_ACK) {
                Serial.println(F("Received ack"));
                display.drawString (0, 20, "Received ACK.");
            }

            if (LMIC.dataLen) {
                int i = 0;
                // data received in rx slot after tx
                Serial.print(F("Data Received: "));
                Serial.write(LMIC.frame+LMIC.dataBeg, LMIC.dataLen);
                Serial.println();
                Serial.println(LMIC.rssi);

                display.drawString (0, 9, "Received DATA.");
                for ( i = 0 ; i < LMIC.dataLen ; i++ )
                TTN_response[i] = LMIC.frame[LMIC.dataBeg+i];
                TTN_response[i] = 0;
                display.drawString (0, 22, String(TTN_response));
                display.drawString (0, 32, String(LMIC.rssi));
                display.drawString (64,32, String(LMIC.snr));
            }

                /* Schedule next transmission */
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            digitalWrite(LEDPIN, LOW);
            display.drawString (0, 50, String (counter));
            
            /* get upTime and read sensor */
            send_uptime_OLED(get_time());
            send_sensor_val_OLED(addr);
            addr++;
            
            if (addr == 4)
            {
                addr = 0;
            }

            display.display ();
            // Schedule next transmission
            os_setTimedCallback(&sendjob, os_getTime()+sec2osticks(TX_INTERVAL), do_send);
            break;

        case EV_JOINING:
            Serial.println(F("EV_JOINING: -> Joining..."));
            display.drawString(0,16 , "OTAA joining...");
            
            /* get uptime and read sensor */
            send_uptime_OLED(get_time());
            send_sensor_val_OLED(addr);

            display.display();
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            display.clear();
            display.drawString(0 , 0 ,  "Joined!");
            display.display();
            // Disable link check validation (automatically enabled
            // during join, but not supported by TTN at this time).
            LMIC_setLinkCheckMode(0);
            break;

        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;

        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;

        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;

        default:
            Serial.println(F("Unknown event"));
            break;
    }
}

void setup(void)
{
    Serial.begin(9600);
    delay(2500); /* Give the Serial comm sometime to pick up */
    Serial.println(F("Starting..."));

    /* Blue pin to signal TX */
    pinMode(LEDPIN,OUTPUT);

    /* Set up and reset the OLED Display */
    pinMode(OLED_RESET, OUTPUT);
    digitalWrite(OLED_RESET, LOW);
    delay(50);
    digitalWrite(OLED_RESET, HIGH);

    display.init();
    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);

    display.setTextAlignment (TEXT_ALIGN_LEFT);

    display.drawString (0, 0, "Starting....");
    display.display ();

    /* BME280 init */
    // if( !bme280.init() )
    // {
    //     Serial.println(F("BME280 init failed!!"));
    // }
    // else
    // {
    //     bme280.printBmeValues();
    // }
    
    /* LMIC init */
    os_init();

    // Reset the MAC state. Session and pending data transfers will be discarded.
    LMIC_reset();
    LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100);
    
    /*  Set up the channels used by the Things Network, which corresponds
        to the defaults of most gateways. Without this, only three base
        channels from the LoRaWAN specification are used, which certainly
        works, so it is good for debugging, but can overload those
        frequencies, so be sure to configure the full frequency range of
        your network here (unless your network autoconfigures them).
        Setting up channels should happen after LMIC_setSession, as that
        configures the minimal channel set.
    */
    // LMIC_setupChannel(0, 868100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    // LMIC_setupChannel(1, 868300000, DR_RANGE_MAP(DR_SF12, DR_SF7B), BAND_CENTI);      // g-band
    // LMIC_setupChannel(2, 868500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    // LMIC_setupChannel(3, 867100000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    // LMIC_setupChannel(4, 867300000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    // LMIC_setupChannel(5, 867500000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    // LMIC_setupChannel(6, 867700000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    // LMIC_setupChannel(7, 867900000, DR_RANGE_MAP(DR_SF12, DR_SF7),  BAND_CENTI);      // g-band
    // LMIC_setupChannel(8, 868800000, DR_RANGE_MAP(DR_FSK,  DR_FSK),  BAND_MILLI);      // g2-band
    
    /*  TTN defines an additional channel at 869.525Mhz using SF9 for class B
        devices' ping slots. LMIC does not have an easy way to define set this
        frequency and support for class B is spotty and untested, so this
        frequency is not configured here.
    */
    // Disable link check validation
    // LMIC_setLinkCheckMode(0);

    // TTN uses SF9 for its RX2 window.
    // LMIC.dn2Dr = DR_SF9;

    // Set data rate and transmit power for uplink (note: txpow seems to be ignored by the library)
    // LMIC_setDrTxpow(DR_SF11,14);
    // LMIC_setDrTxpow(DR_SF7,14);

    /* Start job */
    do_send(&sendjob);     // Will fire up also the joinning to TTN
    //LMIC_startJoining();
}


/* App's While(True) */
void loop()
{
    os_runloop_once();
}