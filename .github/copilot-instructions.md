# Swiss Railway E-Paper Display Project

## Architecture Overview
This is an ESP8266-based e-paper display system that shows Swiss railway station departures. The system fetches real-time train connection data from the Swiss Transport API (`transport.opendata.ch`) and renders it on a 4.2" dual-color e-paper display.

**Key Components:**
- **Main Loop**: Fetches JSON data every 2 minutes, parses connections, renders to display
- **E-Paper Library**: GxEPD2 for 400x300 dual-color display (black/red/white)
- **Display Rendering**: Uses full screen updates with text drawing, supports pixel shifting for scrolling
- **API Integration**: HTTPS requests to transport.opendata.ch with specific field filtering

## Core Patterns

### Display Rendering
- **Color Constants**: `GxEPD_BLACK` (black text), `GxEPD_WHITE` (white background), `GxEPD_RED` (red highlights)
- **Text Drawing**: Use `epd.setFont()`, `epd.setTextColor()`, `epd.setCursor()`, `epd.print()`
- **Font Selection**: Available Adafruit GFX fonts like `FreeMonoBold9pt7b`, `FreeMonoBold12pt7b`, `FreeMonoBold18pt7b`, `FreeMonoBold24pt7b` - choose based on content density
- **Pixel Shifting**: Implement scrolling with `pixelShift` variable (0-4 range) added to Y coordinates in setCursor

### Data Processing
- **API Fields**: Query `connections?from=8503099&to=8503000&transportations[]=train&limit=5&fields[]=connections/from/departure&fields[]=connections/from/delay`
- **Time Parsing**: Handle timezone in departure time by truncating at '+' or 'Z', then use `strptime()` with `"%Y-%m-%dT%H:%M:%S"` format, `strftime()` to `"%H:%M"`
- **JSON Structure**: Access `doc["connections"]` array, then `station["from"]["departure"]` and `["delay"]`
- **String Building**: Concatenate display lines using `std::string.append()` for departure times and delays

### Networking
- **HTTPS Client**: Use `WiFiClientSecure` with fallback to HTTP, or certificate fingerprint validation
- **SSL Configuration**: `client->setInsecure()`, `setBufferSizes(1024, 1024)`, `setSSLVersion(BR_TLS12, BR_TLS12)`
- **Fallback Strategy**: Try HTTP first, then HTTPS with insecure SSL for development
- **WiFi Connection**: Standard ESP8266WiFi with blocking connection loop
- **NTP Time**: Required for SSL validation - `configTime(1 * 3600, 0, "pool.ntp.org", "time.nist.gov")`

## Development Workflow

### Building & Flashing
- Use PlatformIO: `pio run -t upload` to build and flash to NodeMCU v2
- Monitor serial output: `pio device monitor` (115200 baud)
- Dependencies managed via `platformio.ini` lib_deps

### Debugging
- Serial prints throughout main loop for connection status, HTTP codes, JSON parsing
- Check WiFi connection status before HTTPS requests
- Validate JSON deserialization errors with `err.f_str()`
- Detailed failure logging in `fetchTrainData()` for WiFi status, HTTPS begin failures, and HTTP error codes
- Fallback strategy: tries HTTP first, then HTTPS with SSL configuration
- JSON structure debugging: prints parsed JSON and available keys when "connections" not found

## Project-Specific Conventions

### Display Layout
- Header line at Y=10: Route description (e.g., "S4 Sihlau -> Zurich")
- Departure entries: Y=10+(row*30), left-aligned time at X=10, delay at X=200
- EPD initialization: `epd.init(115200); epd.setRotation(0); epd.fillScreen(GxEPD_WHITE); epd.display()`

### Timing & Updates
- 2-minute intervals between updates (`delay(120000)`)
- Pixel shift increments every cycle for subtle scrolling effect
- NTP sync on startup for SSL certificate validation

### Code Organization
- Single `main.cpp` file with `setup()`, `loop()`, `fetchTrainData()`, and `displayTrainData()` functions
- PlatformIO structure with lib_deps for GxEPD2, Adafruit GFX, and ArduinoJson
- Use Arduino framework with ESP8266WiFi, HTTPClient, ArduinoJson

## Key Files
- `src/main.cpp`: Main application logic
- `platformio.ini`: ESP8266/NodeMCU v2 configuration with GxEPD2, Adafruit GFX, and ArduinoJson dependencies</content>
<parameter name="filePath">d:\OneDrive\Documents\PlatformIO\Projects\SwissRailwayEpaper\.github\copilot-instructions.md