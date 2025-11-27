// HistoryJson.cpp
#include "pch.h"
#include "history/HistoryJson.h"

#include <cctype>
#include <cmath>
#include <cstring>
#include <stdexcept>

using namespace HistoryJson;

Parser::Parser(const std::string& data)
    : data_(data)
    , pos_(0)
{
}

bool Parser::Parse(Value& output, std::string& error)
{
    SkipWhitespace();
    if (!ParseValue(output, error))
    {
        return false;
    }
    SkipWhitespace();
    if (pos_ != data_.size())
    {
        error = "Unexpected characters after JSON payload";
        return false;
    }
    return true;
}

bool Parser::ParseValue(Value& output, std::string& error)
{
    SkipWhitespace();
    char c = Peek();
    if (c == '"')
    {
        output.type = Type::String;
        return ParseString(output.stringValue, error);
    }
    if (c == '{')
    {
        return ParseObject(output, error);
    }
    if (c == '[')
    {
        return ParseArray(output, error);
    }
    if (c == 't')
    {
        return ParseLiteral("true", output, Type::Bool, error);
    }
    if (c == 'f')
    {
        return ParseLiteral("false", output, Type::Bool, error);
    }
    if (c == 'n')
    {
        return ParseLiteral("null", output, Type::Null, error);
    }
    if (c == '-' || (c >= '0' && c <= '9'))
    {
        return ParseNumber(output, error);
    }
    error = "Unexpected token in JSON";
    return false;
}

bool Parser::ParseObject(Value& output, std::string& error)
{
    output.type = Type::Object;
    if (!Consume('{'))
    {
        error = "Expected '{'";
        return false;
    }

    SkipWhitespace();
    if (Peek() == '}')
    {
        Consume('}');
        return true;
    }

    while (true)
    {
        std::string key;
        if (!ParseString(key, error))
        {
            return false;
        }

        SkipWhitespace();
        if (!Consume(':'))
        {
            error = "Expected ':' after key";
            return false;
        }

        SkipWhitespace();
        Value member;
        if (!ParseValue(member, error))
        {
            return false;
        }

        output.objectValue.emplace(std::move(key), std::move(member));

        SkipWhitespace();
        if (Consume('}'))
        {
            break;
        }
        if (!Consume(','))
        {
            error = "Expected ',' between object entries";
            return false;
        }
        SkipWhitespace();
    }

    return true;
}

bool Parser::ParseArray(Value& output, std::string& error)
{
    output.type = Type::Array;
    if (!Consume('['))
    {
        error = "Expected '['";
        return false;
    }

    SkipWhitespace();
    if (Peek() == ']')
    {
        Consume(']');
        return true;
    }

    while (true)
    {
        Value element;
        if (!ParseValue(element, error))
        {
            return false;
        }
        output.arrayValue.emplace_back(std::move(element));

        SkipWhitespace();
        if (Consume(']'))
        {
            break;
        }
        if (!Consume(','))
        {
            error = "Expected ',' between array entries";
            return false;
        }
        SkipWhitespace();
    }

    return true;
}

bool Parser::ParseString(std::string& output, std::string& error)
{
    if (!Consume('"'))
    {
        error = "Expected '\"'";
        return false;
    }

    while (pos_ < data_.size())
    {
        char c = data_[pos_++];
        if (c == '"')
        {
            return true;
        }
        if (c == '\\')
        {
            if (pos_ >= data_.size())
            {
                error = "Invalid escape sequence";
                return false;
            }
            char escaped = data_[pos_++];
            switch (escaped)
            {
            case '"':  output.push_back('"');  break;
            case '\\': output.push_back('\\'); break;
            case '/':  output.push_back('/');  break;
            case 'b':  output.push_back('\b'); break;
            case 'f':  output.push_back('\f'); break;
            case 'n':  output.push_back('\n'); break;
            case 'r':  output.push_back('\r'); break;
            case 't':  output.push_back('\t'); break;
            default:   output.push_back(escaped); break;
            }
            continue;
        }
        output.push_back(c);
    }

    error = "Unterminated string";
    return false;
}

bool Parser::ParseNumber(Value& output, std::string& error)
{
    size_t start = pos_;
    if (Peek() == '-')
    {
        ++pos_;
    }

    if (Peek() == '0')
    {
        ++pos_;
    }
    else if (std::isdigit(static_cast<unsigned char>(Peek())))
    {
        while (std::isdigit(static_cast<unsigned char>(Peek())))
        {
            ++pos_;
        }
    }
    else
    {
        error = "Invalid number";
        return false;
    }

    if (Peek() == '.')
    {
        ++pos_;
        if (!std::isdigit(static_cast<unsigned char>(Peek())))
        {
            error = "Invalid number";
            return false;
        }
        while (std::isdigit(static_cast<unsigned char>(Peek())))
        {
            ++pos_;
        }
    }

    if (Peek() == 'e' || Peek() == 'E')
    {
        ++pos_;
        if (Peek() == '+' || Peek() == '-')
        {
            ++pos_;
        }
        if (!std::isdigit(static_cast<unsigned char>(Peek())))
        {
            error = "Invalid number";
            return false;
        }
        while (std::isdigit(static_cast<unsigned char>(Peek())))
        {
            ++pos_;
        }
    }

    try
    {
        output.numberValue = std::stod(data_.substr(start, pos_ - start));
    }
    catch (const std::exception&)
    {
        error = "Invalid number";
        return false;
    }

    output.type = Type::Number;
    return true;
}

bool Parser::ParseLiteral(const char* literal, Value& output, Type type, std::string& error)
{
    size_t length = std::strlen(literal);
    if (data_.compare(pos_, length, literal) != 0)
    {
        error = "Unexpected literal";
        return false;
    }
    pos_ += length;
    output.type = type;
    if (type == Type::Bool)
    {
        output.boolValue = (literal[0] == 't');
    }
    return true;
}

void Parser::SkipWhitespace()
{
    while (pos_ < data_.size() && std::isspace(static_cast<unsigned char>(data_[pos_])))
    {
        ++pos_;
    }
}

char Parser::Peek() const
{
    if (pos_ >= data_.size())
    {
        return '\0';
    }
    return data_[pos_];
}

bool Parser::Consume(char expected)
{
    if (Peek() != expected)
    {
        return false;
    }
    ++pos_;
    return true;
}

const Value* HistoryJson::GetMember(const Value& object, const std::string& key)
{
    if (object.type != Type::Object)
    {
        return nullptr;
    }
    auto it = object.objectValue.find(key);
    if (it == object.objectValue.end())
    {
        return nullptr;
    }
    return &it->second;
}

std::optional<std::string> HistoryJson::AsString(const Value* value)
{
    if (!value || value->type != Type::String)
    {
        return std::nullopt;
    }
    return value->stringValue;
}

std::optional<int> HistoryJson::AsInt(const Value* value)
{
    if (!value)
    {
        return std::nullopt;
    }
    if (value->type == Type::Number)
    {
        return static_cast<int>(std::round(value->numberValue));
    }
    if (value->type == Type::String)
    {
        try
        {
            size_t idx = 0;
            int parsed = std::stoi(value->stringValue, &idx);
            if (idx == value->stringValue.size())
            {
                return parsed;
            }
        }
        catch (...) {}
    }
    return std::nullopt;
}

std::vector<std::string> HistoryJson::AsStringList(const Value* value)
{
    std::vector<std::string> result;
    if (!value || value->type != Type::Array)
    {
        return result;
    }
    for (const auto& element : value->arrayValue)
    {
        if (element.type == Type::String)
        {
            result.push_back(element.stringValue);
        }
    }
    return result;
}

namespace
{
    void AssignStringMember(const Value& object, const char* key, std::string& target)
    {
        if (const auto* member = GetMember(object, key))
        {
            if (const auto value = AsString(member))
            {
                target = *value;
            }
        }
    }

    void AssignIntMember(const Value& object, const char* key, int& target)
    {
        if (const auto value = AsInt(GetMember(object, key)))
        {
            target = *value;
        }
    }

    void PopulateMmrHistorySection(const Value& root, HistorySnapshot& snapshot)
    {
        const Value* mmrValue = GetMember(root, "mmrHistory");
        if (!mmrValue || mmrValue->type != Type::Array)
        {
            return;
        }

        for (const auto& record : mmrValue->arrayValue)
        {
            if (record.type != Type::Object)
            {
                continue;
            }

            MmrHistoryEntry entry;
            entry.id = AsString(GetMember(record, "id")).value_or(std::string());
            entry.timestamp = AsString(GetMember(record, "timestamp")).value_or(std::string());
            entry.playlist = AsString(GetMember(record, "playlist")).value_or(std::string());
            entry.mmr = AsInt(GetMember(record, "mmr")).value_or(0);
            entry.gamesPlayedDiff = AsInt(GetMember(record, "gamesPlayedDiff")).value_or(0);
            entry.source = AsString(GetMember(record, "source")).value_or(std::string());
            snapshot.mmrHistory.emplace_back(std::move(entry));
        }
    }

    void PopulateTrainingHistorySection(const Value& root, HistorySnapshot& snapshot)
    {
        const Value* trainingValue = GetMember(root, "trainingHistory");
        if (!trainingValue || trainingValue->type != Type::Array)
        {
            return;
        }

        for (const auto& record : trainingValue->arrayValue)
        {
            if (record.type != Type::Object)
            {
                continue;
            }

            TrainingHistoryEntry entry;
            entry.id = AsString(GetMember(record, "id")).value_or(std::string());
            entry.startedTime = AsString(GetMember(record, "startedTime")).value_or(std::string());
            entry.finishedTime = AsString(GetMember(record, "finishedTime")).value_or(std::string());
            entry.source = AsString(GetMember(record, "source")).value_or(std::string());
            entry.presetId = AsString(GetMember(record, "presetId")).value_or(std::string());
            entry.notes = AsString(GetMember(record, "notes")).value_or(std::string());
            entry.actualDuration = AsInt(GetMember(record, "actualDuration")).value_or(0);
            entry.blocks = AsInt(GetMember(record, "blocks")).value_or(0);
            entry.skillIds = AsStringList(GetMember(record, "skillIds"));
            snapshot.trainingHistory.emplace_back(std::move(entry));
        }
    }

    void PopulateStatusFilters(const Value& filtersValue, HistorySnapshot& snapshot)
    {
        AssignStringMember(filtersValue, "playlist", snapshot.status.filters.playlist);
        AssignStringMember(filtersValue, "mmrFrom", snapshot.status.filters.mmrFrom);
        AssignStringMember(filtersValue, "mmrTo", snapshot.status.filters.mmrTo);
        AssignStringMember(filtersValue, "sessionStart", snapshot.status.filters.sessionStart);
        AssignStringMember(filtersValue, "sessionEnd", snapshot.status.filters.sessionEnd);
    }

    void PopulateStatusSection(const Value& root, HistorySnapshot& snapshot)
    {
        const Value* statusValue = GetMember(root, "status");
        if (!statusValue || statusValue->type != Type::Object)
        {
            return;
        }

        AssignStringMember(*statusValue, "receivedAt", snapshot.status.receivedAt);
        AssignStringMember(*statusValue, "generatedAt", snapshot.status.generatedAt);
        AssignStringMember(*statusValue, "lastMmrTimestamp", snapshot.status.lastMmrTimestamp);
        AssignStringMember(*statusValue, "lastTrainingTimestamp", snapshot.status.lastTrainingTimestamp);

        AssignIntMember(*statusValue, "mmrEntries", snapshot.status.mmrEntries);
        AssignIntMember(*statusValue, "trainingSessions", snapshot.status.trainingSessions);
        AssignIntMember(*statusValue, "mmrLimit", snapshot.status.mmrLimit);
        AssignIntMember(*statusValue, "sessionLimit", snapshot.status.sessionLimit);

        if (const Value* filtersValue = GetMember(*statusValue, "filters");
            filtersValue && filtersValue->type == Type::Object)
        {
            PopulateStatusFilters(*filtersValue, snapshot);
        }
    }
}

bool HistoryJson::ParseHistoryResponse(const std::string& payload, HistorySnapshot& snapshot, std::string& error)
{
    Parser parser(payload);
    Value root;
    if (!parser.Parse(root, error))
    {
        return false;
    }

    snapshot = HistorySnapshot();

    PopulateMmrHistorySection(root, snapshot);
    PopulateTrainingHistorySection(root, snapshot);
    PopulateStatusSection(root, snapshot);

    return true;
}
