#include "pch.h"
#include "Hardstuck.h"

#include "diagnostics/DiagnosticLogger.h"
#include "payload/HsPayloadBuilder.h"
#include "settings/SettingsService.h"
#include "storage/LocalDataStore.h"
#include "src/user/UserIdResolver.h"
#include <sstream>
#include <filesystem>

// Using the plugin_version symbol from Hardstuck.h's include of version.h
BAKKESMOD_PLUGIN(Hardstuck, "Hardstuck : Rocket League Training Journal", plugin_version, PERMISSION_ALL)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void Hardstuck::onLoad()
{
	_globalCvarManager = cvarManager;

	// Ensure settings service and backend are initialized early so UI and backend operations work.
	InitializeSettingsService();
	InitializeBackend();
	RegisterSessionCommands();
	RegisterUiCommands();

	// Hook match events so post-match staged capture and uploads run automatically.
	try
	{
		HookMatchEvents();
		if (cvarManager)
		{
			cvarManager->log("HS: hooked match events on load");
		}
	}
	catch (...)
	{
		DiagnosticLogger::Log("onLoad: failed to hook match events");
	}
}

void Hardstuck::onUnload()
{
	PersistSettings();
	ShutdownBackend();
	UnregisterUi();
	pendingMatchUploads_.clear();
}

void Hardstuck::InitializeSettingsService()
{
	if (settingsService_)
	{
		return;
	}

	settingsService_ = std::make_unique<SettingsService>(cvarManager);
	settingsService_->RegisterCVars();
	settingsService_->LoadPersistedSettings();
}

void Hardstuck::InitializeBackend()
{
	std::filesystem::path dataDir;
	if (settingsService_)
	{
		dataDir = static_cast<SettingsService*>(settingsService_.get())->GetDataDirectory();
	}
	if (dataDir.empty())
	{
		dataDir = std::filesystem::temp_directory_path() / "hardstuck";
	}
	resolvedUserId_ = UserIdResolver::ResolveUserId(gameWrapper.get(), static_cast<SettingsService*>(settingsService_.get()));
	DiagnosticLogger::Log(std::string("onLoad: creating LocalDataStore at ") + dataDir.string() + " for user " + resolvedUserId_);
	auto dataStore = std::make_unique<LocalDataStore>(dataDir, resolvedUserId_);
	if (settingsService_)
	{
		dataStore->SetLimits(
			static_cast<SettingsService*>(settingsService_.get())->GetMaxStoreBytes(),
			static_cast<SettingsService*>(settingsService_.get())->GetMaxStoreFiles()
		);
	}
	backend_ = std::make_unique<HsBackend>(
		std::move(dataStore),
		resolvedUserId_,
		cvarManager.get(),
		gameWrapper.get(),
		static_cast<SettingsService*>(settingsService_.get())
	);
	if (cvarManager)
	{
		cvarManager->log("HS: backend created");
	}
}

void Hardstuck::PersistSettings() const
{
	if (settingsService_)
	{
		settingsService_->SavePersistedSettings();
	}
}
void Hardstuck::ShutdownBackend()
{
	if (!backend_)
	{
		return;
	}

	backend_->CleanupFinishedRequests();
	backend_.reset();
}

void Hardstuck::UnregisterUi()
{
	if (!gameWrapper)
	{
		return;
	}

	gameWrapper->UnregisterDrawables();
	if (cvarManager)
	{
		cvarManager->log("HS: unregistered drawables");
	}
}

bool Hardstuck::BindImGuiContext() const
{
	if (imguiContext_)
	{
		ImGui::SetCurrentContext(imguiContext_);
	}
	return ImGui::GetCurrentContext() != nullptr;
}

void Hardstuck::RenderOverlay(const std::string& lastResponse,
	const std::string& lastError,
	const HistorySnapshot& historySnapshot,
	const std::string& historyError,
	bool historyLoading,
	std::chrono::system_clock::time_point historyLastFetched)
{
	const bool inFreeplay = IsInFreeplay(gameWrapper.get());
	const std::string sessionLabel = CurrentSessionTypeString(inFreeplay, 0);
	const bool manualActive = focusedSessionActive_ || currentSessionLabel_ != SessionLabel::Unknown;
	HsRenderOverlayUi(
		cvarManager.get(),
		lastResponse,
		lastError,
		historySnapshot,
		historyError,
		historyLoading,
		historyLastFetched,
		sessionLabel,
		manualActive,
		[this]() { TriggerManualUpload(); },
		[this]() { ExecuteHistoryWindowCommand(); },
		[this]() { FetchHistory(); }
	);
}

void Hardstuck::DispatchPayloadAsync(const std::string& endpoint, const std::string& body)
{
	if (!backend_)
	{
		if (cvarManager)
		{
			cvarManager->log("HS: backend not initialised; cannot dispatch payload");
		}
		return;
	}
	backend_->DispatchPayloadAsync(endpoint, body);
}

void Hardstuck::RenderHistoryWindow(const HistorySnapshot& snapshot,
	const std::string& errorMessage,
	bool loading,
	std::chrono::system_clock::time_point lastFetched)
{
	const bool inFreeplay = IsInFreeplay(gameWrapper.get());
	const std::string sessionLabel = CurrentSessionTypeString(inFreeplay, 0);
	const bool manualActive = focusedSessionActive_ || currentSessionLabel_ != SessionLabel::Unknown;
	HsRenderHistoryWindowUi(snapshot, errorMessage, loading, lastFetched, &showHistoryWindow_, sessionLabel, manualActive);
}

void Hardstuck::RenderSettings()
{
	if (!BindImGuiContext())
	{
		return;
	}

	HsRenderSettingsUi(
		settingsService_.get(),
		cvarManager.get(),
		[this]() { TriggerManualUpload(); },
		backend_ ? backend_->GetStorePath() : std::filesystem::path()
	);
}

std::string Hardstuck::GetPluginName()
{
	return "Hardstuck : Rocket League Training Journal";
}

std::string Hardstuck::GetMenuName()
{
	return "hardstuck"; // internal menu name (no spaces)
}

std::string Hardstuck::GetMenuTitle()
{
	return "Hardstuck : Rocket League Training Journal"; // title shown in the BakkesMod menu
}

void Hardstuck::SetImGuiContext(uintptr_t ctx)
{
	// Store the context pointer so we can bind it on whichever thread renders.
	imguiContext_ = reinterpret_cast<ImGuiContext*>(ctx);
	ImGui::SetCurrentContext(imguiContext_);
}

bool Hardstuck::IsInFreeplay(GameWrapper* gw) const
{
	if (!gw)
	{
		return false;
	}

	bool inFreeplay = false;
	try
	{
		inFreeplay = gw->IsInFreeplay();
	}
	catch (...)
	{
		inFreeplay = false;
	}

	return inFreeplay;
}

bool Hardstuck::CaptureServerAndStageDelayedUpload(ServerWrapper server, const char* contextTag)
{
	if (!server)
	{
		DiagnosticLogger::Log("CaptureServerAndStageDelayedUpload: server invalid");
		return false;
	}

	HsMatchPayloadComponents components;
	int playlistMmrId = 0;
	if (!HsCollectMatchPayloadComponents(server, settingsService_.get(), resolvedUserId_, components, playlistMmrId))
	{
		DiagnosticLogger::Log("CaptureServerAndStageDelayedUpload: failed to collect match components");
		return false;
	}
	components.sessionType = CurrentSessionTypeString(false, playlistMmrId);

	auto pending = std::make_shared<Hardstuck::PendingMatchUpload>();
	pending->components = std::move(components);
	pending->playlistMmrId = playlistMmrId;
	pending->contextTag = contextTag ? contextTag : "match_event";
	pending->finalized = false;
	pending->postDestroyScheduled = false;
	pendingMatchUploads_.push_back(pending);

	if (cvarManager)
	{
		cvarManager->log("HS: staged match payload for delayed MMR refresh");
	}

	const float fallbackDelay = GetPostMatchDelaySeconds() + 2.0f;
	SchedulePendingMatchUpload(pending, fallbackDelay, "fallback_post_match");
	return true;
}

void Hardstuck::SchedulePendingMatchUpload(
	const std::shared_ptr<Hardstuck::PendingMatchUpload>& pending,
	float delaySeconds,
	const char* reason
)
{
	if (!gameWrapper || !pending || pending->finalized)
	{
		return;
	}

	const float delay = std::max(0.5f, delaySeconds);
	const std::string context = pending->contextTag;
	const std::string reasonLabel = reason ? reason : "unspecified";
	DiagnosticLogger::Log(
		std::string("SchedulePendingMatchUpload: context=") + context
		+ ", delay=" + std::to_string(delay)
		+ ", reason=" + reasonLabel
		);

	gameWrapper->SetTimeout([this, pending](GameWrapper* /*gw*/)
	{
		this->FinalizePendingMatchUpload(pending);
	}, delay);
}

void Hardstuck::FinalizePendingMatchUpload(const std::shared_ptr<Hardstuck::PendingMatchUpload>& pending)
{
	if (!pending || pending->finalized)
	{
		return;
	}

	pending->finalized = true;

	const int latestMmr = this->FetchLatestMmr(pending->playlistMmrId);
	const std::string payload = HsBuildMatchPayloadFromComponents(pending->components, latestMmr);
	DiagnosticLogger::Log(
		std::string("FinalizePendingMatchUpload: context=") + pending->contextTag
		+ ", mmr=" + std::to_string(latestMmr)
		);

	this->CacheLastPayload(payload, pending->contextTag.c_str());
	this->DispatchPayloadAsync("/api/mmr-log", payload);
	this->RemovePendingMatchUpload(pending);
}

void Hardstuck::RemovePendingMatchUpload(const std::shared_ptr<Hardstuck::PendingMatchUpload>& pending)
{
	if (!pending)
	{
		return;
	}

	pendingMatchUploads_.erase(
		std::remove_if(
			pendingMatchUploads_.begin(),
			pendingMatchUploads_.end(),
			[&pending](const std::shared_ptr<Hardstuck::PendingMatchUpload>& candidate)
			{
				return candidate == pending;
			}
		),
		pendingMatchUploads_.end()
	);
}

int Hardstuck::FetchLatestMmr(int playlistMmrId) const
{
	float rating = 0.0f;
	const bool hasRating = HsTryFetchPlaylistRating(gameWrapper.get(), playlistMmrId, rating);
	return hasRating ? static_cast<int>(std::round(rating)) : 0;
}

float Hardstuck::GetPostMatchDelaySeconds() const
{
	if (!settingsService_)
	{
		return 4.0f;
	}
	return settingsService_->GetPostMatchMmrDelaySeconds();
}

void Hardstuck::TriggerManualUpload()
{
	if (!gameWrapper)
	{
		if (cvarManager)
		{
			cvarManager->log("HS: no game wrapper");
		}
		return;
	}

	if (backend_)
	{
		backend_->FlushBufferedWrites();
	}

	gameWrapper->Execute([this](GameWrapper* gw) {
		const bool inFreeplay = IsInFreeplay(gw);
		const char* manualContext = inFreeplay ? "manual_sync_freeplay" : "manual_sync";

		if (!inFreeplay)
		{
			ServerWrapper server = ResolveActiveServer(gw);
			if (server && CaptureServerAndUpload(server, manualContext))
			{
				if (cvarManager)
				{
					cvarManager->log("HS: manual sync uploaded active match payload");
				}
				return;
			}

			if (!server && cvarManager)
			{
				cvarManager->log("HS: manual sync found no active server; falling back to MMR snapshot");
			}
			else if (cvarManager)
			{
				cvarManager->log("HS: manual sync failed to capture match payload; falling back to MMR snapshot");
			}
		}
		else if (cvarManager)
		{
			cvarManager->log("HS: manual sync detected Freeplay; capturing MMR snapshot instead of match payload");
		}

		if (UploadMmrSnapshot(manualContext))
		{
			if (cvarManager)
			{
				cvarManager->log("HS: manual sync uploaded current ranked MMR snapshot");
			}
			return;
		}

		if (cvarManager)
		{
			cvarManager->log("HS: manual sync could not gather snapshot data; attempting cached payload");
		}

		if (DispatchCachedPayload(manualContext))
		{
			if (cvarManager)
			{
				cvarManager->log("HS: manual sync dispatched cached payload");
			}
			return;
		}

		if (cvarManager)
		{
			cvarManager->log("HS: manual sync failed; no payloads available");
		}
	});
}

ServerWrapper Hardstuck::ResolveActiveServer(GameWrapper* gw) const
{
	if (!gw)
	{
		return ServerWrapper(0);
	}

	ServerWrapper server = gw->GetOnlineGame();
	if (!server)
	{
		server = gw->GetGameEventAsServer();
	}
	return server;
}

bool Hardstuck::CaptureServerAndUpload(ServerWrapper server, const char* contextTag)
{
	const char* tag = contextTag ? contextTag : "unknown";
	if (!server)
	{
		DiagnosticLogger::Log(std::string("CaptureServerAndUpload: server invalid for context ") + tag);
		return false;
	}
	HsMatchPayloadComponents components;
	int playlistMmrId = 0;
	if (!HsCollectMatchPayloadComponents(server, settingsService_.get(), resolvedUserId_, components, playlistMmrId))
	{
		DiagnosticLogger::Log(std::string("CaptureServerAndUpload: failed to collect components for context ") + tag);
		return false;
	}
	components.sessionType = CurrentSessionTypeString(false, playlistMmrId);

	float mmr = 0.0f;
	const bool hasRating = HsTryFetchPlaylistRating(gameWrapper.get(), playlistMmrId, mmr);
	const int finalMmr = hasRating ? static_cast<int>(std::round(mmr)) : 0;

	const std::string payload = HsBuildMatchPayloadFromComponents(components, finalMmr);
	DiagnosticLogger::Log(std::string("CaptureServerAndUpload: context=") + tag + ", payload_len=" + std::to_string(payload.size()));
	if (backend_)
	{
		backend_->CacheLastPayload(payload, tag);
		backend_->DispatchPayloadAsync("/api/mmr-log", payload);
	}
	return true;
}

void Hardstuck::CacheLastPayload(const std::string& payload, const char* contextTag)
{
	if (backend_)
	{
		backend_->CacheLastPayload(payload, contextTag);
	}
}

bool Hardstuck::DispatchCachedPayload(const char* reason)
{
	if (!backend_)
	{
		return false;
	}
	return backend_->DispatchCachedPayload(reason);
}

bool Hardstuck::UploadMmrSnapshot(const char* contextTag)
{
	if (!backend_)
	{
		return false;
	}
	const bool inFreeplay = IsInFreeplay(gameWrapper.get());
	const std::string sessionType = CurrentSessionTypeString(inFreeplay, 0);
	return backend_->UploadMmrSnapshot(contextTag, sessionType);
}

void Hardstuck::FetchHistory()
{
	if (!backend_)
	{
		if (cvarManager)
		{
			cvarManager->log("HS: backend not initialised for history fetch");
		}
		return;
	}
	backend_->FetchHistory();
}

void Hardstuck::OpenHistoryWindow()
{
	showHistoryWindow_ = true;
	FetchHistory();
}

void Hardstuck::ExecuteHistoryWindowCommand()
{
	if (cvarManager)
	{
		cvarManager->executeCommand("hs_history_window");
		return;
	}

	OpenHistoryWindow();
}

void Hardstuck::CleanupFinishedRequests()
{
	if (backend_)
	{
		backend_->CleanupFinishedRequests();
	}
}

bool Hardstuck::ShouldBlockInput()
{
	return false;
}

bool Hardstuck::IsActiveOverlay()
{
	return true;
}

std::string Hardstuck::SessionLabelToString(SessionLabel label) const
{
	switch (label)
	{
	case SessionLabel::FocusedFreeplay: return "focused_freeplay";
	case SessionLabel::TrainingPack:    return "training_pack";
	case SessionLabel::Workshop:        return "workshop";
	case SessionLabel::Casual:          return "casual";
	case SessionLabel::Ranked:          return "ranked";
	default:                            return "unknown";
	}
}

Hardstuck::SessionLabel Hardstuck::ResolveSessionLabel(bool inFreeplay, int playlistMmrId) const
{
	if (currentSessionLabel_ != SessionLabel::Unknown)
	{
		return currentSessionLabel_;
	}

	if (inFreeplay)
	{
		return SessionLabel::FocusedFreeplay;
	}

	return playlistMmrId == 0 ? SessionLabel::Casual : SessionLabel::Ranked;
}

std::string Hardstuck::CurrentSessionTypeString(bool inFreeplay, int playlistMmrId) const
{
	return SessionLabelToString(ResolveSessionLabel(inFreeplay, playlistMmrId));
}

void Hardstuck::SetSessionLabel(SessionLabel label, const char* reason)
{
	currentSessionLabel_ = label;
	DiagnosticLogger::Log(std::string("Session label set to ") + SessionLabelToString(label) +
		" (" + (reason ? reason : "unspecified") + ")");
}

void Hardstuck::WriteFocusedSessionRecord(std::chrono::system_clock::time_point start, std::chrono::system_clock::time_point end)
{
	if (!backend_)
	{
		return;
	}

	const auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
	std::ostringstream oss;
	oss << '{'
		<< "\"timestamp\":" << JsonEscape(FormatTimestamp(start)) << ','
		<< "\"playlist\":\"Freeplay\"," 
		<< "\"mmr\":0," 
		<< "\"gamesPlayedDiff\":0," 
		<< "\"source\":\"manual_session\"," 
		<< "\"sessionType\":\"focused_freeplay\"," 
		<< "\"userId\":" << JsonEscape(resolvedUserId_) << ','
		<< "\"durationSeconds\":" << duration << ','
		<< "\"teams\":[],"
		<< "\"scoreboard\":[]"
		<< '}';

	backend_->DispatchPayloadAsync("/api/manual-session", oss.str());
}

void Hardstuck::StartFocusedFreeplayTimer()
{
	if (focusedSessionActive_)
	{
		return;
	}
	focusedSessionActive_ = true;
	focusedSessionStart_ = std::chrono::system_clock::now();
	SetSessionLabel(SessionLabel::FocusedFreeplay, "focused_timer_start");
}

void Hardstuck::StopFocusedFreeplayTimer()
{
	if (!focusedSessionActive_)
	{
		return;
	}
	const auto end = std::chrono::system_clock::now();
	const auto start = focusedSessionStart_;
	focusedSessionActive_ = false;
	WriteFocusedSessionRecord(start, end);
}

void Hardstuck::ToggleFocusedFreeplayTimer()
{
	if (focusedSessionActive_)
	{
		StopFocusedFreeplayTimer();
	}
	else
	{
		StartFocusedFreeplayTimer();
	}
}

void Hardstuck::RegisterSessionCommands()
{
	if (!cvarManager)
	{
		return;
	}

	auto registerSession = [this](const char* name, SessionLabel label, const char* description)
	{
		cvarManager->registerNotifier(
			name,
			[this, label, name](auto) { SetSessionLabel(label, name); },
			description,
			PERMISSION_ALL
		);
	};

	registerSession("hs_session_focus_freeplay", SessionLabel::FocusedFreeplay, "Mark session as focused freeplay");
	registerSession("hs_session_training_pack", SessionLabel::TrainingPack, "Mark session as training pack");
	registerSession("hs_session_workshop", SessionLabel::Workshop, "Mark session as workshop");
	registerSession("hs_session_casual", SessionLabel::Casual, "Mark session as casual");
	registerSession("hs_session_ranked", SessionLabel::Ranked, "Mark session as ranked");

	cvarManager->registerNotifier(
		"hs_toggle_focus_session",
		[this](auto) { ToggleFocusedFreeplayTimer(); },
		"Start/stop focused freeplay manual session",
		PERMISSION_ALL
	);
}

void Hardstuck::RegisterUiCommands()
{
	if (!cvarManager)
	{
		return;
	}

	cvarManager->registerNotifier(
		"hs_history_window",
		[this](auto) { OpenHistoryWindow(); },
		"Open the Hardstuck history window and refresh history data",
		PERMISSION_ALL
	);
}
void Hardstuck::OnOpen()
{
	menuOpen_ = true;
	if (!showHistoryWindow_)
	{
		showHistoryWindow_ = true;
		FetchHistory();
	}
}

void Hardstuck::OnClose()
{
	menuOpen_ = false;
	showHistoryWindow_ = false;
}

void Hardstuck::Render()
{
	std::string lastResponse;
	std::string lastError;
	if (backend_)
	{
		backend_->SnapshotRequestState(lastResponse, lastError);
	}
	std::string storageStatus;
	size_t buffered = 0;
	if (backend_)
	{
		backend_->SnapshotStorageDiagnostics(storageStatus, buffered);
		if (!storageStatus.empty())
		{
			lastResponse = storageStatus + " | buffered=" + std::to_string(buffered);
		}
	}

	if (!BindImGuiContext())
	{
		return;
	}

	HistorySnapshot historySnapshot;
	std::string historyError;
	bool historyLoading = false;
	std::chrono::system_clock::time_point historyLastFetched;
	if (backend_ && (showHistoryWindow_ || menuOpen_))
	{
		backend_->SnapshotHistory(historySnapshot, historyError, historyLoading, historyLastFetched);
	}

	if (showHistoryWindow_)
	{
		RenderHistoryWindow(historySnapshot, historyError, historyLoading, historyLastFetched);
	}
	if (!menuOpen_)
	{
		return;
	}

	RenderOverlay(lastResponse, lastError, historySnapshot, historyError, historyLoading, historyLastFetched);
	if (showHistoryWindow_)
	{
		RenderHistoryWindow(historySnapshot, historyError, historyLoading, historyLastFetched);
	}
}

// Stub implementations for match event hooks
void Hardstuck::HookMatchEvents()
{
	if (!gameWrapper)
	{
		if (cvarManager) cvarManager->log("HS: gameWrapper unavailable; cannot hook events");
		return;
	}

	try
	{
		gameWrapper->HookEvent("Function TAGame.GameEvent_Soccar_TA.EventMatchEnded",
			std::bind(&Hardstuck::HandleGameEnd, this, std::placeholders::_1));
		gameWrapper->HookEvent("Function TAGame.ReplayDirector_TA.EventReplayRecorded",
			std::bind(&Hardstuck::HandleReplayRecorded, this, std::placeholders::_1));
		gameWrapper->HookEvent("Function TAGame.GameInfo_TA.Destroyed",
			std::bind(&Hardstuck::HandleGameDestroyed, this, std::placeholders::_1));
		if (cvarManager) cvarManager->log("HS: hooked match end, replay recorded, and game destroyed events");
	}
	catch(...)
	{
		DiagnosticLogger::Log("HookMatchEvents: exception hooking events");
	}
}

void Hardstuck::HandleGameEnd(std::string eventName)
{
	DiagnosticLogger::Log(std::string("HandleGameEnd: event=") + eventName);
	if (!gameWrapper) return;
	gameWrapper->Execute([this](GameWrapper* gw){
		const bool inFreeplay = IsInFreeplay(gw);
		const char* context = inFreeplay ? "match_end_freeplay" : "match_end";
		if (!inFreeplay)
		{
			ServerWrapper server = ResolveActiveServer(gw);
			if (CaptureServerAndStageDelayedUpload(server, context)) return;
		}
		else
		{
			DiagnosticLogger::Log("HandleGameEnd: skipping match payload because session is Freeplay");
		}

		if (UploadMmrSnapshot(context)) return;
		DispatchCachedPayload(context);
	});
}

void Hardstuck::HandleReplayRecorded(std::string eventName)
{
	DiagnosticLogger::Log(std::string("HandleReplayRecorded: event=") + eventName);
	if (!gameWrapper) return;
	gameWrapper->Execute([this](GameWrapper* gw){
		const bool inFreeplay = IsInFreeplay(gw);
		const char* context = inFreeplay ? "replay_recorded_freeplay" : "replay_recorded";
		if (!inFreeplay)
		{
			ServerWrapper server = ResolveActiveServer(gw);
			if (CaptureServerAndStageDelayedUpload(server, context)) return;
		}
		else
		{
			DiagnosticLogger::Log("HandleReplayRecorded: skipping match payload because session is Freeplay");
		}

		if (UploadMmrSnapshot(context)) return;
		DispatchCachedPayload(context);
	});
}

void Hardstuck::HandleGameDestroyed(std::string eventName)
{
	DiagnosticLogger::Log(std::string("HandleGameDestroyed: event=") + eventName);
	if (!gameWrapper)
	{
		return;
	}

	gameWrapper->Execute([this](GameWrapper* /*gw*/){
		if (pendingMatchUploads_.empty())
		{
			return;
		}

		const float delay = GetPostMatchDelaySeconds();
		for (const auto& pending : pendingMatchUploads_)
		{
			if (!pending || pending->finalized || pending->postDestroyScheduled)
			{
				continue;
			}

			pending->postDestroyScheduled = true;
			this->SchedulePendingMatchUpload(pending, delay, "post_destroy");
		}
	});
}
