# BeamMP Protocol Documentation
WIP of course, but hope it helps others

## TCP
### Basic message format
All messages are sent as strings (?).
Every packet starts with a 4 byte header, denoting how many bytes the packet data is.
This means that you first must read 4 bytes from your socket, to get the header data, and afterwards read the data ready for reading.

### Communication flow
Upon client connection:
1. Authentication
2. Sync with client?

#### Authentication
1. C->S - Client sends the server a single byte. The BeamMP source checks for 3 letters, but I only saw C
2. C->S - Client sends a packet containing the client version
3. S->C - Server sends the server a regular packet containing only 1 byte: `S` (the ascii character)
4. C->S - Client sends the server its public key
5. S->C - Server sends the client their assigned server ID

#### Syncing resources
1. C->S - Client sends the server a packet with the code `SR`
2. S->C - Server sends the client a packet containing a filelist containing the name and size of files to download, or "-" if it's empty

### Packets
All packets start with a single character, denoted as "Packet Code" or simply "Code" down below.
The data mentioned is merely example data, to show the data format. This includes the code!
Example:
| Code | Dir | Explanation | Data |
| ---- | --- | ----------- | ---- |
| `C` | C->S | Client version | VC2.0 |

#### Uncategorized
| Code | Dir | Explanation | Data |
| ---- | --- | ----------- | ---- |
| `J` | S->C | Show a message in the top left on the clients screen | JWelcome Luuk! |

#### Authentication
| Code | Dir | Explanation | Data |
| ---- | --- | ----------- | ---- |
| `C` | C->S | Client version | VC2.0 |
| `S` | S->C | Confirmation? | None |
| `D` | C->S | Start download phase? Seems unused from testing | None |
| `P` | S->C | Send client their assigned ID | P123 |

#### Syncing resources
| Code | Dir | Explanation | Data |
| ---- | --- | ----------- | ---- |
| `SR` | C->S | Client requests a list containing all mods, containing their name and file size | None |
| `f` | C->S | Client requests file with name | fcool_mod.zip |
| `-` | S->C | Technically not a packet, see [Syncing Resources](https://github.com/Lucky4Luuk/beammp_rust_server/blob/master/BEAMMP_PROTOCOL_DOCS.md#syncing-resources), step 2. | * |

#### Syncing client state
| Code | Dir | Explanation | Data |
| ---- | --- | ----------- | ---- |
| `H` | C->S | Client requests a full sync with the server state | None |

### Vehicle packets
| Code | Dir | Explanation | Data |
| ---- | --- | ----------- | ---- |
| `Os` | C->S | Client informs server they are spawning a car | Os:C_ID:{car_json} |
| `Os` | S->C | Server sends this packet to all clients, informing them a client has spawned a car | Os:C_ROLES:C_NAME:C_ID-CAR_ID:CAR_JSON |
| `Or` | C->S | Client informs server they are respawning their car | Or:C_ID-CAR_ID:{respawn_json} |
| `Or` | S->C | Server informs all other clients that a client is respawning their car | Or:C_ID-CAR_ID:{respawn_json} |
| `Od` | C->S | Client informs server they are deleting their car | Od:C_ID-CAR_ID |
| `Od` | S->C | Server informs all other clients that a client is deleting their car | Od:C_ID-CAR_ID |
| `Oc` | C->S | Client informs server they are editing their car | Oc:?... |
| `Oc` | S->C | Server informs all other clients that a client has edited their car | Oc:?... |

## UDP
### Packets
#### Uncategorized
| Code | Dir | Explanation | Data |
| ---- | --- | ----------- | ---- |
| `p` | C->S | Ping! | None |
| `p` | S->C | Pong! | None |

#### Vehicle related
| Code | Dir | Explanation | Data |
| ---- | --- | ----------- | ---- |
| `Zp` | C->S | Positional packet | Zp:C_ID-CAR_ID:{transform_json} |
