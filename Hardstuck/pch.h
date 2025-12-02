#pragma once

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include "bakkesmod/plugin/bakkesmodplugin.h"

#include <string>
#include <vector>
#include <functional>
#include <memory>

#include "imgui/imgui.h"
#include "imgui/imgui_stdlib.h"
#include "IMGUI/imgui_searchablecombo.h"
#include "IMGUI/imgui_rangeslider.h"

#include "logging.h"

// Common plugin headers used by migrated code
#include "settings/ISettingsService.h"
#include "diagnostics/DiagnosticLogger.h"
#include "utils/HsUtils.h"
#include "backend/HsBackend.h"
#include "storage/LocalDataStore.h"
#include "payload/HsPayloadBuilder.h"
#include "history/HistoryTypes.h"
#include "ui/HsOverlayUi.h"
#include "ui/HsSettingsUi.h"
#include "ui/HsHistoryWindowUi.h"
