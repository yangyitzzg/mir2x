--, u8R"###(
--

-- example:
--     addLog(LOGTYPE_INFO, 'hello world')
--     addLog(LOGTYPE_INFO, 'hello world: %d', 12)
--
function addLog(logType, logString, ...)
    if type(logType) ~= 'number' or type(logString) ~= 'string' then
        error(string.format('invalid argument type: addLog(%s, %s, ...)', type(logType), type(logString)))
    end
    addLogString(logType, logString:format(...))
end

function fatalPrintf(s, ...)
    if type(s) ~= 'string' then
        error(string.format('invalid argument type: fatalPrintf(%s, ...)', type(s)))
    end
    error(s:format(...))
end

function argDefault(arg, def)
    if arg == nil then
        assert(def ~= nil)
        return def
    else
        return arg
    end
end

function assertType(var, typestr)
    if type(typestr) ~= 'string' then
        fatalPrintf('invalid type string, expect string, get %s)', type(typestr))
    end

    if type(var) == 'number' then
        if math.type(var) ~= typestr then
            fatalPrintf('assertion failed: expect %s, get %s', typestr, math.type(var))
        end
    else
        if type(var) ~= typestr then
            fatalPrintf('assertion failed: expect %s, get %s', typestr, type(var))
        end
    end
    return var
end

function assertValue(var, value)
    if var ~= value then
        fatalPrintf('assertion failed: expect [%s](%s), get [%s](%s)', type(var), tostring(value), type(value), tostring(var))
    end
    return var
end

function asString(arg)
    typeStr = type(arg)
    if typeStr == 'nil' or typeStr == 'number' or typeStr == 'boolean' or typeStr == 'string' then
        return scalarAsString(arg)
    elseif typeStr == 'table' then
        local convTable = {}
        for k, v in pairs(arg) do
            convTable[asKeyString(asString(k))] = asString(v)
        end
        return convTableAsString(convTable)
    else
        fatalPrintf('asString(%s) type not supported: %s', tostring(arg), type(arg))
    end
end

function fromString(s)
    if type(s) ~= 'string' then
        fatalPrintf('fromString(%s) expecting string, provided %s', tostring(s), type(s))
    end

    if string.sub(s, -1) == 't' then -- conv_table
        local convTable = convTableFromString(s)
        local realTable = {}
        for k, v in pairs(convTable) do
            realTable[fromString(fromKeyString(k))] = fromString(v)
        end
        return realTable
    else
        return scalarFromString(s)
    end
end

function hasChar(s)
    if s == nil then
        return false
    end

    assertType(s, 'string')
    return string.len(s) > 0
end

function asyncWait(ms)
    local start = getTime()
    while getTime() < start + ms
    do
        coroutine.yield()
    end
end

function getFileName()
    return debug.getinfo(2, 'S').source
end

function rotable(tbl, recursive)
    assertType(tbl, 'table')
    if recursive ~= nil then
        assertType(recursive, 'boolean')
    end

    local function plain_rotable(tb)
        return setmetatable({}, {
            __index = tb,
            __newindex = function()
                error("attempt to update a read-only table")
            end,

            __pairs = function()
                return next, tb, nil
            end,

            __ipairs = function()
                local function iter(t, i)
                    local j = i + 1
                    local v = t[j]
                    if v ~= nil then
                        return j, v
                    end
                end
                return iter, tbl, 0
            end,

            __len = function()
                return #tb
            end
        })
    end

    local function dfs_rotable(tb)
        for k, v in pairs(tb) do
            if type(v) == 'table' then
                tb[k] = plain_rotable(dfs_rotable(v))
            end
        end
        return plain_rotable(tb)
    end

    if (recursive == nil or recursive) then
        return dfs_rotable(tbl)
    else
        return plain_rotable(tbl)
    end
end

function getBackTraceLine()
    local info = debug.getinfo(3, "Sl")

    -- check if the backtracing info valid
    -- if not valid we return a empty string to addLog()

    if not info then
        return ""
    end

    -- if it's invoked by a C function like:
    --     LuaModule["addLog"]("hello world")
    -- then return "C_FUNC"

    if info.what == "C" then
        return "C_FUNC"
    end

    -- invoked from a lua function
    -- return the invocation layer information

    return string.format("[%s]: %d", info.short_src, info.currentline)
end

function addExtLog(logType, logInfo)

    -- add type checking here
    -- need logType as int and logInfo as string

    if type(logType) == 'number' and type(logInfo) == 'string' then
        addLog(logType, getBackTraceLine() .. ': ' .. logInfo)
        return
    end

    -- else we need to give warning
    addLog(1, 'addExtLog(logType: int, logInfo: string)')
end

--
-- )###"
