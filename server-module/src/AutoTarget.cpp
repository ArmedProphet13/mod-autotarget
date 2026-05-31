/*
 * Copyright (C) 2016+ AzerothCore <www.azerothcore.org>, released under GNU AGPL v3 license: https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3
 */

#include "ConfigValueCache.h"
#include "Player.h"
#include "ScriptMgr.h"

enum class AutoTargetConfig
{
    ENABLED,

    NUM_CONFIGS,
};

class AutoTargetConfigData : public ConfigValueCache<AutoTargetConfig>
{
public:
    AutoTargetConfigData() : ConfigValueCache(AutoTargetConfig::NUM_CONFIGS) { }

    void BuildConfigCache() override
    {
        SetConfigValue<bool>(AutoTargetConfig::ENABLED, "AutoTarget.Enable", true);
    }
};

static AutoTargetConfigData gConfig;

class AutoTargetWorldScript : public WorldScript
{
public:
    AutoTargetWorldScript() : WorldScript("AutoTargetWorldScript", {
        WORLDHOOK_ON_BEFORE_CONFIG_LOAD
    }) { }

    void OnBeforeConfigLoad(bool reload) override
    {
        gConfig.Initialize(reload);
    }
};

void AddAutoTargetScripts()
{
    new AutoTargetWorldScript();
}
