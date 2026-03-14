#pragma once
#include <vector>
#include <string.h>
#include <fstream>
#include <json.hpp>
#include <random>

struct PlaylistLevelSetup
{
    std::string LevelName;
    std::string GameMode;
    std::string TOD;
    std::string InclusionOptions;
    std::string SettingsToApply;
    std::string Loadscreen_GamemodeName;
    std::string Loadscreen_LevelName;
    std::string Loadscreen_LevelDescription;
    std::string Loadscreen_UIAssetPath;
};

struct MixedModeConfig
{
    std::vector<std::string> AvailableModes;
    std::unordered_map<std::string, std::vector<std::string>> AvailableLevelsForModes;
    std::unordered_map<std::string, std::vector<std::string>> AvailableTODForLevels;
};

class ServerPlaylist
{
public:

    bool LoadFromFile(const char* path)
    {
        std::ifstream playlistFile(path);
        if (playlistFile.is_open())
        {
            nlohmann::json playlistJson = nlohmann::json::parse(playlistFile);

            m_setups.clear();

            m_mixedEnabled = playlistJson["IsMixed"].get<bool>();
            m_roundsPerSetup = playlistJson["RoundsPerSetup"].get<unsigned int>();

            if(playlistJson.contains("Loadscreen_GamemodeNameOverride"))
                m_loadscreenGamemodeName = playlistJson["Loadscreen_GamemodeNameOverride"].get<std::string>();

            if(playlistJson.contains("Loadscreen_LevelNameOverride"))
                m_loadscreenLevelName = playlistJson["Loadscreen_LevelNameOverride"].get<std::string>();

            if(playlistJson.contains("Loadscreen_LevelDescriptionOverride"))
                m_loadscreenLevelDescription = playlistJson["Loadscreen_LevelDescriptionOverride"].get<std::string>();
            
            if (playlistJson.contains("Loadscreen_UIAssetPathOverride"))
                m_loadscreenUIAssetPath = playlistJson["Loadscreen_UIAssetPathOverride"].get<std::string>();

            // init rng engine
            m_mtRNG = std::mt19937(m_rd());

            if (m_mixedEnabled)
            {
                std::vector<std::string> modesForMixed = playlistJson["AvailableModes"].get<std::vector<std::string>>();
                for (const auto& modeStr : modesForMixed)
                {
                    m_mixedConfig.AvailableModes.push_back(modeStr);
                }

                for (const auto& [mode, levels] : playlistJson["AvailableLevelsForModes"].items())
                {
                    m_mixedConfig.AvailableLevelsForModes[mode] = levels.get<std::vector<std::string>>();
                }

            }
            else
            {
                for (const auto& setup : playlistJson.at("PlaylistRotation"))
                {
                    PlaylistLevelSetup levelSetup;
                    levelSetup.LevelName = setup["LevelName"].get<std::string>();
                    levelSetup.GameMode = setup["GameMode"].get<std::string>();
                    setup.contains("TOD")
                        ? levelSetup.TOD = setup["TOD"].get<std::string>()
                        : levelSetup.TOD = "Day";
                    setup.contains("SettingsToApply")
                        ? levelSetup.SettingsToApply = setup["SettingsToApply"].get<std::string>()
                        : levelSetup.SettingsToApply = "";

                    setup.contains("Loadscreen_LevelName")
                        ? levelSetup.Loadscreen_LevelName = setup["Loadscreen_LevelName"].get<std::string>()
                        : levelSetup.Loadscreen_LevelName = m_loadscreenLevelName;

                    setup.contains("Loadscreen_GamemodeName")
                        ? levelSetup.Loadscreen_GamemodeName = setup["Loadscreen_GamemodeName"].get<std::string>()
                        : levelSetup.Loadscreen_GamemodeName = m_loadscreenGamemodeName;

                    setup.contains("Loadscreen_LevelDescription")
                        ? levelSetup.Loadscreen_LevelDescription = setup["Loadscreen_LevelDescription"].get<std::string>()
                        : levelSetup.Loadscreen_LevelDescription = m_loadscreenLevelDescription;

                    setup.contains("Loadscreen_UIAssetPath")
                        ? levelSetup.Loadscreen_UIAssetPath = setup["Loadscreen_UIAssetPath"].get<std::string>()
                        : levelSetup.Loadscreen_UIAssetPath = m_loadscreenUIAssetPath;


                    m_setups.push_back(levelSetup);
                }
            }

            if (playlistJson.contains("AvailableTODForLevels"))
            {
                for (const auto& [level, TODs] : playlistJson["AvailableTODForLevels"].items())
                {
                    m_mixedConfig.AvailableTODForLevels[level] = TODs.get<std::vector<std::string>>();
                }
            }

            if (!m_mixedEnabled && !m_setups.empty())
            {
                m_currentSetup = m_setups[0];
            }

            return true;
        }

        return false;
    }

    bool IsMixedMode() { return m_mixedEnabled; }

    bool AllRoundsCompletedForSetup() { return m_currentRoundsOnSetup == m_roundsPerSetup; }

    void ResetRoundCount() { m_currentRoundsOnSetup = 0; }

    void RoundCompleted()
    {
        m_currentRoundsOnSetup++;
        if (AllRoundsCompletedForSetup())
        {
            ResetRoundCount();
            m_shouldGetNewSetup = true;
        }
        else
        {
            m_shouldGetNewSetup = false;

        }
    }

    const PlaylistLevelSetup* GetMixedLevelSetup(bool forceNew)
    {
        if (!m_mixedEnabled)
            return nullptr;

        std::uniform_int_distribution<> modeDist(0, m_mixedConfig.AvailableModes.size() - 1);
pick_mode:
        const std::string& randomMode = m_mixedConfig.AvailableModes[modeDist(m_mtRNG)];
        // Check if the random pick is the same mode 
        // Need to check without the last character because of alt modes for alt coop maps, such as Domination0 and Domination1
        if (CompareStrWithoutLastCharacter(m_currentSetup.GameMode, randomMode))
        {
            goto pick_mode;
        }

        m_currentSetup.GameMode = randomMode;
        
        int numLevelsForMode = m_mixedConfig.AvailableLevelsForModes[randomMode.c_str()].size();

        std::uniform_int_distribution<> levelDist(0, numLevelsForMode - 1);
pick_level:
        const std::string& randomLevel = m_mixedConfig.AvailableLevelsForModes[randomMode][levelDist(m_mtRNG)];
        if (m_currentSetup.LevelName == randomLevel)
        {
            goto pick_level;
        }

        m_currentSetup.LevelName = randomLevel;

        const char* TODOption = "Day";
        auto it = m_mixedConfig.AvailableTODForLevels.find(m_currentSetup.LevelName);
        if (it != m_mixedConfig.AvailableTODForLevels.end())
        {
            std::uniform_int_distribution<> todDist(0, it->second.size() - 1);
            TODOption = it->second[todDist(m_mtRNG)].c_str();
        }

        char buf[128];
        sprintf_s(buf, "GameMode=%s;TOD=%s", randomMode.c_str(), TODOption);
        m_currentSetup.TOD = TODOption;
        m_shouldGetNewSetup = false;
        return &m_currentSetup;
    }

    const PlaylistLevelSetup* GetNextSetup(bool forceNew = false)
    {


        RoundCompleted();

        if (!m_shouldGetNewSetup)
            return &m_currentSetup;

        if (IsMixedMode())
            return GetMixedLevelSetup(false);

        m_currentSetupIndex++;
        if (m_currentSetupIndex >= m_setups.size())
            m_currentSetupIndex = 0;
        m_currentSetup = m_setups[m_currentSetupIndex];
        return &m_currentSetup;
    }

    const PlaylistLevelSetup* GetSetup(int index) 
    {
        if (IsMixedMode())
            return GetMixedLevelSetup(false);

        return &m_setups[index]; 
    
    }

    const PlaylistLevelSetup* GetCurrentSetup() { return &m_currentSetup; }

private:
    bool CompareStrWithoutLastCharacter(const std::string& str1, const std::string& str2)
    {
        if (str1.length() < 1 || str2.length() < 1)
        {
            return false;
        }

        return str1.substr(0, str1.length() - 1) == str2.substr(0, str2.length() - 1);
    }

    MixedModeConfig m_mixedConfig;
    std::vector<PlaylistLevelSetup> m_setups;
    PlaylistLevelSetup m_currentSetup;
    std::string m_loadscreenGamemodeName;
    std::string m_loadscreenLevelName;
    std::string m_loadscreenLevelDescription;
    std::string m_loadscreenUIAssetPath;
    unsigned int m_currentRoundsOnSetup = 0;
    unsigned int m_roundsPerSetup = 1;
    unsigned int m_currentSetupIndex = 0;
    bool m_mixedEnabled;
    bool m_shouldGetNewSetup = true;
    std::random_device m_rd;
    std::mt19937 m_mtRNG;
};