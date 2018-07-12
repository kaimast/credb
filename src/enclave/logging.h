#pragma once

#ifdef FAKE_ENCLAVE
#include <glog/logging.h>
#else
#include "Enclave_t.h"
#endif

#include <sgx_error.h>

#include "util/defines.h"
#include "util/error.h"

#include <assert.h>
#include <stdexcept>
#include <stdio.h>
#include <string>

#ifdef FAKE_ENCLAVE

#define log_fatal(msg)    \
    LOG(FATAL) << msg;

#define log_error(msg)    \
    LOG(ERROR) << msg;

#define log_warning(msg)  \
    LOG(WARNING) << msg;

#define log_info(msg)  \
    LOG(INFO) << msg;

#define log_debug(msg)     \
    DLOG(INFO) << msg;

#else

#if defined(DEBUG) && not defined(IS_TEST)
#define log_debug(msg)                          \
    ({                                          \
        std::string _s = "DEBUG [";             \
        _s += __FILE__;                         \
        _s += ":" + to_string(__LINE__) + "] "; \
        _s += msg;                              \
        print_info(_s.c_str());                 \
    })
#else
#define log_debug(msg) \
    while(false)       \
    {                  \
    }
#endif

#define log_warning(msg)                        \
    ({                                          \
        std::string _s = "WARNING [";           \
        _s += __FILE__;                         \
        _s += ":" + to_string(__LINE__) + "] "; \
        _s += msg;                              \
        print_info(_s.c_str());                 \
    })

#define log_error(msg)                          \
    ({                                          \
        std::string _s = "ERROR [";             \
        _s += __FILE__;                         \
        _s += ":" + to_string(__LINE__) + "] "; \
        _s += msg;                              \
        print_error(_s.c_str());                \
    })

#define log_fatal(msg)                          \
    ({                                          \
        std::string _s = "ERROR [";             \
        _s += __FILE__;                         \
        _s += ":" + to_string(__LINE__) + "] "; \
        _s += msg;                              \
        print_error(_s.c_str());                \
    })


#define log_info(msg)                           \
    ({                                          \
        std::string _s = "INFO [";              \
        _s += __FILE__;                         \
        _s += ":" + to_string(__LINE__) + "] "; \
        _s += msg;                              \
        print_info(_s.c_str());                 \
    })

#endif
