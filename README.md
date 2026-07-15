![EspCall](https://img.shields.io/badge/EspCall-v1.0-green?style=flat)
![ESP32-S3](https://img.shields.io/badge/Platform-ESP32--S3-red?logo=espressif&logoColor=white)
![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP--IDF-orange?logo=platformio&logoColor=white)
![C](https://img.shields.io/badge/Language-C99-blue?logo=c&logoColor=white)
![License](https://img.shields.io/badge/License-MIT-blue)
![GitHub](https://img.shields.io/badge/GitHub-tsanthosh1328--coder-181717?logo=github&logoColor=white)

# EspCall

A VoIP phone built from scratch on ESP32-S3 that can dial real phone numbers over the internet.  
No libraries. No shortcuts. Pure C — custom SIP client, RTP packetizer, and G.711 codec on bare metal.

---

## Features

| Feature | Details |
|---|---|
| **Protocol** | SIP (RFC 3261) — REGISTER, INVITE, ACK, BYE |
| **Audio transport** | RTP over UDP (RFC 3550) |
| **Codec** | G.711 µ-law / PCMU (PT=0) |
| **Authentication** | MD5 Digest Auth (RFC 1321) — no external library |
| **SIP provider** | DIDLogic — calls real phone numbers worldwide |
| **Audio I/O** | INMP441 I2S mic + MAX98357A I2S DAC + 8Ω speaker |
| **Network** | WiFi via ESP-IDF lwIP stack |
| **Framework** | ESP-IDF (not Arduino) via PlatformIO CLI |

---

## Hardware

| Component | Purpose |
|---|---|
| ESP32-S3-DevKitC-1 (N16R8) | Main MCU — 16MB Flash, 8MB PSRAM, dual-core LX7 |
| INMP441 | I2S MEMS microphone |
| MAX98357A | I2S DAC + Class D amplifier |
| 8Ω speaker | Audio output |

---

## Project Structure

```
src/
├── main.c          — Entry point, WiFi init, pipeline orchestration
├── audio/          — I2S driver for INMP441 (mic) and MAX98357A (speaker)
├── codec/          — G.711 µ-law encoder/decoder
├── rtp/            — RTP packetizer and receiver over UDP
└── sip/            — Lightweight SIP client (REGISTER, INVITE, ACK, BYE)
```

---

## Call Flow

```
ESP32-S3
   │
   ├─ I2S ── INMP441 (mic input)
   ├─ I2S ── MAX98357A (speaker output)
   │
   └─ WiFi
        │
        ▼
   DIDLogic SIP Server (sip1.didlogic.com:5060)
        │
        ▼
   PSTN → Real phone number
```

---

## Milestones

| # | Milestone | Status |
|---|---|---|
| 1 | I2S audio loopback (mic → speaker) | ⏳ Pending hardware |
| 2 | G.711 µ-law encode/decode | ✅ Done |
| 3 | RTP packetizer over LAN | ✅ Done |
| 4 | SIP client — REGISTER with DIDLogic | ✅ Done |
| 5 | SIP-to-SIP call via Linphone/Zoiper | ⏳ Pending hardware |
| 6 | PSTN call to real phone number | ⏳ Pending hardware |

---

## Build & Flash

> Coming soon — hardware in transit.

---

## License

MIT — see [LICENSE](LICENSE) for details.

## Author

Santhosh — EE Student, IIT Bombay  
[github.com/tsanthosh1328-coder](https://github.com/tsanthosh1328-coder)
