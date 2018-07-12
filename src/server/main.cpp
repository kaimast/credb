#include "Server.h"

#include "Disk.h"
#include <boost/program_options.hpp>
#include <glog/logging.h>
#include <iostream>
#include <yael/EventLoop.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __USE_GNU
#define __USE_GNU
#endif

#include <cxxabi.h>
#include <execinfo.h>
#include <csignal>
#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>
#include <ucontext.h>

namespace po = boost::program_options;
using namespace yael;

/* This structure mirrors the one found in /usr/include/asm/ucontext.h */
struct sig_ucontext_t
{
    unsigned long uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    struct sigcontext uc_mcontext;
    sigset_t uc_sigmask;
};

void segv_handler(int sig_num, siginfo_t *info, void *ucontext)
{
    void *array[50];

    auto uc = reinterpret_cast<sig_ucontext_t *>(ucontext);

    /* Get the address at the time the signal was raised */
#if defined(__i386__) // gcc specific
    auto caller_address = (void *)uc->uc_mcontext.eip; // EIP: x86 specific
#elif defined(__x86_64__) // gcc specific
    auto caller_address = (void *)uc->uc_mcontext.rip; // RIP: x86_64 specific
#else
#error Unsupported architecture. // TODO: Add support for other arch.
#endif

    std::cerr << "signal " << sig_num << " (" << strsignal(sig_num) << "), address is "
              << info->si_addr << " from " << caller_address << std::endl
              << std::endl;

    int size = backtrace(array, 50);

    array[1] = caller_address;

    char **messages = backtrace_symbols(array, size);

    // skip first stack frame (points here)
    for(int i = 1; i < size && messages != NULL; ++i)
    {
        char *mangled_name = nullptr;
        char *offset_begin = nullptr, *offset_end = nullptr;

        // find parantheses and +address offset surrounding mangled name
        for(char *p = messages[i]; *p != 0; ++p)
        {
            if(*p == '(')
            {
                mangled_name = p;
            }
            else if(*p == '+')
            {
                offset_begin = p;
            }
            else if(*p == ')')
            {
                offset_end = p;
                break;
            }
        }

        // if the line could be processed, attempt to demangle the symbol
        if(mangled_name && (offset_begin != nullptr) &&
            (offset_end != nullptr) && mangled_name < offset_begin)
        {
            *mangled_name++ = '\0';
            *offset_begin++ = '\0';
            *offset_end++ = '\0';

            int status;
            char *real_name = abi::__cxa_demangle(mangled_name, 0, 0, &status);

            // if demangling is successful, output the demangled function name
            if(status == 0)
            {
                std::cerr << "[bt]: (" << i << ") " << messages[i] << " : " << real_name << "+"
                          << offset_begin << offset_end << std::endl;
            }
            // otherwise, output the mangled function name
            else
            {
                std::cerr << "[bt]: (" << i << ") " << messages[i] << " : " << mangled_name << "+"
                          << offset_begin << offset_end << std::endl;
            }
            free(real_name);
        }
        // otherwise, print the whole line
        else
        {
            std::cerr << "[bt]: (" << i << ") " << messages[i] << std::endl;
        }
    }
    std::cerr << std::endl;

    free(messages);

    exit(EXIT_FAILURE);
}

void stop_handler(int i)
{
    (void)i;
    LOG(INFO) << "Received signal. Stopping...";
    EventLoop::get_instance().stop();
}

void null_handler(int sig) { (void)sig; }

int main(int ac, char *av[])
{
    std::stringstream sstr;
    sstr << "credb.log.";

    signal(SIGPIPE, null_handler);
    signal(SIGSTOP, stop_handler);
    signal(SIGTERM, stop_handler);

    struct sigaction sigact;

    sigact.sa_sigaction = segv_handler;
    sigact.sa_flags = SA_RESTART | SA_SIGINFO;

    if(sigaction(SIGSEGV, &sigact, (struct sigaction *)nullptr) != 0 ||
       sigaction(SIGILL, &sigact, (struct sigaction *)nullptr) != 0)
    {
        fprintf(stderr, "error setting signal handler for %d (%s)\n", SIGSEGV, strsignal(SIGSEGV));

        exit(EXIT_FAILURE);
    }

#ifdef DEBUG
    FLAGS_logbufsecs = 0;
#endif

    google::SetLogDestination(google::GLOG_INFO, sstr.str().c_str());
    google::InitGoogleLogging(av[0]);

    FLAGS_logbufsecs = 0;
    FLAGS_logbuflevel = google::GLOG_INFO;
    FLAGS_alsologtostderr = true;

    po::positional_options_description p;
    p.add("name", 1);

    po::options_description desc("Allowed options");
    desc.add_options()("name", po::value<std::string>(),
                       "the name of this node")("hostname", po::value<std::string>(), "the hostname to listen on")(
    "port", po::value<uint16_t>(),
    "which client port to use")("listen", po::value<uint16_t>()->implicit_value(SERVER_PORT),
                                "should be listen for peers?")("help,h", "produce help message")(
    "connect,c", po::value<std::string>())(
    "upstream", po::value<std::string>(),
    "upstream server address")("dbpath", po::value<std::string>(), "path to data storage. in-memory if not set.");

    po::variables_map vm;
    po::store(po::command_line_parser(ac, av).options(desc).positional(p).run(), vm);
    po::notify(vm);

    std::string hostname = "0.0.0.0";

    if(vm.count("help"))
    {
        std::cout << desc << std::endl;
        return 0;
    }

    if(vm.count("name") == 0)
    {
        std::cout << "No node name specified" << std::endl;
        return -1;
    }

    std::string dbpath;

    if(vm.count("dbpath"))
    {
        dbpath = vm["dbpath"].as<std::string>();
        struct stat info;
        if(stat(dbpath.c_str(), &info) != 0 || !(info.st_mode & S_IFDIR))
        {
            std::cerr << "Directory " << dbpath << " does not exists" << std::endl;
            return -1;
        }
    }

    if(vm.count("hostname") != 0)
    {
        hostname = vm["hostname"].as<std::string>();
    }

    // set this to match TCSnum in Enclave.config.xml
    EventLoop::initialize(50);

    uint16_t port = 0;

    if(vm.count("port") > 0)
    {
        port = vm["port"].as<uint16_t>();
    }

    credb::untrusted::Server db(vm["name"].as<std::string>(), hostname, port, dbpath);

    if(vm.count("listen") > 0)
    {
        db.listen(vm["listen"].as<uint16_t>());
    }

    if(vm.count("connect") > 0)
    {
        db.connect(vm["connect"].as<std::string>());
    }

    if(vm.count("upstream") > 0)
    {
        db.set_upstream(vm["upstream"].as<std::string>());
    }

    auto &event_loop = EventLoop::get_instance();
    event_loop.wait();
    event_loop.destroy();

    google::ShutdownGoogleLogging();
    google::ShutDownCommandLineFlags();
    return 0;
}
