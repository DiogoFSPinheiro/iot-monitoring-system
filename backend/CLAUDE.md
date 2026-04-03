# Backend — IoT Device Management API

@../CLAUDE.md for full project context.

## Overview

RESTful API for managing IoT devices, ingesting sensor data, and processing alerts. Built with FastAPI, PostgreSQL, Redis, and Celery.

## API Structure

All endpoints under `/api/v1/`. Auth required unless noted.

```
POST   /api/v1/auth/register          (public)
POST   /api/v1/auth/login              (public)
POST   /api/v1/auth/refresh

GET    /api/v1/devices
POST   /api/v1/devices
GET    /api/v1/devices/{id}
PUT    /api/v1/devices/{id}
DELETE /api/v1/devices/{id}

POST   /api/v1/devices/{id}/readings   (device API key auth)
GET    /api/v1/devices/{id}/readings?start=&end=&type=&limit=&offset=
GET    /api/v1/devices/{id}/readings/summary?period=hour|day|week

GET    /api/v1/alerts
POST   /api/v1/alerts/rules
GET    /api/v1/alerts/rules
PUT    /api/v1/alerts/rules/{id}
DELETE /api/v1/alerts/rules/{id}

GET    /api/v1/dashboard/summary
```

## Database Schema

```
users
├── id: UUID (PK)
├── email: str (unique, indexed)
├── hashed_password: str
├── role: enum(admin, user)
├── created_at: timestamp
└── updated_at: timestamp

devices
├── id: UUID (PK)
├── owner_id: UUID (FK → users.id)
├── name: str
├── api_key: str (unique, indexed, for device auth)
├── location: str (nullable)
├── is_active: bool (default true)
├── last_seen_at: timestamp (nullable)
├── created_at: timestamp
└── updated_at: timestamp

sensor_readings
├── id: BIGINT (PK, auto)
├── device_id: UUID (FK → devices.id, indexed)
├── sensor_type: enum(temperature, humidity, light, motion)
├── value: float
├── recorded_at: timestamp (indexed, from device)
└── received_at: timestamp (server time)
INDEX: (device_id, sensor_type, recorded_at DESC)

alert_rules
├── id: UUID (PK)
├── device_id: UUID (FK → devices.id)
├── sensor_type: enum
├── condition: enum(gt, lt, gte, lte, eq)
├── threshold: float
├── level: enum(info, warning, critical)
├── is_active: bool
├── created_at: timestamp
└── updated_at: timestamp

alert_events
├── id: BIGINT (PK, auto)
├── rule_id: UUID (FK → alert_rules.id)
├── device_id: UUID (FK → devices.id)
├── sensor_type: enum
├── value: float
├── threshold: float
├── level: enum
├── triggered_at: timestamp (indexed)
└── acknowledged_at: timestamp (nullable)
```

## Project Layout

```
backend/
├── app/
│   ├── __init__.py
│   ├── main.py                    # FastAPI app factory
│   ├── core/
│   │   ├── config.py              # Settings via pydantic-settings
│   │   ├── security.py            # JWT encode/decode, password hashing
│   │   ├── database.py            # Async engine + session factory
│   │   └── dependencies.py        # get_db, get_current_user
│   ├── api/
│   │   ├── __init__.py
│   │   └── v1/
│   │       ├── __init__.py
│   │       ├── router.py          # Aggregates all v1 routers
│   │       ├── auth.py
│   │       ├── devices.py
│   │       ├── readings.py
│   │       ├── alerts.py
│   │       └── dashboard.py
│   ├── models/
│   │   ├── __init__.py
│   │   ├── user.py
│   │   ├── device.py
│   │   ├── reading.py
│   │   └── alert.py
│   ├── schemas/
│   │   ├── __init__.py
│   │   ├── user.py
│   │   ├── device.py
│   │   ├── reading.py
│   │   ├── alert.py
│   │   └── common.py              # PaginatedResponse, ErrorResponse
│   ├── services/
│   │   ├── __init__.py
│   │   ├── auth_service.py
│   │   ├── device_service.py
│   │   ├── reading_service.py
│   │   └── alert_service.py
│   └── workers/
│       ├── __init__.py
│       ├── celery_app.py
│       └── alert_processor.py
├── alembic/
│   ├── env.py
│   └── versions/
├── tests/
│   ├── conftest.py                # Fixtures: async client, test db, auth headers
│   ├── test_auth.py
│   ├── test_devices.py
│   ├── test_readings.py
│   ├── test_alerts.py
│   └── test_dashboard.py
├── pyproject.toml
├── Dockerfile
├── docker-compose.yml
└── .env.example
```

## Key Patterns

### App Factory (main.py)
```python
def create_app() -> FastAPI:
    app = FastAPI(title="IoT Monitoring API", version="1.0.0")
    app.include_router(v1_router, prefix="/api/v1")
    return app

app = create_app()
```

### Dependency Injection
```python
# All route handlers receive DB and user via Depends()
async def get_readings(
    device_id: UUID,
    db: AsyncSession = Depends(get_db),
    current_user: User = Depends(get_current_user),
):
```

### Service Layer
- Route handlers do validation + response formatting ONLY
- Business logic lives in `services/`
- Services receive the DB session as a parameter (testable)
- Services raise `HTTPException` for expected errors

### Pagination
```python
# Standard pattern for all list endpoints
class PaginatedResponse(BaseModel, Generic[T]):
    items: list[T]
    total: int
    limit: int
    offset: int
```

### Device Authentication
- Devices authenticate with API key (sent in `X-API-Key` header)
- Users authenticate with JWT Bearer token
- The readings ingestion endpoint accepts BOTH (API key for devices, JWT for manual testing)

## Docker Compose Services

```yaml
services:
  api:        # FastAPI app (port 8000)
  db:         # PostgreSQL 16 (port 5432)
  redis:      # Redis 7 (port 6379)
  worker:     # Celery worker
```

## Environment Variables

Defined in `.env.example`:
```
DATABASE_URL=postgresql+asyncpg://iot:iot@db:5432/iot_monitoring
REDIS_URL=redis://redis:6379/0
SECRET_KEY=change-me-in-production
ACCESS_TOKEN_EXPIRE_MINUTES=30
REFRESH_TOKEN_EXPIRE_DAYS=7
```

IMPORTANT: Settings loaded via `pydantic-settings` in `core/config.py`. Never read `os.environ` directly.

## Testing Strategy

- All tests are async (`pytest-asyncio`)
- Use `httpx.AsyncClient` as test client (NOT TestClient)
- Test DB: separate PostgreSQL database, created/dropped per test session
- Fixtures in `conftest.py`:
  - `db_session` — async session with rollback after each test
  - `client` — authenticated async HTTP client
  - `test_user` — pre-created user
  - `test_device` — pre-created device with API key
- Test coverage target: >80%
- IMPORTANT: Every endpoint needs at least: happy path, auth failure, validation error, not found

## Dependency Versions

Pin exact versions in `pyproject.toml`:
```toml
[project]
dependencies = [
    "fastapi>=0.115.0,<1.0",
    "uvicorn[standard]>=0.32.0",
    "sqlalchemy[asyncio]>=2.0.36",
    "asyncpg>=0.30.0",
    "alembic>=1.14.0",
    "pydantic>=2.10.0",
    "pydantic-settings>=2.6.0",
    "python-jose[cryptography]>=3.3.0",
    "passlib[bcrypt]>=1.7.4",
    "celery[redis]>=5.4.0",
    "redis>=5.2.0",
    "httpx>=0.28.0",
]

[project.optional-dependencies]
dev = [
    "pytest>=8.3.0",
    "pytest-asyncio>=0.24.0",
    "pytest-cov>=6.0.0",
    "ruff>=0.8.0",
    "mypy>=1.13.0",
    "httpx>=0.28.0",
]
```

## Common Mistakes to Avoid

- Do NOT use sync SQLAlchemy — everything must be async (asyncpg + AsyncSession)
- Do NOT return raw model instances from endpoints — always convert to Pydantic schema
- Do NOT forget to await async operations (SQLAlchemy 2.0 async is easy to miss)
- Do NOT store plain passwords — always hash with bcrypt
- Do NOT hardcode config values — use pydantic-settings
- Do NOT skip pagination — every list endpoint must paginate
- Do NOT put business logic in route handlers — use the service layer
- Do NOT create circular imports between models (use TYPE_CHECKING)
- Do NOT forget to add indexes on columns used in WHERE/ORDER BY clauses
- Do NOT write tests that hit the real database — use the test fixtures

## Lessons Learned
<!-- Add entries here when Claude makes mistakes -->
