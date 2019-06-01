/// (c) 2018 Cornell University
/// This file is part of the CreDB Project. See LICENSE for more information.

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
    try {
        po::store(po::command_line_parser(ac, av).options(desc).positional(p).run(), vm);
    } catch(const std::exception &e) {
        std::cerr << "Failed to parse command line arguments: " << e.what() << std::endl;
        return -1;
    }

    try {
        po::notify(vm);
    } catch(const std::exception &e) {
        std::cerr << "Invalid command line arguments: " << e.what() << std::endl;
        return -1;
    }

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
