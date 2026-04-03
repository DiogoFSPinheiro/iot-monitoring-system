# IoT Environmental Monitoring System

End-to-end IoT system — Arduino Uno R3 collects environmental data and streams it to a FastAPI backend via Raspberry Pi.

## Architecture

```
┌─────────────────────────────────┐
│         Arduino Uno R3          │
│  DHT22 · BH1750FVI · HC-SR501   │
│      (FreeRTOS, 3 tasks)        │
└──────────────┬──────────────────┘
               │ UART 115200 baud
               │ binary protocol
       ┌───────▼────────┐
       │  Raspberry Pi  │
       │  serial bridge │
       └───────┬────────┘
               │ HTTP POST
  ┌────────────▼──────────────────┐
  │       FastAPI Backend         │
  │  PostgreSQL · Redis · Celery  │
  └───────────────────────────────┘
```

## Sensors

| Sensor     | Measures             | Protocol  |
|------------|----------------------|-----------|
| DHT22      | Temperature/Humidity | One-wire  |
| BH1750FVI  | Light intensity      | I2C       |
| HC-SR501   | Motion detection     | GPIO      |

## Subprojects

| Directory    | Description                                |
|--------------|--------------------------------------------|
| `embedded/`  | FreeRTOS firmware for Arduino Uno R3       |
| `backend/`   | FastAPI REST API — devices, readings, alerts |

## Documentation

- [`docs/architecture.md`](docs/architecture.md) — detailed system architecture
- [`docs/serial-protocol.md`](docs/serial-protocol.md) — binary serial protocol spec
- [`docs/api-examples.md`](docs/api-examples.md) — API usage examples

## Quick Start

### Backend

```bash
cd backend
cp .env.example .env        # fill in SECRET_KEY
docker compose up -d
alembic upgrade head
```

API docs: <http://localhost:8000/docs>

### Embedded

```bash
cd embedded
pio run --target upload
pio device monitor --baud 115200
```
