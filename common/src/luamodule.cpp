#include <ctime>
#include <chrono>
#include <thread>
#include "log.hpp"
#include "uidf.hpp"
#include "luaf.hpp"
#include "totype.hpp"
#include "sysconst.hpp"
#include "fflerror.hpp"
#include "luamodule.hpp"
#include "raiitimer.hpp"
#include "dbcomid.hpp"

LuaModule::LuaModule()
    : m_luaState()
    , m_replaceEnv(m_luaState, sol::create)
{
    m_luaState.open_libraries();
    execRawString(R"###(
        local _G = _G
        local error = error
        local coroutine = coroutine
        local RESERVED_NAME_G_sandbox = {}

        function getTLSTable()
            local threadId, inMainThread = coroutine.running()
            if inMainThread then
                error('call getTLSTable() in main thread')
            else
                if RESERVED_NAME_G_sandbox[threadId] == nil then
                    RESERVED_NAME_G_sandbox[threadId] = {}
                end
                return RESERVED_NAME_G_sandbox[threadId]
            end
        end

        function clearTLSTable()
            local threadId, inMainThread = coroutine.running()
            if inMainThread then
                error('call clearTLSTable() in main thread')
            else
                RESERVED_NAME_G_sandbox[threadId] = nil
            end
        end

        RESERVED_NAME_replaceEnvMetaTable = {
            __index = function(_, key)
                local threadId, inMainThread = coroutine.running()
                if not inMainThread then
                    if RESERVED_NAME_G_sandbox[threadId] ~= nil and RESERVED_NAME_G_sandbox[threadId][key] ~= nil then
                        return RESERVED_NAME_G_sandbox[threadId][key]
                    end
                end
                return _G[key]
            end,

            __newindex = function(_, key, value)
                local threadId, inMainThread = coroutine.running()
                if inMainThread then
                    _G[key] = value
                else
                    if RESERVED_NAME_G_sandbox[threadId] == nil then
                        RESERVED_NAME_G_sandbox[threadId] = {}
                    end
                    RESERVED_NAME_G_sandbox[threadId][key] = value
                end
            end
        }
    )###");

    m_replaceEnv[sol::metatable_key] = sol::table(m_luaState["RESERVED_NAME_replaceEnvMetaTable"]);

    // idea from: https://blog.rubenwardy.com/2020/07/26/sol3-script-sandbox/
    // set replaceEnv as default environment, otherwise I don't know how to setup replaceEnv to thread/coroutine

    lua_rawgeti(m_luaState.lua_state(), LUA_REGISTRYINDEX, m_replaceEnv.registry_index());
    lua_rawseti(m_luaState.lua_state(), LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS);

    execString("LOGTYPE_INFO    = 0");
    execString("LOGTYPE_WARNING = 1");
    execString("LOGTYPE_FATAL   = 2");
    execString("LOGTYPE_DEBUG   = 3");

    execString("UID_NONE  = %d", UID_NONE );
    execString("UID_BEGIN = %d", UID_BEGIN);
    execString("UID_COR   = %d", UID_COR  );
    execString("UID_MAP   = %d", UID_MAP  );
    execString("UID_NPC   = %d", UID_NPC  );
    execString("UID_MON   = %d", UID_MON  );
    execString("UID_PLY   = %d", UID_PLY  );
    execString("UID_RCV   = %d", UID_RCV  );
    execString("UID_END   = %d", UID_END  );

    execString("INVOP_TRADE  = %d", INVOP_TRADE );
    execString("INVOP_SECURE = %d", INVOP_SECURE);
    execString("INVOP_REPAIR = %d", INVOP_REPAIR);

    execString("DIR_UP        = %d", DIR_UP       );
    execString("DIR_UPRIGHT   = %d", DIR_UPRIGHT  );
    execString("DIR_RIGHT     = %d", DIR_RIGHT    );
    execString("DIR_DOWNRIGHT = %d", DIR_DOWNRIGHT);
    execString("DIR_DOWN      = %d", DIR_DOWN     );
    execString("DIR_DOWNLEFT  = %d", DIR_DOWNLEFT );
    execString("DIR_LEFT      = %d", DIR_LEFT     );
    execString("DIR_UPLEFT    = %d", DIR_UPLEFT   );

    execString("SYS_DEBUG = %s", to_boolcstr(SYS_DEBUG));

    execString("SYS_NPCINIT  = \"%s\"", SYS_NPCINIT );
    execString("SYS_NPCDONE  = \"%s\"", SYS_NPCDONE );
    execString("SYS_NPCQUERY = \"%s\"", SYS_NPCQUERY);
    execString("SYS_NPCERROR = \"%s\"", SYS_NPCERROR);
    execString("math.randomseed(%d)", to_d(hres_tstamp().to_nsec() % 1000000ULL));

    bindFunction("addLogString", [this](sol::object logType, sol::object logInfo)
    {
        if(logType.is<int>() && logInfo.is<std::string>()){
            addLogString(logType.as<int>(), to_u8cstr(logInfo.as<std::string>()));
            return;
        }

        if(logType.is<int>()){
            addLogString(1, to_u8cstr(str_printf("Invalid argument: addLogString(%d, \"?\")", logType.as<int>())));
            return;
        }

        if(logInfo.is<std::string>()){
            addLogString(1, to_u8cstr(str_printf("Invalid argument: addLogString(?, \"%s\")", logInfo.as<std::string>().c_str())));
            return;
        }

        addLogString(1, u8"Invalid argument: addLogString(?, \"?\")");
    });

    bindFunction("getTime", [timer = hres_timer()]() -> int
    {
        return to_d(timer.diff_msec());
    });

    bindFunction("getNanoTstamp", []() -> std::string
    {
        return std::to_string(hres_tstamp().to_nsec());
    });

    bindFunction("getAbsTime", []() -> int
    {
        return to_d(std::time(nullptr));
    });

    m_luaState.script(BEGIN_LUAINC(char)
#include "luamodule.lua"
    END_LUAINC());

    bindFunction("getUIDType", [](uint64_t uid)
    {
        return uidf::getUIDType(uid);
    });

    bindFunction("sleep", [](int nSleepMS)
    {
        if(nSleepMS > 0){
            std::this_thread::sleep_for(std::chrono::milliseconds(nSleepMS));
        }
    });

    bindFunction("exit", [](int exitCode)
    {
        std::exit(exitCode);
    });

    bindFunction("getItemName", [](int itemID, sol::this_state s) -> sol::object
    {
        sol::state_view sv(s);
        if(const auto name = DBCOM_ITEMRECORD(itemID).name; str_haschar(name)){
            return sol::object(sv, sol::in_place_type<std::string>, std::string(to_cstr(name)));
        }
        return sol::make_object(sv, sol::nil);
    });

    bindFunction("getItemID", [](std::string itemName) -> int
    {
        return DBCOM_ITEMID(to_u8cstr(itemName));
    });

    bindFunction("getMonsterName", [](int monsterID, sol::this_state s) -> sol::object
    {
        sol::state_view sv(s);
        if(const auto name = DBCOM_MONSTERRECORD(monsterID).name; str_haschar(name)){
            return sol::object(sv, sol::in_place_type<std::string>, std::string(to_cstr(name)));
        }
        return sol::make_object(sv, sol::nil);
    });

    bindFunction("getMonsterID", [](std::string monsterName) -> int
    {
        return DBCOM_MONSTERID(to_u8cstr(monsterName));
    });

    bindFunction("getMapName", [](int mapID) -> std::string
    {
        return to_cstr(DBCOM_MAPRECORD(mapID).name);
    });

    bindFunction("getMapID", [](std::string mapName) -> int
    {
        return DBCOM_MAPID(to_u8cstr(mapName));
    });

    bindFunction("randString", [this](sol::variadic_args args) -> std::string
    {
        // generate random string
        // for debug purpose of utf8 layout board

        int length = 0;
        std::string alphabet;

        const std::vector<sol::object> argList(args.begin(), args.end());
        switch(argList.size()){
            case 1:
                {
                    if(!argList[0].is<int>()){
                        throw fflerror("Invalid argument: randString(length: int, [alphabet: string])");
                    }

                    length = argList[0].as<int>();
                    alphabet = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
                    break;
                }

            case 2:
                {
                    if(!(argList[0].is<int>() && argList[1].is<std::string>())){
                        throw fflerror("Invalid argument: randString(length: int, [alphabet: string])");
                    }

                    length = argList[0].as<int>();
                    alphabet = argList[1].as<std::string>();
                    break;
                }
            default:
                {
                    throw fflerror("Invalid argument: randString(length: int, [alphabet: string])");
                }
        }

        if(length < 0 || alphabet.empty()){
            const auto reportAlphabet = [&alphabet]() -> std::string
            {
                if(alphabet.empty()){
                    return "(empty)";
                }

                if(alphabet.length() < 5){
                    return alphabet;
                }
                return alphabet.substr(0, 3) + "...";
            }();
            throw fflerror("Invalid argument: randString(length = %d, alphabe = \'%s\')", length, reportAlphabet.c_str());
        }

        std::string result;
        for(int i = 0; i < length; ++i){
            result.push_back(alphabet[std::rand() % alphabet.length()]);
        }
        return result;
    });

    bindFunction("scalarAsString", [this](sol::object obj) -> std::string
    {
        return luaf::buildBlob<sol::object>(obj);
    });

    bindFunction("convTableAsString", [this](sol::as_table_t<luaf::conv_table> convTable) -> std::string
    {
        return luaf::buildBlob<luaf::conv_table>(convTable.value());
    });

    bindFunction("scalarFromString", [this](std::string s, sol::this_state state)
    {
        return luaf::buildLuaObj(sol::state_view(state), s);
    });

    bindFunction("convTableFromString", [this](std::string s)
    {
        return luaf::buildLuaConvTable(s);
    });

    bindFunction("asKeyString", [this](std::string s) -> std::string
    {
        return luaf::asKeyString(s);
    });

    bindFunction("fromKeyString", [this](std::string s) -> std::string
    {
        return luaf::fromKeyString(s);
    });
}

bool LuaModule::pfrCheck(const sol::protected_function_result &pfr)
{
    if(pfr.valid()){
        return true;
    }

    const sol::error err = pfr;
    std::stringstream errStream(err.what());

    std::string errStr;
    while(std::getline(errStream, errStr, '\n')){
        addLogString(Log::LOGTYPEV_WARNING, to_u8cstr(errStr));
    }
    return false;
}
