# Docs++: Distributed Network File System


## Overview
**Docs++** is a scalable, fault-tolerant Distributed Network File System developed in C. Built using a robust multi-threaded architecture with POSIX sockets, it separates metadata management from data storage, supporting concurrent client operations, sentence-level locking, automated replication, and granular access control. It enables multiple clients to connect, perform transactional file operations, and manage files securely over a network.

## Key Features
- **Distributed Architecture**: Separation of concerns between a centralized **Name Server** (managing metadata, directory structure, and load balancing) and multiple **Storage Servers** (providing persistent data storage and rapid file retrieval).
- **Concurrency & Synchornization**: Implements robust concurrency control. Writers lock specific sentence IDs (row-level locking), allowing multiple users to edit different parts of a file simultaneously without conflict.
- **Access Control List (ACL) & Workflow**: Built-in permission system with Owner, Read, and Write modes. Users can seamlessly request permissions, and owners can review, approve, or deny access requests.
- **Versioning & Checkpoints**: Allows saving point-in-time snapshots of file contents. Users can list checkpoints, read historical versions, and instantly revert the live file to a previous state.
- **Fault Tolerance & Data Replication**: Implements a 3x replication factor. The Name Server continuously monitors Storage Server health via heartbeats. In case of node failure, data is preserved, and missing replicas are automatically synced.
- **Hierarchical Storage**: Full support for creating logical directories, moving files between folders, and hierarchical viewing.
- **Data Streaming & Execution**: Unique support for streaming file contents (simulating audio/video) with dynamic delays, and remote script execution directly on the Storage Servers.

## Tech Stack
- **Language**: C
- **Networking/I/O**: POSIX Sockets, TCP/IP
- **Concurrency**: Pthreads, Mutexes, Reader-Writer Locks
- **Data Structures**: Tries (for hierarchical directory modeling and fast path resolution), Hash Maps

## Architecture & Project Structure
- `name_server/`: Source code for the Name Server (Metadata management, Trie, Load Balancer, Authentication, Replication Management).
- `storage_server/`: Source code for the Storage Server (Data Persistence, Local File IO, Locking, Syncing, Client Data Transmission).
- `client/`: Interactive Command Line Interface (CLI) resolving user commands into network requests.
- `include/`: Shared protocol headers defining network command strings, constants, and network structures.
- `bin/`: Compiled executable binaries.
- `ss_data/`: Root persistent data storage directory populated by Storage Servers at runtime.

---

## Installation & Build

Build the Name Server, Storage Server, and Client binaries seamlessly using the provided `Makefile`:

```bash
# Clean previous builds and server logs
make clean

# Compile all binaries into the bin/ directory
make
```

---

## Running the System

Docs++ supports both single-machine local testing and full multi-machine cluster deployments.

### Localhost (Single Machine Testing)
Open 3 separate terminals.

1. **Start Name Server:**
   ```bash
   ./bin/name_server
   ```
2. **Start Storage Server:**
   ```bash
   # Usage: ./bin/storage_server <Port> <NM_IP> <SS_IP>
   ./bin/storage_server 9002 127.0.0.1 127.0.0.1
   ```
3. **Start Client:**
   ```bash
   # Usage: ./bin/client <NM_IP>
   ./bin/client 127.0.0.1
   ```
   *(You will be prompted to enter a username to register with the Name Server.)*

### Multi-Machine (LAN / Wi-Fi)
*Assume **Machine A** (IP: `192.168.1.50`) hosts the Name Server, and **Machine B** (IP: `192.168.1.60`) acts as a Client and secondary Storage Server.*

1. **Machine A (Name Server):**
   ```bash
   ./bin/name_server
   ```
2. **Machine A (Storage Server 1):**
   ```bash
   ./bin/storage_server 9002 192.168.1.50 192.168.1.50
   ```
3. **Machine B (Client):**
   ```bash
   ./bin/client 192.168.1.50
   ```
> **Firewall Warning:** Ensure ports **9001** (Name Server default) and any specified SS ports (e.g. **9002**) are allowed through your firewall (`sudo ufw allow 9001/tcp`).

---

## Command Reference

Once the client CLI is started, you can input the following commands. If a syntax error is made, the CLI automatically prints the correct usage.

### 1. File Operations
| Command | Syntax | Description |
| :--- | :--- | :--- |
| **CREATE** | `CREATE <filename>` | Create a new empty file in the DFS. |
| **READ** | `READ <filename>` | Read complete file content from the assigned Storage Server. |
| **WRITE** | `WRITE <filename> <sentence_id>` | Acquire a lock on a specific sentence. Provide word updates, terminate with `ETIRW` to commit. |
| **DELETE** | `DELETE <filename>` | Delete a file permanently. |
| **INFO** | `INFO <filename>` | View file metadata (size, owner, character count, and access rights). |
| **STREAM** | `STREAM <filename>` | Stream file content dynamically (delayed word-by-word streaming). |
| **EXEC** | `EXEC <filename>` | Execute a script remotely directly on the Storage Server, returning the output. |

### 2. Access Control System (ACL)
| Command | Syntax | Description |
| :--- | :--- | :--- |
| **ADDACCESS** | `ADDACCESS -R <file> <user>` | Grant a user Read privileges. |
| | `ADDACCESS -W <file> <user>` | Grant a user full Write (and Read) privileges. |
| **REMACCESS** | `REMACCESS <file> <user>` | Revoke all non-owner permissions for a given user. |
| **REQACCESS** | `REQACCESS <file> -R` | Request Read access from the file's owner. |
| | `REQACCESS <file> -W` | Request Write access from the file's owner. |
| **VIEWREQUESTS**| `VIEWREQUESTS <filename>` | *(Owner Only)* View all pending access requests for your file. |
| **APPROVE** | `APPROVE <req_id>` | Approve a pending request. |
| **DENY** | `DENY <req_id>` | Deny a pending request. |
| **MYREQUESTS** | `MYREQUESTS` | Track the status of your sent requests (Pending, Approved, Denied). |

### 3. Versioning & Organization
| Command | Syntax | Description |
| :--- | :--- | :--- |
| **CHECKPOINT** | `CHECKPOINT <file> <tag>` | Take a snapshot of the current file state, tagged with a specific name. |
| **VIEWCHECKPOINT**| `VIEWCHECKPOINT <file> <tag>` | Read the contents from a saved checkpoint tag. |
| **REVERT** | `REVERT <file> <tag>` | Fast-revert the live file to the exact state of a specified checkpoint. |
| **LISTCHECKPOINTS**| `LISTCHECKPOINTS <file>` | Display all saved checkpoint tags and timestamps for a file. |
| **CREATEFOLDER**| `CREATEFOLDER <name>` | Create a logical directory on the network. |
| **MOVE** | `MOVE <file> <folder>` | Migrate a file into a designated folder. |
| **VIEWFOLDER** | `VIEWFOLDER <name>` | Display the subtree of contents within a specific folder. |

### 4. General Commands
| Command | Syntax | Description |
| :--- | :--- | :--- |
| **VIEW** | `VIEW [-l, -a]` | View accessible files. `-l` gives a detailed list table. `-a` displays the full global tree (Admin level). |
| **LIST** | `LIST` | List all network-registered users (clients). |
| **HELP** | `HELP` | Prints the syntax and descriptions of all supported system commands. |
| **EXIT / QUIT** | `EXIT` | Disconnect the client safely. |

