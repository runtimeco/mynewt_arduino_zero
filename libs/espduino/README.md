# libs/espduino

Library to talk to ESPLink running on ESP8266.
API tries to capture the spirit of https://github.com/tuanpmt/espduino.git.

There are multiple restrictions:
 - not multi-threaded, or even protected against it
 - input from ESP is 'polled' (need to call esp_process() to drain
   responses)
 - HTTP responses have to fit into 512 bytes
 - poor error reporting

TODO:
 - mqtt
 - create a task, make reports asyncronous
 - create API to register handler for wifi state notifications
 - create a queue for HTTP requests
