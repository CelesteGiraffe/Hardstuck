#pragma once

// keep version and plugin version macro
#include "version.h"
constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

#include "GuiBase.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/pluginsettingswindow.h"
#include "bakkesmod/wrappers/GameWrapper.h"
#include "bakkesmod/wrappers/GameEvent/ServerWrapper.h"
#include "pch.h"

// Core includes used by migrated code
#include "settings/ISettingsService.h"
#include "diagnostics/DiagnosticLogger.h"
#include "settings/SettingsService.h"
#include "utils/HsUtils.h"
#include "backend/HsBackend.h"
#include "payload/HsPayloadBuilder.h"
#include <chrono>

// ImGui includes are provided via pch.h

// History types
#include "history/HistoryTypes.h"

// Replace the template skeleton with the migrated plugin surface area
class Hardstuck : public BakkesMod::Plugin::BakkesModPlugin,
			  public BakkesMod::Plugin::PluginSettingsWindow,
			  public BakkesMod::Plugin::PluginWindow
{
public:
	// Lifecycle
	void onLoad() override;
	void onUnload() override;

	// UI hooks
	void Render() override;
	void RenderSettings() override;
	void SetImGuiContext(uintptr_t ctx) override;
	bool ShouldBlockInput() override;
	bool IsActiveOverlay() override;
	void OnOpen() override;
	void OnClose() override;

	// Menu metadata
	std::string GetPluginName() override;
	std::string GetMenuName() override;
	std::string GetMenuTitle() override;

private:
	struct PendingMatchUpload {
		HsMatchPayloadComponents components;
		int playlistMmrId;
		std::string contextTag;
		bool finalized;
		bool postDestroyScheduled;
	};

	// functionality and helpers are implemented in Hardstuck.cpp
	void HookMatchEvents();
	void HandleGameEnd(std::string eventName);
	void HandleReplayRecorded(std::string eventName);
	void HandleGameDestroyed(std::string eventName);
	ServerWrapper ResolveActiveServer(GameWrapper* gw) const;
	bool CaptureServerAndUpload(ServerWrapper server, const char* contextTag);
	bool CaptureServerAndStageDelayedUpload(ServerWrapper server, const char* contextTag);
	void CacheLastPayload(const std::string& payload, const char* contextTag);
	bool DispatchCachedPayload(const char* reason);
	bool UploadMmrSnapshot(const char* contextTag);
	void DispatchPayloadAsync(const std::string& endpoint, const std::string& body);
	void CleanupFinishedRequests();
	void TriggerManualUpload();
	void FetchHistory();
	void OpenHistoryWindow();
	void ExecuteHistoryWindowCommand();
	void RenderHistoryWindow();
	void InitializeSettingsService();
	void InitializeBackend();
	void PersistSettings() const;
	void ShutdownBackend();
	void UnregisterUi();
	bool BindImGuiContext() const;
	void RenderOverlay(const std::string& lastResponse, const std::string& lastError);
	bool IsInFreeplay(GameWrapper* gw) const;
	void SchedulePendingMatchUpload(const std::shared_ptr<PendingMatchUpload>& pending, float delaySeconds, const char* reason);
	void FinalizePendingMatchUpload(const std::shared_ptr<PendingMatchUpload>& pending);
	void RemovePendingMatchUpload(const std::shared_ptr<PendingMatchUpload>& pending);
	int FetchLatestMmr(int playlistMmrId) const;
	float GetPostMatchDelaySeconds() const;
	void RegisterSessionCommands();
	enum class SessionLabel { Unknown, FocusedFreeplay, TrainingPack, Workshop, Casual, Ranked };
	SessionLabel ResolveSessionLabel(bool inFreeplay, int playlistMmrId) const;
	std::string SessionLabelToString(SessionLabel label) const;
	void SetSessionLabel(SessionLabel label, const char* reason);
	void ToggleFocusedFreeplayTimer();
	void StartFocusedFreeplayTimer();
	void StopFocusedFreeplayTimer();
	void WriteFocusedSessionRecord(std::chrono::system_clock::time_point start, std::chrono::system_clock::time_point end);
	std::string CurrentSessionTypeString(bool inFreeplay, int playlistMmrId) const;

	std::unique_ptr<class HsBackend> backend_;
	bool showHistoryWindow_ = false;
	std::vector<std::shared_ptr<PendingMatchUpload>> pendingMatchUploads_;
	ImGuiContext* imguiContext_ = nullptr;
	bool menuOpen_ = false;
	std::unique_ptr<ISettingsService> settingsService_;
	SessionLabel currentSessionLabel_{SessionLabel::Unknown};
	bool focusedSessionActive_{false};
	std::chrono::system_clock::time_point focusedSessionStart_{};
	std::string resolvedUserId_;
};
