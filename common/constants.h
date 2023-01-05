#pragma once

#include <cstdlib>
#include <string>

namespace http_response {

const std::string kBadRequest = "400 Bad Request";
const std::string kNotFound = "404 Not Found";
const std::string kRequestEntityTooLarge = "413 Request Entity Too Large";

}  // namespace http_response

namespace errors {

const std::string kParseErrorPrefix = "Could not parse json: ";
const std::string kValidationErrorPrefix = "Validation error: ";
const std::string kRuntimeErrorPrefix = "Runtime error: ";
const std::string kAPIErrorPrefix = "API error: ";

const std::string kDuplicatedLocation = "duplicated location";
const std::string kInvalidConnection = "invalid connection";
const std::string kLoopsNotSupported = "loops are not supported";
const std::string kUndefinedCommand = "undefined command";
const std::string kNotImplemented = "not implemented";
const std::string kAlreadyRunning = "workflow is already running";

}  // namespace errors

namespace signals {

const std::string kRun = "run";
const std::string kStop = "stop";

}  // namespace signals

namespace states {

const std::string kRunning = "running";
const std::string kComplete = "complete";

}  // namespace states

namespace paths {

const std::string kResourcesPath =
    getenv("RESOURCES_PATH") ? getenv("RESOURCES_PATH") : DEFAULT_RESOURCES_PATH;
const std::string kDataPath = getenv("DATA_PATH") ? getenv("DATA_PATH") : DEFAULT_DATA_PATH;
const std::string kContainersSubdir = "containers";
const std::string kUserSubdir = "user";
const std::string kTestSubdir = "test";

}  // namespace paths
