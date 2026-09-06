-- ScoreManager.lua
-- Keeps the top 3 record scores for the title Record UI.

local ScoreManager = {}

local MAX_RECORDS = 3
local RECORD_FILE_PATH = "Saves/ScoreRecords.txt"
local DEFAULT_RECORDS = { 0, 0, 0 }

_G.__DriftSalvageScoreRecords = _G.__DriftSalvageScoreRecords or {}
local Records = _G.__DriftSalvageScoreRecords
local bLoadedFromFile = false

local function NormalizeScore(score)
    return math.max(0, math.floor(tonumber(score) or 0))
end

local function SortRecords()
    table.sort(Records, function(a, b)
        return NormalizeScore(a) > NormalizeScore(b)
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
        Records[i] = NormalizeScore(newRecords[i])
    end

    SortRecords()
end

local function SerializeRecords()
    SortRecords()

    local lines = {}
    for i = 1, MAX_RECORDS do
        lines[i] = tostring(NormalizeScore(Records[i]))
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
            for value in string.gmatch(content, "%d+") do
                table.insert(loadedRecords, NormalizeScore(value))
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
        result[i] = NormalizeScore(Records[i])
    end
    return result
end

function ScoreManager.RecordScore(score)
    ScoreManager.Load()
    table.insert(Records, NormalizeScore(score))
    SortRecords()
    ScoreManager.Save()
    return ScoreManager.GetRecords()
end

return ScoreManager
