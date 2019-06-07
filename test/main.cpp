#include <gtest/gtest.h>
#include <glog/logging.h>

bool g_unsafe_mode = false;

int main(int argc, char **argv)
{
    google::SetLogDestination(google::GLOG_INFO, "credb-test");
    google::InitGoogleLogging(argv[0]);

    FLAGS_logbufsecs = 0;
    FLAGS_logbuflevel = google::GLOG_INFO;
    FLAGS_alsologtostderr = 1;

    testing::InitGoogleTest(&argc, argv);

    if(argc > 1 && strcmp(argv[1], "--unsafe_mode") == 0)
    {
        LOG(INFO) << "Running tests in unsafe mode";
        g_unsafe_mode = true;
    }

    auto res = RUN_ALL_TESTS();

    google::ShutdownGoogleLogging();
    google::ShutDownCommandLineFlags();
    return res;
}
