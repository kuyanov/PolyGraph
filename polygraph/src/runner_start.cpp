#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

#include "common.h"
#include "runner_start.h"

namespace fs = std::filesystem;

void RunnerStart(const RunnerStartOptions &options) {
    RequireRoot();
    CreateDirs();
    int runner_id = 0;
    for (int iter = 0; iter < options.num; ++iter) {
        fs::path pid_path;
        while (fs::exists(pid_path = fs::path(GetRunDir()) /
                                     ("runner" + std::to_string(runner_id) + ".pid"))) {
            ++runner_id;
        }
        std::ofstream pid_file(pid_path.string());
        if (fork() == 0) {
            Daemonize();
            pid_file << getpid() << std::endl;
            setenv("POLYGRAPH_HOST", options.host.c_str(), 1);
            setenv("POLYGRAPH_PORT", std::to_string(options.port).c_str(), 1);
            setenv("POLYGRAPH_RUNNER_ID", std::to_string(runner_id).c_str(), 1);
            setenv("POLYGRAPH_RUNNER_PARTITION", options.partition.c_str(), 1);
            setenv("POLYGRAPH_RUNNER_RECONNECT_INTERVAL_MS",
                   std::to_string(options.reconnect_interval_ms).c_str(), 1);
            fs::path exec_path = fs::path(GetExecDir()) / "polygraph-runner";
            execl(exec_path.c_str(), "polygraph-runner", nullptr);
            fs::remove(pid_path);
            perror("Failed to start runner");
            exit(EXIT_FAILURE);
        }
    }
}
