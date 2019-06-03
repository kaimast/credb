#pragma once
#include <string>

typedef struct _sgx_errlist_t
{
    sgx_status_t err;
    const char *msg;
} sgx_errlist_t;

/* Error code returned by sgx_create_enclave */
static sgx_errlist_t sgx_errlist[] =
{ { SGX_ERROR_UNEXPECTED, "Unexpected error occurred." },
  { SGX_ERROR_BUSY, "Busy" },
  { SGX_ERROR_INVALID_PARAMETER, "Invalid parameter." },
  { SGX_ERROR_OUT_OF_MEMORY, "Out of memory." },
  { SGX_ERROR_OUT_OF_TCS, "All TCSs in use" },
  { SGX_ERROR_ENCLAVE_LOST, "Power transition occurred." },
  { SGX_ERROR_INVALID_ENCLAVE, "Invalid enclave image." },
  { SGX_ERROR_INVALID_ENCLAVE_ID, "Invalid enclave identification." },
  { SGX_ERROR_INVALID_SIGNATURE, "Invalid enclave signature." },
  { SGX_ERROR_OUT_OF_EPC, "Out of EPC memory." },
  { SGX_ERROR_NO_DEVICE, "Invalid SGX device." },
  { SGX_ERROR_MEMORY_MAP_CONFLICT, "Memory map conflicted." },
  { SGX_ERROR_INVALID_METADATA, "Invalid enclave metadata." },
  { SGX_ERROR_FEATURE_NOT_SUPPORTED, "Feature not supported." },
  { SGX_ERROR_INVALID_STATE, "Invalid state" },
  { SGX_ERROR_DEVICE_BUSY, "SGX device was busy." },
  { SGX_ERROR_INVALID_VERSION, "Enclave version was invalid." },
  { SGX_ERROR_INVALID_ATTRIBUTE, "Enclave was not authorized." },
  { SGX_ERROR_ENCLAVE_FILE_ACCESS, "Can't open enclave file." },
  { SGX_ERROR_ENCLAVE_CRASHED, "Enclave crashed" },
  { SGX_ERROR_OCALL_NOT_ALLOWED, "Ocall is not allowed (at this time)" },
  { SGX_ERROR_ECALL_NOT_ALLOWED, "Ecall is not allowed (at this time)" },
  { SGX_ERROR_SERVICE_INVALID_PRIVILEGE, "Enclave has no privilege to get launch token" },
  { SGX_ERROR_INVALID_CPUSVN, "Invalid CPUSVN" },
  { SGX_ERROR_INVALID_ISVSVN, "Invalid ISVSVN" },
  { SGX_ERROR_INVALID_KEYNAME, "Invalid keyname" },
  { SGX_ERROR_SERVICE_UNAVAILABLE, "AESM service is not available" } };

inline std::string to_string(sgx_status_t ret)
{
    size_t idx = 0;
    size_t ttl = sizeof sgx_errlist / sizeof sgx_errlist[0];

    std::string msg = "UNKNOWN";

    for(idx = 0; idx < ttl; idx++)
    {
        if(ret == sgx_errlist[idx].err)
        {
            msg = sgx_errlist[idx].msg;
            break;
        }
    }

    msg += " (errno " + std::to_string(static_cast<int>(ret)) + ")";
    return msg;
}
