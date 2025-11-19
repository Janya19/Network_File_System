# OSN Course Project: Final Network Protocol Specification

## 1. Overview

This document defines the complete network protocol for the **fault-tolerant, hierarchical** Distributed File System. It details all communication flows between the three components:
* **Client (`client`)**
* **Name Server (`name_server` or NM)**
* **Storage Server (`storage_server` or SS)**

This is the definitive "contract" for the project. All messages and constants are defined in `protocol.h`.

## 2. General Principles

* **Transport:** All communication uses **TCP**.
* **Format:** All messages are **text-based (ASCII)** and **newline-terminated (`\n`)**. Servers must read from sockets in a loop until a `\n` is found.
* **Structure:** `COMMAND_PREFIX <arg1> <arg2> ...\n`. All prefixes are in `protocol.h`.
* **Paths:** All file and folder paths are **absolute** (e.g., `/`, `/docs/file.txt`). The root is `/`.
* **Responses:** All server responses start with a 3-digit Status Code (from `protocol.h`).
    * `CODE <optional_payload>\n`
    * `200 OK` (Success)
    * `202 127.0.0.1 9002` (Success, with SS info)
    * `404 File not found` (Error)
* **Connections:**
    * **NM-Client & NM-SS:** A single, persistent TCP connection is maintained. A `read()` returning 0 signals a disconnect/crash, which the NM must handle.
    * **Client-SS:** A new, temporary TCP connection is made for each data operation (`READ`, `WRITE`, `STREAM`, etc.).

## 3. Component Initialization

### 3.1. Client Initialization

1.  Client starts, prompts user for `username`.
2.  Client connects to the NM at `NM_LISTEN_PORT`.
3.  **Client -> NM:** `C_INIT <username>\n`
4.  NM registers the client and keeps the connection.

### 3.2. Storage Server Initialization & Recovery

This flow handles both a *new* SS and a *recovering* (restarting) SS.

1.  SS starts. It's given its IP, NM-facing port, and Client-facing port.
2.  SS listens on its two ports.
3.  SS connects to the NM at `NM_LISTEN_PORT`.
4.  **SS -> NM:** `S_INIT <ip> <nm_facing_port> <client_facing_port>\n`
5.  **NM Logic:**
    * **If New SS:** NM adds it to the pool of available servers.
    * **If Recovering SS:** NM marks it as "RECOVERING". It finds all files this SS *should* have (as Primary or Replica). For each file, it finds a "good" copy on another live SS.
    * **NM -> SS (Recovering):** `NM_PULL_SYNC <path> <source_ss_ip> <source_ss_port>\n`
    * (This tells the recovering SS to connect to the "good" SS and pull the latest file version. This must be done for all files and checkpoints.)
    * After all sync commands are *sent*, the NM marks the SS as "ALIVE".

## 4. Response Code Summary (Reference)

| Code | `protocol.h` Name | Meaning |
| :--- | :--- | :--- |
| **200** | `RESP_OK` | **OK:** Generic success.
| **201** | `RESP_LOCKED` | **OK (Locked):** SS to Client. Sentence lock acquired.
| **202** | `RESP_SS_INFO`| **OK (Info):** NM to Client. Payload is the IP/Port of the SS to contact.
| | | |
| **400** | `RESP_BAD_REQ` | **Bad Request:** Malformed command, wrong args.
| **403** | `RESP_FORBIDDEN`| **Forbidden:** No permission (not owner, no R/W access, etc.).
| **404** | `RESP_NOT_FOUND`| **Not Found:** File, folder, or sentence index does not exist.
| **405** | `RESP_NOT_EMPTY`| **Not Empty:** Cannot delete a folder that is not empty.
| **409** | `RESP_CONFLICT` | **Conflict:** Item already exists at this path (on `CREATE`).
| | | |
| **500** | `RESP_SRV_ERR` | **Internal Server Error:** A generic error on the NM or SS.
| **5.0.3** | `RESP_SS_DOWN` | **SS Unavailable:** All replicas for this file are currently down.
| **504** | `RESP_LOCKED_ERR`| **Sentence Locked:** Cannot lock; already locked by another user.

## 5. Data Payload Formats (Reference)

* **`VIEW` Payload:** A single string, items separated by `;`, fields by `,`.
    * `D,/docs,userA;F,/file.txt,userB,69,420;`
    * Format: `Type(D/F),Path,Owner[,WordCount,CharCount]`
* **`LISTREQS` Payload:**
    * `reqID1,userA,R;reqID2,userC,W;`
    * Format: `RequestID,Username,PermissionSought`
* **`LISTCHECKPOINTS` Payload:**
    * `tag1,2025-10-10_14:32;tag2,2025-10-11_09:01;`
    * Format: `Tag,Timestamp`

---

## 6. Core & Bonus Command Flows

### 6.1. File & Folder Operations

#### `CREATE <path>` (e.g., `/docs/file.txt`)
1.  **Client -> NM:** `C_REQ_CREATE /docs/file.txt\n`
2.  NM checks if parent (`/docs`) exists and if user has 'W' access in it.
3.  NM checks if `/docs/file.txt` already exists (`409`).
4.  **Replication:** NM selects a **Primary SS** (SS1) and a **Replica SS** (SS2).
5.  **NM -> SS1 (Primary):** `NM_CREATE /docs/file.txt\n`
6.  `SS1 -> NM: 200\n`
7.  NM updates metadata: `/docs/file.txt -> {primary: SS1, replica: SS2, ...}`
8.  **NM -> Client:** `200\n`
9.  **Asynchronously:** NM sends the create command to the replica.
    * **NM -> SS2 (Replica):** `NM_CREATE /docs/file.txt\n` (NM does not wait for this ACK).

#### `CREATEFOLDER <path>`
1.  **Client -> NM:** `C_CREATE_FOLDER /new_folder\n`
2.  NM checks parent (`/`) for 'W' access.
3.  NM checks if `/new_folder` exists (`409`).
4.  NM updates its *internal directory tree* metadata. (No SS is contacted).
5.  **NM -> Client:** `200\n`

#### `READ <path>`
1.  **Client -> NM:** `C_REQ_READ /docs/file.txt\n`
2.  NM checks for file existence (`404`) and 'R'/'W' access (`403`).
3.  **Fault Tolerance:**
    * NM finds file metadata: `{primary: SS1, replica: SS2}`.
    * NM checks liveness of SS1.
    * **If SS1 is ALIVE:** `NM -> Client: 202 <SS1_ip> <SS1_port>\n`
    * **If SS1 is DEAD:** NM checks liveness of SS2.
    * **If SS2 is ALIVE:** `NM -> Client: 202 <SS2_ip> <SS2_port>\n`
    * **If SS1 & SS2 are DEAD:** `NM -> Client: 503\n`
4.  Client opens a **new connection** to the provided SS.
5.  **Client -> SS:** `SS_GET_FILE /docs/file.txt\n`
6.  **SS -> Client:** `[Raw file bytes streamed]`
7.  SS closes connection.

#### `WRITE <path> <sentence_num>`
1.  **Client -> NM:** `C_REQ_WRITE /docs/file.txt\n`
2.  NM checks for existence (`404`) and 'W' access (`403`).
3.  **Fault Tolerance:** NM finds Primary SS (SS1).
    * **If SS1 (Primary) is ALIVE:** `NM -> Client: 202 <SS1_ip> <SS1_port>\n`
    * **If SS1 is DEAD:** NM promotes Replica (SS2) to be the new Primary.
    * `NM -> Client: 202 <SS2_ip> <SS2_port>\n`
4.  Client opens a **new connection** to the provided SS (now the Primary).
5.  **Client -> SS (Primary):** `SS_LOCK /docs/file.txt 3\n`
6.  `SS -> Client: 201\n` (Or `504` if locked).
7.  Client sends updates:
    * **Client -> SS (Primary):** `SS_UPDATE 2 hello\n`
8.  Client commits:
    * **Client -> SS (Primary):** `SS_COMMIT\n`
9.  SS (Primary) saves to its disk, releases lock.
10. **SS (Primary) -> Client:** `200\n` (Client is now done).
11. **Replication (Async):**
    * **SS (Primary) -> NM:** `S_META_UPDATE /docs/file.txt 150 780\n`
    * NM receives this update (e.g., new size/timestamp).
    * NM commands the Replica (SS2) to pull the update from the Primary (SS1).
    * **NM -> SS2 (Replica):** `NM_PULL_SYNC /docs/file.txt <SS1_ip> <SS1_port>\n`

#### `DELETE <path>` (File)
1.  **Client -> NM:** `C_REQ_DELETE /docs/file.txt\n`
2.  NM checks if user is owner (`403`).
3.  NM finds all SSs for this file (SS1, SS2).
4.  **NM -> SS1:** `NM_DELETE /docs/file.txt\n`
5.  **NM -> SS2:** `NM_DELETE /docs/file.txt\n` (NM sends to all replicas).
6.  After primary ACKs, NM removes file from its metadata.
7.  **NM -> Client:** `200\n`

#### `DELETE <path>` (Folder)
1.  **Client -> NM:** `C_REQ_DELETE /docs\n`
2.  NM checks if user is owner (`403`).
3.  NM checks its metadata to see if folder `/docs` is empty. If not: `NM -> Client: 405\n`
4.  If empty, NM removes folder from its internal directory tree.
5.  **NM -> Client:** `200\n`

#### `MOVE <source_path> <dest_path>`
1.  **Client -> NM:** `C_MOVE /file.txt /docs/\n` (or `C_MOVE /file.txt /docs/new.txt`)
2.  NM checks ownership of `file.txt` and 'W' access in `/docs`.
3.  NM checks if destination path already exists (`409`).
4.  This is a **pure metadata operation** on the NM. No SSs are contacted.
5.  NM updates its internal map: `file.txt` metadata is now associated with the path `/docs/file.txt`.
6.  **NM -> Client:** `200\n`

### 6.2. Metadata & Access Control

#### `VIEW <path>`
1.  **Client -> NM:** `C_VIEW -al /docs\n` (Path can be `/` or any folder).
2.  NM checks for 'R' access on `/docs`.
3.  NM looks at its directory tree for all items inside `/docs`.
4.  NM filters list based on user (unless `-a` is present).
5.  NM constructs the payload string (see Section 5).
6.  **NM -> Client:** `200 D,/docs/sub,userA;F,/docs/file.txt,userB,69,420;\n`

#### `INFO <path>`
1.  **Client -> NM:** `C_INFO /docs/file.txt\n`
2.  NM checks 'R' access.
3.  NM gathers all metadata (Owner, Timestamps, Size, Full ACL) from its internal state.
4.  NM formats this into a human-readable string (payload format is flexible).
5.  **NM -> Client:** `200 Owner: userB\nSize: 420 bytes\n...\n`

#### `LIST` (Users)
1.  **Client -> NM:** `C_REQ_LIST\n`
2.  NM iterates its list of all *ever-registered* usernames.
3.  **NM -> Client:** `200 userA;userB;kaevi;\n` (Payload is flexible, e.g., newline-separated).

#### `ADDACCESS` / `REMACCESS`
1.  **Client -> NM:** `C_ADD_ACC /file.txt userC W\n` (or `C_REM_ACC /file.txt userC`)
2.  NM checks if requestor is the *owner* of `/file.txt` (`403`).
3.  NM updates its internal ACL for `/file.txt`.
4.  **NM -> Client:** `200\n`

#### `REQACCESS <path> <R/W>` (Bonus)
1.  **ClientA -> NM:** `C_REQ_ACCESS /file.txt R\n`
2.  NM checks if ClientA *already* has access. If yes, `200\n`.
3.  If no, NM adds to a "Pending Requests" list: `reqID1 -> {file, userA, R, PENDING}`.
4.  **NM -> ClientA:** `200 Request submitted.\n`

#### `LISTREQS <path>` (Bonus)
1.  **ClientB (Owner) -> NM:** `C_LIST_REQS /file.txt\n`
2.  NM checks if ClientB is owner (`403`).
3.  NM finds all pending requests for `/file.txt`.
4.  NM formats payload (see Section 5).
5.  **NM -> ClientB:** `200 reqID1,userA,R;reqID2,userD,W;\n`

#### `APPROVEREJ <req_id> <A/D>` (Bonus)
1.  **ClientB (Owner) -> NM:** `C_APPROVE_REQ reqID1 A\n`
2.  NM finds `reqID1` and verifies ClientB is the owner of the associated file.
3.  **If 'A' (Approve):** NM performs the `ADDACCESS` logic internally.
4.  **If 'D' (Deny):** NM does nothing to the ACL.
5.  NM *deletes* the pending request (`reqID1`).
6.  **NM -> ClientB:** `200\n`

### 6.3. Advanced File Operations

#### `STREAM <path>`
* Flow is **identical to `READ`**.
* The *only* difference is the Client-SS command:
    * **Client -> SS:** `SS_GET_STREAM /docs/file.txt\n`
* The SS, upon seeing `SS_GET_STREAM`, will `send()` one word at a time, followed by `usleep(STREAM_DELAY_US)`.

#### `UNDO <path>`
1.  **Client -> NM:** `C_REQ_UNDO /file.txt\n`
2.  NM checks 'W' access (`403`).
3.  NM finds the file's Primary SS (SS1).
4.  **NM -> SS1 (Primary):** `NM_UNDO /file.txt\n`
5.  `SS1 -> NM: 200\n` (SS1 swaps `file.txt.bak` with `file.txt`).
6.  **SS1 -> NM:** `S_META_UPDATE /file.txt 100 500\n` (Tells NM the new size).
7.  **NM -> Client:** `200\n`
8.  **Asynchronously:** NM tells the replica (SS2) to sync this undo.
    * **NM -> SS2 (Replica):** `NM_PULL_SYNC /file.txt <SS1_ip> <SS1_port>\n`

#### `EXEC <path>`
1.  **Client -> NM:** `C_REQ_EXEC /script.sh\n`
2.  NM checks for 'R' access (`403`).
3.  NM finds a live SS (Primary or Replica) for the file (e.g., SS1).
4.  **NM -> SS1:** `NM_GET_FILE /script.sh\n` (NM acts as a client to the SS).
5.  **SS1 -> NM:** `[Raw file bytes streamed]`
6.  NM saves content to a temp file (e.g., `/tmp/exec_userA.sh`).
7.  NM `popen()`s the script.
8.  NM sends `200\n` to Client to signal success (and that output is coming).
9.  **NM -> Client:** `[stdout from popen is streamed]`
10. NM `pclose()`s, deletes temp file, and the response is complete.

#### `CHECKPOINT <path> <tag>` (Bonus)
1.  **Client -> NM:** `C_REQ_CHECKPOINT /file.txt my_tag\n`
2.  NM checks 'W' access (`403`).
3.  NM finds Primary SS (SS1) and Replica SS (SS2).
4.  **NM -> SS1 (Primary):** `NM_CHECKPOINT /file.txt my_tag\n`
5.  `SS1 -> NM: 200\n` (SS1 copies `file.txt` to `file.txt.chk.my_tag`).
6.  **NM -> Client:** `200\n`
7.  **Asynchronously:** NM relays the command to the replica.
    * **NM -> SS2 (Replica):** `NM_CHECKPOINT /file.txt my_tag\n`

#### `REVERT <path> <tag>` (Bonus)
1.  **Client -> NM:** `C_REQ_REVERT /file.txt my_tag\n`
2.  NM checks 'W' access (`403`).
3.  NM finds Primary SS (SS1).
4.  **NM -> SS1 (Primary):** `NM_REVERT /file.txt my_tag\n`
5.  `SS1 -> NM: 200\n` (SS1 creates `.bak`, then copies `.chk.my_tag` over `file.txt`).
6.  **SS1 -> NM:** `S_META_UPDATE /file.txt 80 350\n` (Tells NM the new size).
7.  **NM -> Client:** `200\n`
8.  **Asynchronously:** NM tells replica (SS2) to pull the change.
    * **NM -> SS2 (Replica):** `NM_PULL_SYNC /file.txt <SS1_ip> <SS1_port>\n`

#### `LISTCHECKPOINTS <path>` (Bonus)
1.  **Client -> NM:** `C_REQ_LIST_CHKPTS /file.txt\n`
2.  NM checks 'R' access (`403`).
3.  NM finds *any* live replica (e.g., SS1).
4.  **NM -> Client:** `202 <SS1_ip> <SS1_port>\n`
5.  Client opens new connection to SS1.
6.  **Client -> SS1:** `SS_LIST_CHKPTS /file.txt\n`
7.  SS1 scans its disk for `file.txt.chk.*`, formats payload (see Section 5).
8.  **SS1 -> Client:** `200 tag1,ts1;tag2,ts2;\n`

#### `VIEWCHECKPOINT <path> <tag>` (Bonus)
1.  **Client -> NM:** `C_REQ_VIEW_CHKPT /file.txt my_tag\n`
2.  NM checks 'R' access (`403`).
3.  NM finds *any* live replica (e.g., SS1).
4.  **NM -> Client:** `202 <SS1_ip> <SS1_port>\n`
5.  Client opens new connection to SS1.
6.  **Client -> SS1:** `SS_GET_CHKPT /file.txt my_tag\n`
7.  **SS1 -> Client:** `[Raw bytes of the checkpoint file streamed]`
8.  SS1 closes connection.