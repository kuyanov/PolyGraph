#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "config.h"
#include "json.h"
#include "net.h"
#include "result.h"
#include "serialize.h"
#include "task.h"
#include "uuid.h"

namespace fs = std::filesystem;

std::string test_container;

static WebsocketServer server(Config::Get().scheduler_host, Config::Get().scheduler_port);
static SchemaValidator response_validator("run_response.json");

long long Timestamp() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

void InitContainer() {
    test_container = GenerateUuid();
    fs::path container_path = fs::path(SANDBOX_DIR) / test_container;
    fs::create_directories(container_path);
    fs::permissions(container_path, fs::perms::all, fs::perm_options::add);
}

void AddFile(const std::string &filename, const std::string &content, int other_perms = 7) {
    fs::path filepath = fs::path(SANDBOX_DIR) / test_container / filename;
    std::ofstream(filepath) << content;
    fs::permissions(filepath, fs::perms::others_all, fs::perm_options::remove);
    fs::permissions(filepath, static_cast<fs::perms>(other_perms), fs::perm_options::add);
}

std::string ReadFile(const std::string &filename) {
    fs::path filepath = fs::path(SANDBOX_DIR) / test_container / filename;
    std::stringstream ss;
    ss << std::ifstream(filepath).rdbuf();
    return ss.str();
}

RunResponse SendTask(const Task &task) {
    RunRequest run_request = {.container = test_container, .task = task};
    auto session = server.Accept();
    session.Write(StringifyJSON(Serialize(run_request)));
    RunResponse run_response;
    Deserialize(run_response, response_validator.ParseAndValidate(session.Read()));
    return run_response;
}

void CheckDuration(long long duration, long long expected) {
    ASSERT_GE(duration, expected);
    ASSERT_LT(duration, expected + 100);
}

void CheckExitedNormally(const RunResponse &run_response) {
    ASSERT_TRUE(run_response.result.has_value());
    ASSERT_TRUE(run_response.result->exited);
    ASSERT_EQ(run_response.result->exit_code, 0);
}
