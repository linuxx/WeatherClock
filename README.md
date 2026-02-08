# WeatherClock (ESP8266 + PlatformIO)

WeatherClock is an ESP8266 OLED clock/weather display with WiFi setup via WiFiManager and OpenWeather data by ZIP code.

## Hardware

- Board: `NodeMCU v2 (ESP8266)`
- Display: `SSD1306 128x64` I2C OLED (`0x3C`)
- Button: `D5` (single-click page cycle, hold at boot for reset/config flow)

## Features

- NTP-based clock (no RTC required)
- Weather from OpenWeather One Call 3.0
- ZIP + API key configuration in WiFiManager
- Multiple pages:
  - Home
  - Today
  - Hourly
  - 4-Day
  - Advisories
  - Wind
- Auto return to Home page after inactivity

## Setup

1. Install [PlatformIO](https://platformio.org/).
2. Open this folder.
3. Build/upload:
   - `pio run`
   - `pio run -t upload`
4. Open serial monitor at `115200`.

## First Run / Config

1. Device starts AP (if no saved WiFi or reset requested).
2. Connect to the AP and open the captive portal (or `192.168.4.1`).
3. Set:
   - WiFi credentials
   - ZIP code
   - OpenWeather API key
4. Save and reconnect.

## Runtime Notes

- Weather/time sync runs at boot and hourly.
- If weather fetch fails, UI shows `API ERROR`.
- If NTP/time fails, UI shows `NTP ERROR`.
- Serial output includes detailed `[OWM]` debug logs for parsed values.

## Project Layout

- `src/main.cpp` app loop, WiFiManager, sync orchestration
- `src/DisplayService.*` screen rendering
- `src/OpenWeatherService.*` geocode + weather API calls/parsing
- `src/OpenWeatherConfigService.*` persisted ZIP/API key config
- `src/TimeService.*` NTP + local clock offset handling

## Dependencies

Defined in `platformio.ini`:

- `Adafruit SSD1306`
- `Adafruit GFX Library`
- `WiFiManager`
