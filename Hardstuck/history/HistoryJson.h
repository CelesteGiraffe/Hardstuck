// HistoryJson.h
#pragma once
#include "HistoryTypes.h"

#include <string>
#include <vector>
#include <map>
#include <optional>

// A lightweight JSON representation and parser for the history endpoint.
namespace HistoryJson
{
    enum class Type
    {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    struct Value
    {
        Type type = Type::Null;
        std::string stringValue;
        double numberValue = 0.0;
        bool boolValue = false;
        std::vector<Value> arrayValue;
        std::map<std::string, Value> objectValue;
    };

    class Parser
    {
    public:
        explicit Parser(const std::string& data);

        bool Parse(Value& output, std::string& error);

    private:
        bool ParseValue(Value& output, std::string& error);
        bool ParseObject(Value& output, std::string& error);
        bool ParseArray(Value& output, std::string& error);
        bool ParseString(std::string& output, std::string& error);
        bool ParseNumber(Value& output, std::string& error);
        bool ParseLiteral(const char* literal, Value& output, Type type, std::string& error);
        void SkipWhitespace();
        char Peek() const;
        bool Consume(char expected);

        const std::string& data_;
        size_t pos_;
    };

    const Value* GetMember(const Value& object, const std::string& key);
    std::optional<std::string> AsString(const Value* value);
    std::optional<int> AsInt(const Value* value);
    std::vector<std::string> AsStringList(const Value* value);

    bool ParseHistoryResponse(const std::string& payload, HistorySnapshot& snapshot, std::string& error);
}
