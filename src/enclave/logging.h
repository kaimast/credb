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
#include <iostream>

inline void print_info(const char *str) { std::cout << str << std::endl; }

inline void print_error(const char *str) { std::cerr << str << std::endl; }
#endif

#if defined(DEBUG) && not defined(TEST)
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

#define log_info(msg)                           \
    ({                                          \
        std::string _s = "INFO [";              \
        _s += __FILE__;                         \
        _s += ":" + to_string(__LINE__) + "] "; \
        _s += msg;                              \
        print_info(_s.c_str());                 \
    })
