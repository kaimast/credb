#pragma once

#include <stdint.h>
#include <type_traits>

enum class OperationType : uint8_t
{
    GetObject,
    GetObjectWithWitness, // TODO: consider making generating witness as a global options for all
                          // ops
    GetObjectHistory,
    HasObject,
    CheckObject,
    PutObject,
    PutObjectWithoutKey,
    AddToObject,
    CountObjects,
    FindObjects,
    ListPeers,
    DiffVersions,
    RemoveObject,
    CreateIndex,
    DropIndex,
    Clear,
    CreateWitness,
    ExecuteCode,
    CallProgram,
    ReadFromUpstreamDisk,
    TellPeerType,
    CommitTransaction,
    Peer,
    SetTrigger,
    UnsetTrigger,

    // debug purpose
    NOP,
    DumpEverything,
    LoadEverything,
};

// typedef std::underlying_type<OperationType>::type op_data_t;
typedef uint8_t op_data_t;

typedef uint32_t taskid_t;
typedef uint32_t operation_id_t;
