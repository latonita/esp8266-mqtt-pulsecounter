#Simple pulse counter which sends data over MQTT#
* This counter uses interrupts on 2 pins - I have two water meters attached to ESP8266 via simplest LM393 comparator circuit.
* It is "talk-only" client. 
* It sends out data every 60 seconds by default.
* It keeps adding pulses until succesful connection.

 ###MQTT Message consists of 
       P1, P2 - number of pulses counted since last succesful connection to server
       Sec - period during which we collected P1 and P2, seconds. 
       Per - number of base periods we collected data for. >1 if we failed to connect to server last time
       Up  - ESP8266 uptime

Copyright (C) 2016 Anton Viktorov <latonita@yandex.ru>

This library is free software. You may use/redistribute it under The MIT License terms. 
