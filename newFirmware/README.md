# Important commands


To activate idf env for shell depending where your idf lies on your machine:
```bash
. $HOME/.espressif/tools/activate_idf_v6.0.sh
```
to be able to use `idf.py <something>` commands

**Configuration** | `idf.py menuconfig` this needs to be run once before hand to enter wifi and mqtt specifics. Important: `idf.py menuconfig` needs to be run in project root folder.

**full clean** | `idf.py fullclean` no idea how this is done in the IDE

### for OTA update by fetching binary from http server via mqtt trigger:
- build new binary, `cd build` and `ruby -run -e httpd . -p 8070` or any port of your choice to host firmware
- trigger update via mqtt, topic: `your_mqtt_basetopic+cmd/update` and payload: `http://<your_ip>:8070/newFirmware.bin`

### Hardware/Pin Configurations:
`components/sys_config/include/sys_config.h`is responsible for all pin configurations except the debug LED, 
- `ONEWIRE_BUS_GPIO` defines the GPIO pin used for One-Wire sensors like DS18B20
- The struct below is used to define the DS18B20 sensors with their name and corresponding address, the rest is handled in sensors component
```c
static const ds18b20_target_t HARDWARE_DS18B20_CONFIG[] = {
    { .name = "Temporärer Temp DS18B20-Sensor",  .rom_address = 0x133C6CF64930E728 },
    // { .name = "xyz Temp", .rom_address = 0x... }
};
``` 



## ESP32-C6 Infos

[ESP32-C6 pinout information](https://www.studiopieters.nl/esp32-c6-pinout/ "esp32-c6-pinout")
![Pinout diagram](pinout.png)