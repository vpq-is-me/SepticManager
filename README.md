# SepticManager

This is part of Home Automation Server software stack and it's purpose is  managing interaction with septic automation. For more information please see [home web server](https://github.com/vpq-is-me/home_auto_nodejs.git) description.
Mainly this application 
- transmit messages to/from underlying application [UART_server](https://github.com/vpq-is-me/UART_server.git) which provide connection to Bluetooth MESH net.
- service sqlite database of received data when alarm occures/disappeare 
- provide data on request from __web server__


_for notes check vpq_notes.txt_
