# MQTT Topic Hierarchy Proposal

This document defines a structured MQTT topic scheme for managing multiple terrariums, enabling consistent auto-discovery (e.g., Home Assistant MQTT discovery), clear separation of state vs. command topics, and support for various sensor/actuator types.

## Core Structure

Basic pattern:  
`terrariums/<terrarium_id>/<component_type>/<device_id>/<attribute>`

- **terrariums** – fixed domain prefix  
- **<terrarium_id>** – unique identifier per terrarium (e.g., `rainforest_01`, `desert_03`)  
- **<component_type>** – one of: `sensor`, `actuator`, `number`, `fan`, `binary_sensor`  
- **<device_id>** – logical name of the physical device + unique suffix/description, concatinated with `_` (e.g., `ds18b20_toprightcorner`, `ssr_heatinglamp`)  
- **<attribute>** – e.g., `state`, `command`, `temperature`, `humidity`, `speed`

## Consistency Rules Overview

| Element | Rule |
|---------|------|
| **No spaces** | Use underscores `_` |
| **Lowercase only** | `rainforest_01` |
| **Device ID** | `<logical_name>_<unique_suffix>` |
| **Component type** | Use HA MQTT component names, [MQTT components list](https://www.home-assistant.io/integrations/mqtt/#configuration-via-mqtt-discovery "www.home-assistant.io/integrations/mqtt/#configuration-via-mqtt-discovery")|

## Discovery Topic Pattern

For Home Assistant MQTT auto-discovery, use:

`homeassistant/<component_type>/<object_id>/config`

`object_id` is a concatination with `-` of `terrariums`, `terrarium_id` and `device_id`


Example:  
`homeassistant/sensor/terrariums-rainforest_01-ssr_heatinglamp/config`

Payload contains `state_topic`, `command_topic` (if applicable), `device`, `unique_id` (`unique_id` THE SAME AS `object_id`), etc.

## State vs. Command Topics

| Topic type | Pattern | Example |
|------------|---------|---------|
| State (read-only) | `terrariums/<terrarium_id>/<component_type>/<device_id>/state` | `terrariums/rainforest_01/sensor/ambient_sht35/state` |
| Command (write) | `terrariums/<terrarium_id>/<component_type>/<device_id>/command` | `terrariums/rainforest_01/actuator/misting_ssr/command` |

**Rule:**  
- `state` → device publishes measurements/status
- `command` → controller publishes actions


## Device-Specific Examples
### Sensors

#### **OneWire – DS18B20** (temperature)  
- Component: `sensor`  
- Device ID: `onewire_ds18b20_<serial_suffix>`  
- Topics:  
  - State: `terrariums/rainforest_01/sensor/ds18b20_toprightcorner/state` (payload: `{"temperature": 25.3}`)  
  - Auto-discovery: `homeassistant/sensor/terrariums-rainforest_01-_ds18b20_toprightcorner/config`

#### **I2C – SHT35 (temperature + humidity)**  
Treat as two separate sensors or one device with multiple state topics.  
Recommended: separate devices for clarity.  

- Temp device: `sht35_temperature`  
- Humidity device: `sht35_humidity`  

State topics:  
`terrariums/rainforest_01/sensor/sht35_bottom_temperature/state` → `{"temperature": 23.5}`  
`terrariums/rainforest_01/sensor/sht35_bottom_humidity/state` → `{"humidity": 58.2}`  

*(Optional: single device with `state` publishing `{"temperature": x, "humidity": y}` – less granular but simpler.)*


#### **Simple I/O – XKC-Y25 (water detection)**  
Component: `binary_sensor`  
Device ID: `water_leak_xkc_y25`  
State topic: `terrariums/rainforest_01/binary_sensor/water_leak_xkc_y25/state` (payload: `{"water_detected": true}` or just `ON`/`OFF`)

### Actuators

#### **Simple I/O – SSR (Misting, Lights, Heating)**  
Component: `actuator` (or `switch`)  
Device ID: `ssr_misting`, `ssr_lights`, `ssr_heater`  
Command topic: `terrariums/rainforest_01/actuator/ssr_misting/command` (payload: `ON`/`OFF`)  

#### **Fan Control – 3‑pin & 4‑pin fans**  
For 3‑pin (voltage control) / 4‑pin (PWM) – both benefit from separate speed control.  

Component: `fan` (Home Assistant supports fan component)  
Device ID: `fanpwm_1`  
Command topic: `terrariums/rainforest_01/fan/fanpwm_1/command`  
Payload: `ON`/`OFF` or `{"speed": 128}` (0–255)  
State topic: `terrariums/rainforest_01/fan/fanpwm_1/state`  



