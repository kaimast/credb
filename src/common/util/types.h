#pragma once

typedef uint32_t remote_party_id;
const remote_party_id INVALID_REMOTE_PARTY = 0;

typedef enum PeerType {
    Unknown,
    Client,
    PeerServer,
    DownstreamServer,
} PeerType;
