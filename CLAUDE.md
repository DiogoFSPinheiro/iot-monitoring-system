# IoT Environmental Monitoring System

## Purpose

Portfolio project for Diogo Pinheiro (Software Developer / Embedded Developer).
Two linked subprojects that form an end-to-end IoT system:

1. **embedded/** — Environmental monitoring station (C/C++, FreeRTOS, Arduino Uno)
2. **backend/** — Device & sensor data management API (Python, FastAPI, PostgreSQL)

The embedded station collects sensor data and sends it to the backend API.
Both projects must be **production-quality portfolio pieces** — clean code, good docs, solid tests.

## Project Structure

```
iot-monitoring-system/
├── CLAUDE.md
├── README.md                  # Project overview with architecture diagram
├── embedded/
│   ├── CLAUDE.md              # Embedded-specific instructions
│   ├── README.md
│   ├── platformio.ini
│   ├── src/
│   │   ├── main.cpp
│   │   ├── tasks/             # FreeRTOS tasks (one file per task)
│   │   ├── drivers/           # Sensor drivers (I2C, one-wire, GPIO)
│   │   ├── protocols/         # Serial communication protocol
│   │   └── config/            # Pin definitions, thresholds
│   ├── include/
│   ├── lib/
│   └── test/
├── backend/
│   ├── CLAUDE.md              # Backend-specific instructions
│   ├── README.md
│   ├── pyproject.toml
│   ├── docker-compose.yml
│   ├── Dockerfile
│   ├── alembic/               # DB migrations
│   ├── app/
│   │   ├── main.py
│   │   ├── api/               # Route handlers
│   │   ├── models/            # SQLAlchemy models
│   │   ├── schemas/           # Pydantic schemas
│   │   ├── services/          # Business logic
│   │   ├── core/              # Config, security, dependencies
│   │   └── workers/           # Celery tasks
│   └── tests/
├── docs/
│   ├── architecture.md
│   ├── serial-protocol.md     # Binary protocol spec
│   └── api-examples.md
└── .github/
    └── workflows/             # CI for backend (lint, test, build)
```

## Tech Stack

### Embedded
- **Language:** C++ (C++17)
- **RTOS:** FreeRTOS (Arduino_FreeRTOS_Library)
- **Build:** PlatformIO (NOT Arduino IDE)
- **Board:** Arduino Uno R3 (ATmega328P, 2KB SRAM, 32KB Flash)
- **Sensors:**
  - DHT22 (temperature + humidity, one-wire protocol)
  - BH1750FVI (light intensity, I2C protocol)
  - HC-SR501 PIR (motion detection, digital GPIO)
- **Protocols used:** I2C (BH1750), one-wire (DHT22), GPIO (PIR), UART/Serial (Pi comms)
- **Alerts:** Built-in LED (pin 13)
- **Testing:** Unity test framework (PlatformIO native)

### Backend
- **Language:** Python 3.12+
- **Framework:** FastAPI
- **DB:** PostgreSQL 16 + SQLAlchemy 2.0 (async) + Alembic
- **Cache/Queue:** Redis + Celery
- **Auth:** JWT (python-jose) + bcrypt
- **Testing:** pytest + pytest-asyncio + httpx (async test client)
- **Linting:** ruff
- **Type checking:** mypy (strict mode)
- **Containerization:** Docker + Docker Compose

## Code Style

### C++ (embedded)
- IMPORTANT: Use PlatformIO, never Arduino IDE
- C++17 standard
- snake_case for functions and variables, PascalCase for classes/structs, UPPER_SNAKE for constants
- Every sensor driver must have a clean header file with doxygen-style comments
- Use `const` and `constexpr` aggressively
- No dynamic memory allocation after FreeRTOS scheduler starts (pre-allocate everything)
- Task stack sizes must be explicitly defined and documented
- IMPORTANT: Uno has only 2KB SRAM — memory is extremely tight. Monitor free RAM.
- Prefer `static` for file-scoped functions
- Always check return values from hardware reads

### Python (backend)
- Follow PEP 8 strictly — enforced by ruff
- Type hints on ALL functions (mypy strict)
- snake_case everywhere (PEP 8)
- Use `async def` for all route handlers and DB operations
- Pydantic v2 for all schemas
- SQLAlchemy 2.0 style (mapped_column, not legacy Column)
- Never use `print()` — use `logging` module
- Docstrings for public functions (Google style)

## Key Commands

### Embedded
```bash
# Build
pio run

# Upload to board
pio run --target upload

# Run unit tests (native)
pio test -e native

# Serial monitor
pio device monitor --baud 115200

# Clean
pio run --target clean
```

### Backend
```bash
# Start all services
docker compose up -d

# Run API locally (dev)
uvicorn app.main:app --reload --port 8000

# Run tests
pytest -v --cov=app --cov-report=term-missing

# Lint
ruff check app/ tests/
ruff format app/ tests/

# Type check
mypy app/

# DB migrations
alembic upgrade head
alembic revision --autogenerate -m "description"

# Celery worker
celery -A app.workers.celery_app worker --loglevel=info
```

## Architecture Decisions

### Serial Protocol (embedded → backend)
- Binary protocol, NOT text/JSON (teaches real embedded comms)
- Format: `[START_BYTE][LENGTH][MSG_TYPE][PAYLOAD][CRC8]`
- Baud rate: 115200
- Uno has a single hardware Serial shared with USB — the Pi reads directly via USB-serial cable (no logic level converter needed)
- Full spec in `docs/serial-protocol.md`

### FreeRTOS Task Design (Uno — memory-constrained)
- 3 tasks + idle to fit in 2KB SRAM
- I2C bus protected by mutex (xSemaphoreTake/xSemaphoreGive)
- Tasks communicate via a single shared queue
- Alert logic is embedded in the comms task (no separate alert task)
- IMPORTANT: Monitor stack high water marks with `uxTaskGetStackHighWaterMark()`

### Sensor Selection Rationale
- DHT22 + BH1750 + PIR form a coherent environmental monitoring narrative
- PIR enables event correlation: "motion detected → temperature/humidity/light delta analysis"
- Three distinct protocols (I2C, one-wire, GPIO) demonstrate breadth of embedded knowledge

### Backend API Design
- RESTful, versioned under `/api/v1/`
- Consistent error responses: `{"detail": "message", "code": "ERROR_CODE"}`
- Pagination on all list endpoints (limit/offset)
- Rate limiting on ingestion endpoints
- Background alert processing via Celery (don't block API responses)

## Git Workflow
- Branch per feature: `feat/sensor-dht22`, `feat/api-auth`, `fix/serial-crc`
- Commit messages: conventional commits (`feat:`, `fix:`, `docs:`, `test:`, `refactor:`)
- Never commit directly to `main`
- Squash merge PRs

## What NOT to Do
- Do NOT use Arduino IDE or `.ino` files — PlatformIO only
- Do NOT use `delay()` in FreeRTOS tasks — use `vTaskDelay()` or `xTaskDelayUntil()`
- Do NOT use global mutable state without mutex protection in embedded code
- Do NOT use `String` class in embedded — fragments the heap. Use `char[]` and `snprintf`
- Do NOT use `*` imports in Python
- Do NOT store secrets in code — use environment variables
- Do NOT skip error handling — every DB call, every sensor read, every API call must handle failures
- Do NOT write tests that depend on execution order
- Do NOT allocate memory dynamically after FreeRTOS scheduler starts on Uno
- Do NOT access I2C bus without acquiring the mutex first

## Lessons Learned
<!-- Add entries here when Claude makes mistakes or you discover better patterns -->
<!-- Format: - YYYY-MM-DD: Description of lesson -->
