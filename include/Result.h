#pragma once

#include <ArduinoJson.h>

struct Result
{
    JsonDocument doc;
    std::string errorString;
    bool skip;
    Result() {
        skip = false;
        errorString = std::string();
    }
};
