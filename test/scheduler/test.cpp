#include <climits>
#include <string>
#include <unordered_set>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "check.h"

using ::testing::IsEmpty;
using ::testing::MatchesRegex;
using ::testing::StartsWith;
using ::testing::StrEq;

const int kRunnerDelay = 500;
const Meta kWorkflowMeta = {"sample workflow", "all", INT_MAX};

TEST(ParseError, Trivial) {
    EXPECT_THAT(Submit(""), StartsWith(PARSE_ERROR_PREFIX));
    EXPECT_THAT(Submit("{"), StartsWith(PARSE_ERROR_PREFIX));
    EXPECT_THAT(Submit("}"), StartsWith(PARSE_ERROR_PREFIX));
    EXPECT_THAT(Submit("{:}"), StartsWith(PARSE_ERROR_PREFIX));
    EXPECT_THAT(Submit("{,}"), StartsWith(PARSE_ERROR_PREFIX));
    EXPECT_THAT(Submit("{a:b}"), StartsWith(PARSE_ERROR_PREFIX));
    EXPECT_THAT(Submit("\"a\":\"b\""), StartsWith(PARSE_ERROR_PREFIX));
    EXPECT_THAT(Submit("{[]:[]}"), StartsWith(PARSE_ERROR_PREFIX));
    EXPECT_THAT(Submit("{\"a\":\"b}"), StartsWith(PARSE_ERROR_PREFIX));
    EXPECT_THAT(Submit("{\"a\":2,}"), StartsWith(PARSE_ERROR_PREFIX));
}

TEST(ValidationError, Trivial) {
    EXPECT_THAT(Submit("{}"), StartsWith(VALIDATION_ERROR_PREFIX));
    EXPECT_THAT(Submit("[]"), StartsWith(VALIDATION_ERROR_PREFIX));
}

TEST(ValidationError, MetaMissing) {
    EXPECT_THAT(Submit("{\"blocks\":[],\"connections\":[]}"), StartsWith(VALIDATION_ERROR_PREFIX));
}

TEST(ValidationError, InvalidType) {
    EXPECT_THAT(
        Submit(
            "{\"blocks\":[],\"connections\":0,\"meta\":{\"name\":\"\",\"partition\":\"all\",\"max-"
            "runners\":1}}"),
        StartsWith(VALIDATION_ERROR_PREFIX));
    EXPECT_THAT(
        Submit(
            "{\"blocks\":[],\"connections\":{},\"meta\":{\"name\":\"\",\"partition\":\"all\",\"max-"
            "runners\":1}}"),
        StartsWith(VALIDATION_ERROR_PREFIX));
}

TEST(ValidationError, LocationRegex) {
    for (const auto &location : {"", ".", "../a", "/home", ".a", "a.", "a..b", "a b"}) {
        EXPECT_THAT(SubmitWorkflow({{{.binds = {{location, location}}}}, {}, kWorkflowMeta}),
                    StartsWith(VALIDATION_ERROR_PREFIX));
    }
    for (const auto &location : {"a", "a.in", "a.in.txt", "A.mp4", "a/b/c.D.E"}) {
        EXPECT_THAT(SubmitWorkflow({{{.binds = {{location, location}}}}, {}, kWorkflowMeta}),
                    MatchesRegex(UUID_REGEX));
    }
}

TEST(ValidationError, InvalidConnection) {
    EXPECT_THAT(SubmitWorkflow({{{.outputs = {"a"}}}, {{0, 0, 1, 0}}, kWorkflowMeta}),
                StrEq(VALIDATION_ERROR_PREFIX INVALID_CONNECTION_ERROR));
    EXPECT_THAT(SubmitWorkflow({{{.inputs = {"a"}}}, {{1, 0, 0, 0}}, kWorkflowMeta}),
                StrEq(VALIDATION_ERROR_PREFIX INVALID_CONNECTION_ERROR));
    EXPECT_THAT(
        SubmitWorkflow({{{.outputs = {"a"}}, {.inputs = {"a"}}}, {{0, 1, 1, 0}}, kWorkflowMeta}),
        StrEq(VALIDATION_ERROR_PREFIX INVALID_CONNECTION_ERROR));
    EXPECT_THAT(
        SubmitWorkflow({{{.outputs = {"a"}}, {.inputs = {"a"}}}, {{0, 0, 1, 1}}, kWorkflowMeta}),
        StrEq(VALIDATION_ERROR_PREFIX INVALID_CONNECTION_ERROR));
}

TEST(ValidationError, DuplicatedLocation) {
    EXPECT_THAT(SubmitWorkflow({{{.inputs = {"a"}, .binds = {{"a", "a"}}}}, {}, kWorkflowMeta}),
                StrEq(VALIDATION_ERROR_PREFIX DUPLICATED_LOCATION_ERROR));
    EXPECT_THAT(SubmitWorkflow({{{.outputs = {"a"}, .binds = {{"a", "a"}}}}, {}, kWorkflowMeta}),
                StrEq(VALIDATION_ERROR_PREFIX DUPLICATED_LOCATION_ERROR));
}

TEST(ValidationError, Loop) {
    EXPECT_THAT(
        SubmitWorkflow({{{.inputs = {"a"}, .outputs = {"b"}}}, {{0, 0, 0, 0}}, kWorkflowMeta}),
        StrEq(VALIDATION_ERROR_PREFIX LOOPS_NOT_SUPPORTED_ERROR));
}

TEST(Submit, WorkflowIdUnique) {
    std::string body = StringifyJSON(Serialize(Workflow{{}, {}, kWorkflowMeta}));
    std::unordered_set<std::string> ids;
    for (int i = 0; i < 1000; i++) {
        auto id = Submit(body);
        EXPECT_THAT(id, MatchesRegex(UUID_REGEX));
        ids.insert(id);
    }
    ASSERT_EQ(ids.size(), 1000);
}

TEST(Submit, MaxPayloadLength) {
    std::string body;
    body.resize(Config::Get().scheduler_max_payload_length, '.');
    EXPECT_THAT(Submit(body), StartsWith(PARSE_ERROR_PREFIX));
    body.push_back('.');
    EXPECT_THAT(Submit(body), IsEmpty());
}

TEST(Execution, Empty) {
    Workflow workflow = {{}, {}, kWorkflowMeta};
    CheckExecution(workflow, 1, 1, 0, kRunnerDelay, 0);
}

TEST(Execution, SingleBlock) {
    Workflow workflow = {{{}}, {}, kWorkflowMeta};
    CheckExecution(workflow, 5, 1, 1, kRunnerDelay, kRunnerDelay);
    CheckExecution(workflow, 5, 10, 1, kRunnerDelay, kRunnerDelay);
}

TEST(Execution, Bamboo) {
    Workflow workflow = {
        {{.outputs = {"a"}}, {.inputs = {"a"}, .outputs = {"b"}}, {.inputs = {"b"}}},
        {{0, 0, 1, 0}, {1, 0, 2, 0}},
        kWorkflowMeta};
    CheckExecution(workflow, 5, 1, 3, kRunnerDelay, 3 * kRunnerDelay);
    CheckExecution(workflow, 5, 10, 3, kRunnerDelay, 3 * kRunnerDelay);
}

TEST(Execution, Parallel) {
    Workflow workflow = {{{}, {}, {}}, {}, kWorkflowMeta};
    CheckExecution(workflow, 5, 1, 3, kRunnerDelay, 3 * kRunnerDelay);
    CheckExecution(workflow, 5, 3, 3, kRunnerDelay, kRunnerDelay);
}

TEST(Execution, MaxRunners) {
    Workflow workflow = {{{.outputs = {"a"}},
                          {.inputs = {"a"}, .outputs = {"b"}},
                          {.inputs = {"a"}, .outputs = {"b"}},
                          {.inputs = {"a"}, .outputs = {"b"}},
                          {.inputs = {"a"}, .outputs = {"b"}},
                          {.inputs = {"b1", "b2", "b3", "b4"}}},
                         {{0, 0, 1, 0},
                          {0, 0, 2, 0},
                          {0, 0, 3, 0},
                          {0, 0, 4, 0},
                          {1, 0, 5, 0},
                          {2, 0, 5, 1},
                          {3, 0, 5, 2},
                          {4, 0, 5, 3}},
                         kWorkflowMeta};
    CheckExecution(workflow, 3, 1, 6, kRunnerDelay, 6 * kRunnerDelay);
    CheckExecution(workflow, 3, 2, 6, kRunnerDelay, 4 * kRunnerDelay);
    CheckExecution(workflow, 3, 3, 6, kRunnerDelay, 4 * kRunnerDelay);
    CheckExecution(workflow, 3, 4, 6, kRunnerDelay, 3 * kRunnerDelay);
    CheckExecution(workflow, 3, 6, 6, kRunnerDelay, 3 * kRunnerDelay);
    workflow.meta.max_runners = 4;
    CheckExecution(workflow, 3, 4, 6, kRunnerDelay, 3 * kRunnerDelay);
    workflow.meta.max_runners = 3;
    CheckExecution(workflow, 3, 4, 6, kRunnerDelay, 4 * kRunnerDelay);
    workflow.meta.max_runners = 2;
    CheckExecution(workflow, 3, 4, 6, kRunnerDelay, 4 * kRunnerDelay);
    workflow.meta.max_runners = 1;
    CheckExecution(workflow, 3, 4, 6, kRunnerDelay, 6 * kRunnerDelay);
}

TEST(Execution, FiniteCycle) {
    Workflow workflow = {{{.outputs = {"a"}},
                          {.inputs = {"a"}, .outputs = {"b"}},
                          {.inputs = {"b"}, .outputs = {"c"}},
                          {.inputs = {"c"}, .outputs = {"d"}},
                          {.inputs = {"d"}, .outputs = {"e"}},
                          {.inputs = {"e1", "e2"}, .outputs = {"f"}},
                          {.inputs = {"f"}, .outputs = {"e"}}},
                         {{0, 0, 1, 0},
                          {1, 0, 2, 0},
                          {2, 0, 3, 0},
                          {3, 0, 4, 0},
                          {0, 0, 5, 0},
                          {0, 0, 5, 1},
                          {2, 0, 5, 0},
                          {4, 0, 5, 0},
                          {5, 0, 6, 0},
                          {6, 0, 5, 1}},
                         kWorkflowMeta};
    CheckExecution(workflow, 3, 2, 11, kRunnerDelay, 7 * kRunnerDelay);
}

TEST(Execution, FailedBlocks) {
    Workflow workflow = {{{.outputs = {"a"}},
                          {.inputs = {"a"}},
                          {.outputs = {"b"}},
                          {.inputs = {"b"}, .outputs = {"c"}},
                          {.inputs = {"c"}}},
                         {{0, 0, 1, 0}, {2, 0, 3, 0}, {3, 0, 4, 0}},
                         kWorkflowMeta};
    CheckExecution(workflow, 3, 2, 3, kRunnerDelay, 2 * kRunnerDelay, {0, 3});
}

TEST(Execution, Stress) {
    std::vector<Block> blocks(100);
    Workflow workflow = {blocks, {}, kWorkflowMeta};
    for (int i = 0; i < 100; i++) {
        CheckExecution(workflow, 4, 4, 100, 0, -1);
    }
}
