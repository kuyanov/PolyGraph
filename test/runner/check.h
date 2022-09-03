#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "config.h"
#include "json.h"
#include "net.h"
#include "operations.h"
#include "run_request.h"
#include "run_response.h"

namespace fs = std::filesystem;

static WebsocketServer server(Config::Get().scheduler_host, Config::Get().scheduler_port);
static SchemaValidator response_validator("run_response.json");

long long Timestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string PrepareContainer() {
    std::string container_name = "test";
    if (!fs::exists(fs::path(SANDBOX_DIR) / container_name)) {
        fs::create_directories(fs::path(SANDBOX_DIR) / container_name);
    }
    return container_name;
}

RunResponse SendTasks(const std::vector<Task> &tasks) {
    RunRequest run_request = {.container = PrepareContainer(), .tasks = tasks};
    auto session = server.Accept();
    session.Write(StringifyJSON(Dump(run_request)));
    RunResponse run_response;
    Load(run_response, response_validator.ParseAndValidate(session.Read()));
    return run_response;
}

void CheckTimeDelta(long long start_time, long long end_time, long long expected_delta) {
    long long error = end_time - start_time - expected_delta;
    ASSERT_TRUE(error >= 0 && error < 100);
}

void CheckAllExitedNormally(const RunResponse &run_response, size_t num_tasks) {
    ASSERT_TRUE(!run_response.has_error);
    ASSERT_EQ(run_response.results.size(), num_tasks);
    for (const auto &result : run_response.results) {
        ASSERT_TRUE(result.exited);
        ASSERT_EQ(result.exit_code, 0);
    }
}
