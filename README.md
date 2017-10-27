## WiFi Serial

Setup a web server to use as a serial chatbot on a ESP-01 device.

### Operation

The arduino code first goes through a predefined list of WiFi access points, if none is available it raises its own AP called **ESP_XXXXXX** where XXXXXX are the ESP-01 MAC address 6 last hex digits. In AP mode, default address is **192.168.64.1**

The code then raises a web server that takes the command to send to the serial port on the URI. Then it displays all the content read on the serial port up to the next prompt marker, defined as **": "** i.e. a column followed by a space character.

### Examples

Used as a chatbot for the [Polar Scanner](https://github.com/lyriarte/polar-scanner) arduino controller.

  * http://192.168.64.1/

```
CMD: CMD: CMD: CMD: CMD: 
```

  * http://192.168.64.1/SERVO

```
ANGLE: 
```
  * http://192.168.64.1/60 positions the servo motor 60 degrees on the X axis

```
CMD: 
```
