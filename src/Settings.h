#pragma once
#include <string>
#include <filesystem>

class UI; // forward declaration

// Helper class for settings persistence and default handling.
class SettingsManager
{
public:
  static void EnsureDataDirectoryExists();
  static void RestoreSettingsToDefaults(UI &ui);
  static bool LoadSettingsFromDisk(UI &ui);
  static bool SaveSettingsToDisk(UI &ui);
};
