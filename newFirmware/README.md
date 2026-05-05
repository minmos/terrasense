# Important commands


To activate idf env for shell depending where your idf lies on your machine:
```bash
. $HOME/.espressif/tools/activate_idf_v6.0.sh
```
to be able to use `idf.py <something>` commands

**Configuration** | `idf.py menuconfig` this needs to be run once before hand to enter wifi and mqtt specifics

**full clean** | `idf.py fullclean` no idea how this is done in the IDE

### for OTA update by fetching binary from http server via mqtt trigger:
- build new binary, `cd build` and `ruby -run -e httpd . -p 8070` or any port of your choice to host firmware
- trigger update via mqtt, topic: `your_mqtt_basetopic+cmd/update` and payload: `http://<your_ip>:8070/newFirmware.bin`

## ESP32-C6

[ESP32-C6 info](https://www.studiopieters.nl/esp32-c6-pinout/ "esp32-c6-pinout")
![Pinout diagram](pinout.png)