# NVCS — A Git Alternative in C++

A production-ready distributed version control system written in C++17.  
Features a CLI client (`nvcs`) and an HTTP server (`nvcs-server`) with zlib object compression.

---

## Architecture

```
vcs/
├── include/nvcs/
│   ├── core/          # Object model: blob, tree, commit, index, refs, repository
│   ├── net/           # HTTP transport & pack protocol
│   └── util/          # SHA-256, zlib compression, filesystem helpers
├── src/               # Implementation files
├── cli/               # CLI client (nvcs)
│   └── commands/      # One file per sub-command
├── server/            # HTTP server (nvcs-server)
├── third_party/       # cpp-httplib, nlohmann/json (single-header, vendored)
└── dist/bin/          # Built binaries
```

### Storage Format

Each repository stores its data under `.nvcs/`:

```
.nvcs/
  HEAD                  # "ref: refs/heads/main" or commit hash (detached)
  config                # INI-style config (user, remotes, core settings)
  index                 # Staging area: hash, mode, size, mtime per file
  objects/ab/cdef...    # zlib-compressed, SHA-256 addressed objects
  refs/
    heads/<branch>      # Branch tip hashes
    tags/<tag>          # Tag hashes
    remotes/<remote>/   # Remote-tracking refs
```

**Object types:**
| Type   | Content |
|--------|---------|
| Blob   | Raw file bytes |
| Tree   | Directory listing: `mode name hash` per entry |
| Commit | tree, parents, author/committer signatures, message |

All objects are zlib-compressed at level 6 before writing to disk.  
Object identity is the SHA-256 of `"type size\0data"`.

---

## Building

```bash
cd vcs
./build.sh          # Configures, builds, and installs to dist/
```

Requires: `cmake`, `g++` (C++17), `zlib`, `openssl`, `pkg-config`.

---

## CLI Quick Start

```bash
NVCS=./dist/bin/nvcs

# Initialize a repository
$NVCS init my-project
cd my-project

# Configure identity
$NVCS config user.name "Your Name"
$NVCS config user.email "you@example.com"

# Stage and commit
$NVCS add README.md src/
$NVCS commit -m "initial commit"

# Status / diff / log
$NVCS status
$NVCS diff
$NVCS log --oneline

# Branches
$NVCS branch feature/my-feature
$NVCS checkout feature/my-feature
$NVCS commit -m "add feature"
$NVCS checkout main

# Tags
$NVCS tag v1.0
$NVCS tag              # list tags

# Remotes
$NVCS remote add origin http://host:9000/repos/my-project
$NVCS push origin main
$NVCS pull origin main

# Clone
$NVCS clone http://host:9000/repos/my-project [dest-dir]
```

---

## Server

```bash
# Start the server
./dist/bin/nvcs-server --port 9000 --repo-dir /var/nvcs-repos

# Or via the run script
PORT=9000 REPO_DIR=./repos ./run-server.sh
```

### Server REST API

| Method | Path | Description |
|--------|------|-------------|
| GET | `/healthz` | Health check |
| GET | `/repos` | List repositories |
| POST | `/repos` | Create repository `{"name":"..."}` |
| GET | `/repos/:name` | Repository info |
| DELETE | `/repos/:name` | Delete repository |
| GET | `/repos/:name/info/refs` | Advertise refs (pull/clone entry point) |
| POST | `/repos/:name/upload-pack` | Serve objects to client (pull/clone) |
| POST | `/repos/:name/receive-pack` | Accept objects from client (push) |

---

## Network Protocol

Client-server communication uses JSON over HTTP:

1. **fetch_refs** → `GET /repos/:name/info/refs` — returns all refs as `{ref_name: hash}`
2. **upload-pack** → `POST` with `{wants: [...], haves: [...]}` — server returns a pack
3. **receive-pack** → `POST` with `{updates: [...], pack: base64}` — pushes objects

Pack format: `[4-byte object count][{4-byte hash_len}{hash}{4-byte data_len}{compressed_data}]*`

---

## Differences from Git

| Feature | NVCS | Git |
|---------|------|-----|
| Hash algorithm | SHA-256 (64-char hex) | SHA-1 / SHA-256 |
| Compression | zlib (all objects) | zlib (loose) + delta packs |
| Protocol | HTTP + JSON + base64 pack | Smart HTTP / SSH |
| Config format | INI-like | INI-like |
| Merge | Not implemented (use branches) | 3-way merge |
| Delta compression | Not implemented | Yes (pack files) |

---

## Production Deployment

```bash
# Example: run server as a systemd service
nvcs-server \
  --host 0.0.0.0 \
  --port 9000 \
  --repo-dir /var/lib/nvcs/repos
```

For HTTPS, place the server behind nginx or a TLS-terminating proxy.
