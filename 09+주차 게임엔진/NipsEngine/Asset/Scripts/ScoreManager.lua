-- ScoreManager.lua
-- Keeps the top 3 finish times for the title Record UI.

local ScoreManager = {}

local MAX_RECORDS = 3
local RECORD_FILE_PATH = "Saves/TimeRecords.txt"
local DEFAULT_RECORDS = { 0, 0, 0 }

_G.__DriftSalvageTimeRecords = _G.__DriftSalvageTimeRecords or {}
local Records = _G.__DriftSalvageTimeRecords
local bLoadedFromFile = false

local function NormalizeTime(seconds)
    local value = tonumber(seconds) or 0.0
    if value <= 0.0 then
        return 0.0
    end

    return math.floor(value * 100.0 + 0.5) / 100.0
end

local function HasRecord(seconds)
    return NormalizeTime(seconds) > 0.0
end

local function SortRecords()
    table.sort(Records, function(a, b)
        local timeA = NormalizeTime(a)
        local timeB = NormalizeTime(b)
        local hasA = HasRecord(timeA)
        local hasB = HasRecord(timeB)

        if hasA and hasB then
            return timeA < timeB
        end

        if hasA ~= hasB then
            return hasA
        end

        return false
    end)

    while #Records > MAX_RECORDS do
        table.remove(Records)
    end

    while #Records < MAX_RECORDS do
        table.insert(Records, 0)
    end
end

local function ReplaceRecords(newRecords)
    for i = #Records, 1, -1 do
        Records[i] = nil
    end

    for i = 1, #newRecords do
        Records[i] = NormalizeTime(newRecords[i])
    end

    SortRecords()
end

local function SerializeRecords()
    SortRecords()

    local lines = {}
    for i = 1, MAX_RECORDS do
        lines[i] = string.format("%.2f", NormalizeTime(Records[i]))
    end
    return table.concat(lines, "\n") .. "\n"
end

function ScoreManager.Save()
    if WriteTextFile then
        return WriteTextFile(RECORD_FILE_PATH, SerializeRecords())
    end
    return false
end

function ScoreManager.Load()
    if bLoadedFromFile then
        return ScoreManager.GetRecords()
    end

    local loadedRecords = {}
    if ReadTextFile then
        local content = ReadTextFile(RECORD_FILE_PATH)
        if content and content ~= "" then
            for value in string.gmatch(content, "[^\r\n]+") do
                table.insert(loadedRecords, NormalizeTime(value))
            end
        end
    end

    if #loadedRecords == 0 then
        loadedRecords = DEFAULT_RECORDS
    end

    ReplaceRecords(loadedRecords)
    bLoadedFromFile = true
    ScoreManager.Save()
    return ScoreManager.GetRecords()
end

function ScoreManager.GetRecords()
    if not bLoadedFromFile then
        return ScoreManager.Load()
    end

    SortRecords()

    local result = {}
    for i = 1, MAX_RECORDS do
        result[i] = NormalizeTime(Records[i])
    end
    return result
end

function ScoreManager.RecordTime(seconds)
    ScoreManager.Load()
    local normalized = NormalizeTime(seconds)
    if normalized > 0.0 then
        table.insert(Records, normalized)
        SortRecords()
        ScoreManager.Save()
    end
    return ScoreManager.GetRecords()
end

function ScoreManager.RecordScore(score)
    return ScoreManager.RecordTime(score)
end

return ScoreManager
