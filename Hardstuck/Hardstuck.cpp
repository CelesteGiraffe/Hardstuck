#include "pch.h"
#include "Hardstuck.h"

#include "diagnostics/DiagnosticLogger.h"
#include "payload/HsPayloadBuilder.h"
#include "settings/SettingsService.h"

// Using the plugin_version symbol from Hardstuck.h's include of version.h
BAKKESMOD_PLUGIN(Hardstuck, "Hardstuck : Rocket League Training Journal", plugin_version, PERMISSION_ALL)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void Hardstuck::onLoad()
{
	_globalCvarManager = cvarManager;

	// Ensure settings service and backend are initialized early so UI and backend operations work.
	InitializeSettingsService();
	InitializeBackend();

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
	std::string baseUrl = settingsService_ ? settingsService_->GetBaseUrl() : std::string();
	DiagnosticLogger::Log(std::string("onLoad: creating ApiClient with baseUrl=") + baseUrl);
	auto client = std::make_unique<ApiClient>(baseUrl);
	backend_ = std::make_unique<HsBackend>(
		std::move(client),
		cvarManager.get(),
		gameWrapper.get(),
		static_cast<SettingsService*>(settingsService_.get())
	);
	if (cvarManager)
	{
		cvarManager->log("HS: backend created");
	}

	// cache base URL so changes to settings propagate to backend
	cachedBaseUrl_ = baseUrl;
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

void Hardstuck::RenderOverlay(const std::string& lastResponse, const std::string& lastError)
{
	HsRenderOverlayUi(
		cvarManager.get(),
		lastResponse,
		lastError,
		[this]() { TriggerManualUpload(); },
		[this]() { ExecuteHistoryWindowCommand(); }
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

void Hardstuck::RenderHistoryWindow()
{
	if (!backend_)
	{
		return;
	}
	HistorySnapshot snapshot;
	std::string errorMessage;
	bool loading = false;
	std::chrono::system_clock::time_point lastFetched;
	backend_->SnapshotHistory(snapshot, errorMessage, loading, lastFetched);
	HsRenderHistoryWindowUi(snapshot, errorMessage, loading, lastFetched, &showHistoryWindow_);
}

void Hardstuck::RenderSettings()
{
	if (!BindImGuiContext())
	{
		return;
	}

	// detect base URL changes and apply to backend
	if (settingsService_ && backend_)
	{
		std::string currentBase = settingsService_->GetBaseUrl();
		if (currentBase != cachedBaseUrl_)
		{
			DiagnosticLogger::Log(std::string("Base URL changed from ") + cachedBaseUrl_ + " to " + currentBase);
			cachedBaseUrl_ = currentBase;
			backend_->SetApiBaseUrl(currentBase);
		}
	}

	HsRenderSettingsUi(
		settingsService_.get(),
		cvarManager.get(),
		[this]() { TriggerManualUpload(); }
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
	if (!HsCollectMatchPayloadComponents(server, settingsService_.get(), components, playlistMmrId))
	{
		DiagnosticLogger::Log("CaptureServerAndStageDelayedUpload: failed to collect match components");
		return false;
	}

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
	const std::string payload = HsBuildMatchPayload(server, gameWrapper.get(), settingsService_.get());
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
	return backend_->UploadMmrSnapshot(contextTag);
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

	if (!BindImGuiContext())
	{
		return;
	}

	if (showHistoryWindow_)
	{
		RenderHistoryWindow();
	}
	if (!menuOpen_)
	{
		return;
	}

	RenderOverlay(lastResponse, lastError);
	if (showHistoryWindow_)
	{
		RenderHistoryWindow();
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
