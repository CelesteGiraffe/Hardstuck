#include "pch.h"
#include "settings/SettingsService.h"

#include <memory>

#include "bakkesmod/wrappers/CVarManagerWrapper.h"

#include "diagnostics/DiagnosticLogger.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <vector>
#include <system_error>
#include <random>
#include <sstream>

namespace
{
    std::string Trimmed(const std::string& value)
    {
        std::string trimmed = value;
        trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](unsigned char ch) {
                          return !std::isspace(ch);
                      }).base(),
                      trimmed.end());
        return trimmed;
    }

}

SettingsService::SettingsService(std::shared_ptr<CVarManagerWrapper> cvarManager)
    : cvarManager_(std::move(cvarManager))
{
}

void SettingsService::RegisterCVars()
{
    if (!cvarManager_)
    {
        DiagnosticLogger::Log("SettingsService::RegisterCVars: cvarManager unavailable");
        return;
    }

    const std::filesystem::path defaultDir = GetSettingsPath().parent_path() / "data";
    dataDirectory_ = defaultDir;

    cvarManager_->registerCvar(settings::kDataDirCvarName, defaultDir.string(), "Directory for Hardstuck local data");
    cvarManager_->registerCvar(settings::kStoreMaxBytesCvarName, std::to_string(maxStoreBytes_), "Max size per data file in bytes before rotation");
    cvarManager_->registerCvar(settings::kStoreMaxFilesCvarName, std::to_string(maxStoreFiles_), "Max number of rotated data files to keep");
    cvarManager_->registerCvar(settings::kFocusListCvarName, SerializeFocusList(focusList_), "List of focus labels separated by '|'");
    cvarManager_->registerCvar("hs_install_id", GenerateInstallId(), "Generated install identifier (do not edit)");

    cvarManager_->registerCvar(settings::kUiEnabledCvarName, "1", "Legacy UI toggle (window now follows togglemenu)");
    cvarManager_->registerCvar("hs_ui_debug_show_demo", "0", "Show ImGui demo window for debugging (1 = show)");
    cvarManager_->registerCvar(settings::kGamesPlayedCvarName, "1", "Increment for gamesPlayedDiff payload field");
    cvarManager_->registerCvar(settings::kPostMatchDelayCvarName, "4.0", "Seconds to wait after a match before refreshing MMR");
}

void SettingsService::LoadPersistedSettings()
{
    const std::filesystem::path path = GetSettingsPath();
    std::ifstream input(path);
    if (!input.is_open())
    {
        DiagnosticLogger::Log(std::string("SettingsService::LoadPersistedSettings: missing settings file at ") + path.string());
        SavePersistedSettings();
        return;
    }

    std::string line;
    std::string fileDataDir;
    std::string fileMaxBytes;
    std::string fileMaxFiles;
    std::string fileFocusList;
    std::string fileInstallId;

    while (std::getline(input, line))
    {
        const std::string trimmedLine = Trimmed(line);
        if (trimmedLine.empty() || trimmedLine[0] == '#')
        {
            continue;
        }

        const auto eqPos = trimmedLine.find('=');
        if (eqPos == std::string::npos)
        {
            continue;
        }

        const std::string key = Trimmed(trimmedLine.substr(0, eqPos));
        const std::string value = Trimmed(trimmedLine.substr(eqPos + 1));

        if (key == "data_dir")
        {
            fileDataDir = value;
        }
        else if (key == "store_max_bytes")
        {
            fileMaxBytes = value;
        }
        else if (key == "store_max_files")
        {
            fileMaxFiles = value;
        }
        else if (key == "focuses")
        {
            fileFocusList = value;
        }
        else if (key == "install_id")
        {
            fileInstallId = value;
        }
    }

    if (!fileDataDir.empty())
    {
        SetDataDirectory(fileDataDir);
    }
    if (!fileMaxBytes.empty())
    {
        try { SetMaxStoreBytes(static_cast<uint64_t>(std::stoull(fileMaxBytes))); } catch (...) {}
    }
    if (!fileMaxFiles.empty())
    {
        try { SetMaxStoreFiles(std::stoi(fileMaxFiles)); } catch (...) {}
    }
    if (!fileFocusList.empty())
    {
        SetFocusList(DeserializeFocusList(fileFocusList));
    }
    if (!fileInstallId.empty())
    {
        installId_ = fileInstallId;
    }
}

void SettingsService::SavePersistedSettings()
{
    const std::filesystem::path path = GetSettingsPath();
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream output(path, std::ios::out | std::ios::trunc);
    if (!output.is_open())
    {
        DiagnosticLogger::Log(std::string("SettingsService::SavePersistedSettings: failed to open settings file at ") + path.string());
        return;
    }

    output << "data_dir=" << GetDataDirectory().string() << "\n";
    output << "store_max_bytes=" << GetMaxStoreBytes() << "\n";
    output << "store_max_files=" << GetMaxStoreFiles() << "\n";
    output << "focuses=" << SerializeFocusList(GetFocusList()) << "\n";
    output << "install_id=" << GetInstallId() << "\n";
}

std::filesystem::path SettingsService::GetSettingsPath() const
{
    std::filesystem::path base;
#if defined(_WIN32)
    char* appdata_env = nullptr;
    size_t env_len = 0;
    if (_dupenv_s(&appdata_env, &env_len, "APPDATA") == 0 && appdata_env && appdata_env[0] != '\0')
    {
        base = appdata_env;
    }
    if (appdata_env)
    {
        free(appdata_env);
    }
#else
    const char* appdata_env = std::getenv("APPDATA");
    if (appdata_env && appdata_env[0] != '\0')
    {
        base = appdata_env;
    }
#endif
    if (base.empty())
    {
        base = std::filesystem::temp_directory_path();
    }

    return base / "bakkesmod" / "hardstuck" / "settings.cfg";
}

std::filesystem::path SettingsService::GetDataDirectory() const
{
    if (!dataDirectory_.empty())
    {
        return dataDirectory_;
    }

    if (!cvarManager_)
    {
        return GetSettingsPath().parent_path() / "data";
    }

    try
    {
        const std::string value = cvarManager_->getCvar(settings::kDataDirCvarName).getStringValue();
        return std::filesystem::path(value);
    }
    catch (...)
    {
        DiagnosticLogger::Log("SettingsService::GetDataDirectory: failed to read hs_data_dir, using default");
        return GetSettingsPath().parent_path() / "data";
    }
}

void SettingsService::SetDataDirectory(const std::filesystem::path& dir)
{
    dataDirectory_ = dir;
    if (!cvarManager_)
    {
        return;
    }

    try
    {
        cvarManager_->getCvar(settings::kDataDirCvarName).setValue(dir.string());
    }
    catch (...)
    {
        DiagnosticLogger::Log("SettingsService::SetDataDirectory: failed to set hs_data_dir");
    }
}

std::string SettingsService::GenerateInstallId() const
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 15);
    std::ostringstream oss;
    for (int i = 0; i < 16; ++i)
    {
        oss << std::hex << dist(gen);
    }
    return oss.str();
}

std::string SettingsService::GetInstallId()
{
    if (!installId_.empty())
    {
        return installId_;
    }

    if (cvarManager_)
    {
        try
        {
            installId_ = cvarManager_->getCvar("hs_install_id").getStringValue();
        }
        catch (...)
        {
            installId_.clear();
        }
    }

    if (installId_.empty())
    {
        installId_ = GenerateInstallId();
        if (cvarManager_)
        {
            try
            {
                cvarManager_->getCvar("hs_install_id").setValue(installId_);
            }
            catch (...) {}
        }
    }

    return installId_;
}

uint64_t SettingsService::GetMaxStoreBytes() const
{
    if (!cvarManager_)
    {
        return maxStoreBytes_;
    }
    return ParseUint64Cvar(settings::kStoreMaxBytesCvarName, maxStoreBytes_);
}

void SettingsService::SetMaxStoreBytes(uint64_t bytes)
{
    maxStoreBytes_ = bytes;
    if (!cvarManager_)
    {
        return;
    }

    try
    {
        cvarManager_->getCvar(settings::kStoreMaxBytesCvarName).setValue(std::to_string(bytes));
    }
    catch (...)
    {
        DiagnosticLogger::Log("SettingsService::SetMaxStoreBytes: failed to set hs_store_max_bytes");
    }
}

int SettingsService::GetMaxStoreFiles() const
{
    if (!cvarManager_)
    {
        return maxStoreFiles_;
    }
    return ParseIntCvar(settings::kStoreMaxFilesCvarName, maxStoreFiles_);
}

void SettingsService::SetMaxStoreFiles(int files)
{
    maxStoreFiles_ = std::max(1, files);
    if (!cvarManager_)
    {
        return;
    }

    try
    {
        cvarManager_->getCvar(settings::kStoreMaxFilesCvarName).setValue(std::to_string(maxStoreFiles_));
    }
    catch (...)
    {
        DiagnosticLogger::Log("SettingsService::SetMaxStoreFiles: failed to set hs_store_max_files");
    }
}

std::vector<std::string> SettingsService::NormalizeFocusList(const std::vector<std::string>& focuses)
{
    std::vector<std::string> normalized;
    for (const auto& focus : focuses)
    {
        const std::string trimmed = Trimmed(focus);
        if (trimmed.empty())
        {
            continue;
        }
        const bool exists = std::find_if(normalized.begin(), normalized.end(), [&](const std::string& existing) {
            if (existing.size() != trimmed.size())
            {
                return false;
            }
            for (size_t i = 0; i < existing.size(); ++i)
            {
                if (std::tolower(static_cast<unsigned char>(existing[i])) != std::tolower(static_cast<unsigned char>(trimmed[i])))
                {
                    return false;
                }
            }
            return true;
        }) != normalized.end();
        if (!exists)
        {
            normalized.push_back(trimmed);
        }
    }
    if (normalized.empty())
    {
        normalized.emplace_back("Training focus");
    }
    return normalized;
}

std::string SettingsService::SerializeFocusList(const std::vector<std::string>& focuses)
{
    std::ostringstream oss;
    bool first = true;
    for (const auto& focus : focuses)
    {
        if (!first)
        {
            oss << '|';
        }
        first = false;
        oss << focus;
    }
    return oss.str();
}

std::vector<std::string> SettingsService::DeserializeFocusList(const std::string& serialized)
{
    std::vector<std::string> parsed;
    std::stringstream ss(serialized);
    std::string item;
    while (std::getline(ss, item, '|'))
    {
        parsed.push_back(item);
    }
    return NormalizeFocusList(parsed);
}

std::vector<std::string> SettingsService::GetFocusList() const
{
    if (!cvarManager_)
    {
        return focusList_;
    }

    try
    {
        const std::string serialized = cvarManager_->getCvar(settings::kFocusListCvarName).getStringValue();
        return DeserializeFocusList(serialized);
    }
    catch (...)
    {
        return focusList_;
    }
}

void SettingsService::SetFocusList(const std::vector<std::string>& focuses)
{
    focusList_ = NormalizeFocusList(focuses);
    if (!cvarManager_)
    {
        return;
    }

    try
    {
        cvarManager_->getCvar(settings::kFocusListCvarName).setValue(SerializeFocusList(focusList_));
    }
    catch (...)
    {
        DiagnosticLogger::Log("SettingsService::SetFocusList: failed to set hs_focus_list");
    }
}

int SettingsService::GetGamesPlayedIncrement() const
{
    if (!cvarManager_)
    {
        return 1;
    }

    try
    {
        return cvarManager_->getCvar(settings::kGamesPlayedCvarName).getIntValue();
    }
    catch (...)
    {
        DiagnosticLogger::Log("SettingsService::GetGamesPlayedIncrement: failed to read hs_games_played_increment, defaulting to 1");
        return 1;
    }
}

float SettingsService::GetPostMatchMmrDelaySeconds() const
{
    if (!cvarManager_)
    {
        return 4.0f;
    }

    try
    {
        const std::string value = cvarManager_->getCvar(settings::kPostMatchDelayCvarName).getStringValue();
        return std::stof(value);
    }
    catch (...)
    {
        DiagnosticLogger::Log("SettingsService::GetPostMatchMmrDelaySeconds: failed to read hs_post_match_mmr_delay, defaulting to 4.0");
        return 4.0f;
    }
}

uint64_t SettingsService::ParseUint64Cvar(const char* name, uint64_t defaultValue) const
{
    if (!cvarManager_)
    {
        return defaultValue;
    }

    try
    {
        const std::string value = cvarManager_->getCvar(name).getStringValue();
        return static_cast<uint64_t>(std::stoull(value));
    }
    catch (...)
    {
        return defaultValue;
    }
}

int SettingsService::ParseIntCvar(const char* name, int defaultValue) const
{
    if (!cvarManager_)
    {
        return defaultValue;
    }

    try
    {
        return cvarManager_->getCvar(name).getIntValue();
    }
    catch (...)
    {
        return defaultValue;
    }
}

std::string SettingsService::ReadStringCvar(const char* name, const char* fallback) const
{
    if (!cvarManager_)
    {
        return fallback ? std::string(fallback) : std::string();
    }

    try
    {
        return cvarManager_->getCvar(name).getStringValue();
    }
    catch (...)
    {
        return fallback ? std::string(fallback) : std::string();
    }
}
