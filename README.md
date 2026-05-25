# QLog

QLog is a Linux log capture tool for cellular modules. In this repo, the main use case is collecting Qualcomm diagnostic logs that can be opened in QXDM.

## Build

Build from the QLOG repository:

```bash
make
```

The binary is generated at `out/QLog`.

## Collect 5G QXDM Logs

This repo already includes 5G filter files under `conf/`. For a general 5G NR capture, start with `conf/defaultNR5G1216.cfg`.

### 1. Find the diag port

Use the diag device exposed by the module:

- USB modules: usually `/dev/ttyUSB0`

### 2. Start log capture

Example for a USB-based 5G module:

```bash
mkdir -p logs/5g
sudo ./out/QLog -p /dev/ttyUSB0 -s ./logs/5g -f ./conf/defaultNR5G1216.cfg -m 200 -n 20
```

What these options do in the context of 5G capture:

- `-p`: selects the module diag port
- `-s`: directory where logs are written
- `-f`: loads the 5G NR filter config from this repo
- `-m 200`: rotates each log file at 200 MB
- `-n 20`: keeps up to 20 rotated files

### 3. Stop capture

Press `Ctrl+C` when you have collected enough data.

### 4. Check the output

QLog writes Qualcomm log files (*.qmdl / *.qmdl2) into the save directory:

Open the generated files using QXDM/SCat for decoding and analysis.
