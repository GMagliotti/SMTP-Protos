# SMTP Server & Monitor

Version: 0.0.1

- 62466 - Juan Ignacio Fernández Dinardo
- 62780 - Francisco Marcos Ferrutti
- 61172 - Gianfranco Magliotti
- 63401 - Mateo Roman Pérez de Gracia

## Requirements

- Linux (tested on Ubuntu 22.04)
- GNU Make >= 4.3
- Bash >= 5.1.16

# Compilation

Run

```
make
```

in the src directory to compile the SMTP server & in the monitor_client directory to compile the Monitor.

# Execution

Both the client and server executables are created inside compilation directory.

```
#SMTP server
./smtpd.elf
```
```
#Monitor
./client_monitor.elf
```
