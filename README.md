# Student Database Management System — Client/Server Edition

> A production-quality, modular **TCP Client-Server** student database built entirely in **pure C** — no external libraries.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         CLIENT SIDE                             │
│                                                                 │
│  client_menu.c  ──builds──▶  Request  ──────────────────────┐  │
│  (ANSI CLI menu)                                             │  │
│  client.c       ──sends──▶  TCP socket  ────────────────────┼──┼──▶ SERVER
│                 ◀─reads──   Response   ◀────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                         SERVER SIDE                             │
│                                                                 │
│  server.c        ── accept() ──▶  handle_client()              │
│  request_handler.c ─ dispatch() ─▶ db_*(database.c)            │
│  database.c      ── student.c + file_manager.c + search/sort   │
│  file_manager.c  ── students.dat  (binary, atomic write)        │
└─────────────────────────────────────────────────────────────────┘
```

### Layered Module Design

| Layer | Module | Responsibility |
|-------|--------|----------------|
| Transport | `server.c`, `client.c` | Socket lifecycle, accept loop |
| Protocol | `protocol.h` | Wire format: Request/Response structs |
| Dispatch | `request_handler.c` | OpCode → db_* routing |
| Database | `database.c` | Registry lifecycle, CRUD orchestration |
| Business | `student.c` | CRUD on StudentRegistry |
| Algorithms | `search.c`, `sort.c` | Binary/linear search, qsort comparators |
| I/O | `file_manager.c` | Binary file, atomic write, checksum |
| Validation | `validation.c` | All input sanitisation |

---

## Folder Structure

```
project_C/
│
├── common/                  # Shared by client + server
│   ├── student.h            # Student struct, ResultCode, registry API
│   ├── protocol.h           # Request/Response wire format, OpCode enums
│   └── validation.h         # Input validation function prototypes
│
├── server/                  # Server-only headers + source
│   ├── server.c             # TCP accept loop (entry point: server.exe)
│   ├── request_handler.c/h  # OpCode dispatch switch
│   ├── database.c/h         # DB abstraction layer (db_* API)
│   ├── file_manager.h       # Binary I/O API
│   ├── search.h             # Search algorithm API
│   └── sort.h               # Sort algorithm API
│
├── client/                  # Client-only source
│   ├── client_menu.c        # CLI menu, input collection (entry point: client.exe)
│   ├── client.c             # TCP connect/send/recv
│   └── client.h             # ClientConn handle
│
├── src/                     # Shared implementations
│   ├── student.c            # StudentRegistry CRUD
│   ├── file_manager.c       # Binary file I/O, atomic write, hashing
│   ├── validation.c         # 15 validators, sanitisers, masked password
│   ├── search.c             # Binary/linear/substring/range search
│   ├── sort.c               # qsort comparators + insertion sort
│   └── protocol.c           # opcode_to_string() utility
│
├── data/
│   └── students.dat         # Binary database (created automatically)
│
├── docs/
│   └── screenshots/
│
├── Makefile
└── README.md
```

---

## Communication Protocol

### Wire Format

Both structs are **fixed-size** — no framing, no length prefix needed.

```
Client ──[ sizeof(Request) bytes ]──▶ Server
Server ──[ sizeof(Response) bytes ]──▶ Client
```

### Request (Client → Server)

```c
typedef struct {
    int        operation;          // OpCode
    int        search_type;        // SearchType (OP_SEARCH only)
    int        sort_key;           // SortKey (OP_VIEW_ALL only)
    int        student_id;         // For DELETE / SEARCH by ID
    char       search_keyword[64]; // For SEARCH by name / dept
    Student    student;            // For ADD and UPDATE
} Request;
```

### Response (Server → Client)

```c
typedef struct {
    int        status;                           // ResultCode
    char       message[128];                     // Human-readable result
    int        count;                            // Records in students[]
    Student    students[100];                    // Returned records
    float      avg_gpa;                          // Report: average GPA
    float      top_gpa;                          // Report: highest GPA
    int        total_active;                     // Report: total students
    char       top_student_name[64];             // Report: top performer
} Response;
```

### Operation Codes

| Code | Value | Description |
|------|-------|-------------|
| `OP_ADD` | 1 | Add a new student |
| `OP_VIEW_ALL` | 2 | Retrieve all active records |
| `OP_SEARCH` | 3 | Search by ID / Name / Dept |
| `OP_UPDATE` | 4 | Update an existing record |
| `OP_DELETE` | 5 | Soft-delete a record |
| `OP_REPORT` | 6 | Aggregate statistics |
| `OP_PING` | 7 | Connectivity check |
| `OP_QUIT` | 8 | Graceful disconnect |

---

## Compilation

### Requirements
- **Windows**: MinGW GCC (`gcc` via Git Bash or MSYS2), or any C99-compatible compiler
- **Linux/macOS**: `gcc` or `clang`

### Build Both (Recommended)

```bash
make
```

### Build Individually

```bash
make server    # produces server.exe (Windows) or server (POSIX)
make client    # produces client.exe (Windows) or client (POSIX)
```

### Manual Compile (no make)

```bash
# Server
gcc -Wall -std=c99 -I. -Icommon -Iserver -Isrc \
    src/validation.c src/protocol.c src/student.c \
    src/file_manager.c src/search.c src/sort.c \
    server/database.c server/request_handler.c server/server.c \
    -o server.exe -lws2_32

# Client
gcc -Wall -std=c99 -I. -Icommon -Iclient -Isrc \
    src/validation.c src/protocol.c src/student.c \
    client/client.c client/client_menu.c \
    -o client.exe -lws2_32
```

---

## How to Run

**Terminal 1 — Start Server:**

```bash
./server.exe          # default port 8080
./server.exe 9090     # custom port
```

Expected output:
```
╔══════════════════════════════════════════╗
║  Student Database System  —  SERVER      ║
╚══════════════════════════════════════════╝
[DB]     Initialised. 0 active record(s) loaded.
[Server] Listening on 0.0.0.0:8080 ...
[Server] Press Ctrl+C to stop.
```

**Terminal 2 — Start Client:**

```bash
./client.exe                          # connects to 127.0.0.1:8080
./client.exe 192.168.1.10 9090        # remote server
```

---

## Features

| Feature | Details |
|---------|---------|
| **CRUD** | Add, View, Search, Update, Delete (soft-delete) |
| **Search** | By ID (binary O(log n)), Name (substring, O(n)), Dept (exact, O(n)) |
| **Sort** | By Name, GPA (desc), Roll, ID — server-side before sending |
| **Reports** | Total students, avg GPA, top GPA, top performer name |
| **File I/O** | Binary format, magic bytes `SRMS`, XOR checksum, atomic write |
| **Validation** | 15 validators: ID, name, GPA, email, phone, dept, age, year |
| **Password** | Salted djb2 hash — no plaintext stored; masked input (`*`) |
| **Protocol** | Fixed-size binary structs; no serialisation library needed |
| **Scalability** | `handle_client()` is pthread-ready; threading = one-line change |
| **Portability** | `#ifdef _WIN32` guards throughout; compiles on Windows & POSIX |

---

## Data File Format

```
students.dat:
┌──────────────────────────────┐
│  FileHeader (fixed size)     │  magic "SRMS" + version + checksum
├──────────────────────────────┤
│  Student[0]  (fixed size)    │
│  Student[1]                  │
│  ...                         │
│  Student[n-1]                │
└──────────────────────────────┘
```

- **Atomic writes**: data written to `students.dat.tmp`, then renamed
- **Soft deletes**: `is_active = 0` keeps records until explicit purge
- **Corruption detection**: XOR checksum + magic bytes on every load

---

## Future Enhancements

- [ ] **Multithreading** — replace `handle_client()` call with `pthread_create()` in `server.c` (one line)
- [ ] **SHA-256 passwords** — swap djb2 for OpenSSL `EVP_Digest` (single function change in `file_manager.c`)
- [ ] **Pagination protocol** — add `page` / `page_size` fields to Request for large datasets
- [ ] **Department-wise report** — add `db_report_by_dept()` returning count per department
- [ ] **CSV export** — add `OP_EXPORT` opcode; server streams a CSV Response
- [ ] **TLS** — wrap socket I/O in OpenSSL for encrypted transport

---

## Skills Demonstrated

```
C Programming          Socket Programming      Modular Architecture
Binary File I/O        Atomic Writes           Checksum / Integrity
Dynamic Memory         Binary Search (O(log n)) qsort Comparators
Input Validation       Error Propagation       Cross-Platform Code
Client-Server Design   Wire Protocol Design    Production Patterns
```

---

## License

MIT License — see [LICENSE](LICENSE)
