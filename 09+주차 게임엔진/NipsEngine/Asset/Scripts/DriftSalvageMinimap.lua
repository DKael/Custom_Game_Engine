-- DriftSalvageMinimap.lua

local Minimap = {}

local MINIMAP_SIZE = 260
local MINIMAP_RELATIVE_X = 0.86
local MINIMAP_RELATIVE_Y = 0.20
local MINIMAP_RANGE_HALF = 75.0
local MINIMAP_EDGE_PADDING = 8
local MINIMAP_DOT_SIZE = 8
local MINIMAP_ICON_SIZE = 24
local MINIMAP_BG_PNG = "Asset/Mesh/MiniMap.png"
local MINIMAP_BOAT_PNG = "Asset/Texture/UI/Icon_boat.png"
local MINIMAP_LIGHTHOUSE_PNG = "Asset/Texture/UI/Icon_lighthouse.png"
local MINIMAP_LIGHTHOUSE_ICON_SIZE = 30
local MINIMAP_LIGHTHOUSE_TAGS = { "Lighthouse", "LightHouse" }

local MINIMAP_TAG_COLORS = {
    Trash      = { 0.7, 0.7, 0.7 },
    Resource   = { 0.3, 0.8, 1.0 },
    Recyclable = { 0.3, 0.9, 0.3 },
    Premium    = { 1.0, 0.85, 0.2 },
    Hazard     = { 1.0, 0.25, 0.25 },
    Rock       = { 0.45, 0.65, 0.9 },
    Tire       = { 0.0, 0.0, 0.0 },
}

local MINIMAP_DOT_TAGS = { "Rock", "Tire", "Trash", "Resource", "Recyclable", "Premium", "Hazard" }

local function WorldDeltaToBoatLocal(boatPos, targetPos, boatYawDegrees)
    local dx = targetPos.X - boatPos.X
    local dy = targetPos.Y - boatPos.Y
    local yaw = math.rad(boatYawDegrees or 0.0)
    local cosYaw = math.cos(yaw)
    local sinYaw = math.sin(yaw)

    local localForward = dx * cosYaw + dy * sinYaw
    local localRight = -dx * sinYaw + dy * cosYaw
    return localForward, localRight
end

local function FindFirstActorByTags(tags)
    if not FindActorByTag then
        return nil
    end

    for _, tag in ipairs(tags) do
        local actor = FindActorByTag(tag)
        if actor and actor:IsValid() then
            return actor
        end
    end

    return nil
end

local function ProjectToMinimap(boatPos, targetPos, boatYawDegrees)
    local localForward, localRight = WorldDeltaToBoatLocal(boatPos, targetPos, boatYawDegrees)
    local half = MINIMAP_SIZE * 0.5
    local px = (localRight / MINIMAP_RANGE_HALF) * half
    local py = -(localForward / MINIMAP_RANGE_HALF) * half

    if math.abs(px) > half or math.abs(py) > half then
        return nil, nil
    end

    return px, py
end

local function ProjectToMinimapClamped(boatPos, targetPos, boatYawDegrees, iconSize)
    local localForward, localRight = WorldDeltaToBoatLocal(boatPos, targetPos, boatYawDegrees)
    local half = MINIMAP_SIZE * 0.5
    local px = (localRight / MINIMAP_RANGE_HALF) * half
    local py = -(localForward / MINIMAP_RANGE_HALF) * half
    local iconHalf = (iconSize or 0) * 0.5
    local limit = half - MINIMAP_EDGE_PADDING - iconHalf
    local maxAxis = math.max(math.abs(px), math.abs(py))
    local outOfRange = false

    if maxAxis > limit then
        local scale = limit / maxAxis
        px = px * scale
        py = py * scale
        outOfRange = true
    end

    return px, py, outOfRange
end

function Minimap.Create(deps)
    local state = {
        Panel = nil,
        BoatIcon = nil,
        LighthouseIcon = nil,
        DotPool = {},
        DotCount = 0,
        ResolveBoatActor = deps.ResolveBoatActor,
        GetActorYawDegrees = deps.GetActorYawDegrees,
    }

    local function AcquireDot(r, g, b)
        state.DotCount = state.DotCount + 1
        local dot = state.DotPool[state.DotCount]

        if not dot then
            dot = UIManager.CreateImage(state.Panel, 0, 0, MINIMAP_DOT_SIZE, MINIMAP_DOT_SIZE, nil, "Centered")
            state.DotPool[state.DotCount] = dot
        end

        dot:SetVisible(true)
        dot:SetColor(r, g, b, 1.0)
        return dot
    end

    local function HideUnusedDots()
        for i = state.DotCount + 1, #state.DotPool do
            local dot = state.DotPool[i]
            if dot then dot:SetVisible(false) end
        end
    end

    function state:Show()
        if self.Panel then return end

        self.Panel = UIManager.CreateImage(nil,
            MINIMAP_RELATIVE_X, MINIMAP_RELATIVE_Y,
            MINIMAP_SIZE, MINIMAP_SIZE,
            MINIMAP_BG_PNG, "RelativePos")
        self.Panel:SetColor(1.0, 1.0, 1.0, 1.0)

        self.LighthouseIcon = UIManager.CreateImage(self.Panel,
            0, 0, MINIMAP_LIGHTHOUSE_ICON_SIZE, MINIMAP_LIGHTHOUSE_ICON_SIZE,
            MINIMAP_LIGHTHOUSE_PNG, "Centered")
        self.LighthouseIcon:SetColor(1.0, 0.9, 0.2, 1.0)
        self.LighthouseIcon:SetVisible(false)

        self.BoatIcon = UIManager.CreateImage(self.Panel,
            0, 0, MINIMAP_ICON_SIZE, MINIMAP_ICON_SIZE,
            MINIMAP_BOAT_PNG, "Centered")
        self.BoatIcon:SetColor(1.0, 1.0, 1.0, 1.0)
    end

    function state:Hide()
        if self.Panel then
            UIManager.DestroyElement(self.Panel)
        end

        self.Panel = nil
        self.BoatIcon = nil
        self.LighthouseIcon = nil
        self.DotPool = {}
        self.DotCount = 0
    end

    function state:Update()
        if not self.Panel then return end

        local boat = self.ResolveBoatActor()
        if not boat then
            self.DotCount = 0
            HideUnusedDots()
            if self.BoatIcon then self.BoatIcon:SetVisible(false) end
            if self.LighthouseIcon then self.LighthouseIcon:SetVisible(false) end
            return
        end

        local boatPos = boat:GetPosition()
        local boatYaw = self.GetActorYawDegrees(boat)

        if self.BoatIcon then
            self.BoatIcon:SetVisible(true)
            self.BoatIcon:SetPosition(0, 0)
            self.BoatIcon:SetRotation(0.0)
        end

        local lighthouse = FindFirstActorByTags(MINIMAP_LIGHTHOUSE_TAGS)
        if lighthouse then
            local lpos = lighthouse:GetPosition()
            local px, py, outOfRange = ProjectToMinimapClamped(boatPos, lpos, boatYaw, MINIMAP_LIGHTHOUSE_ICON_SIZE)
            if self.LighthouseIcon then
                self.LighthouseIcon:SetVisible(true)
                self.LighthouseIcon:SetPosition(px, py)
                if outOfRange then
                    self.LighthouseIcon:SetColor(1.0, 0.9, 0.2, 0.7)
                else
                    self.LighthouseIcon:SetColor(1.0, 0.9, 0.2, 1.0)
                end
            end
        elseif self.LighthouseIcon then
            self.LighthouseIcon:SetVisible(false)
        end

        self.DotCount = 0
        if FindActorsByTag then
            for _, tag in ipairs(MINIMAP_DOT_TAGS) do
                local color = MINIMAP_TAG_COLORS[tag]
                if color then
                    local actors = FindActorsByTag(tag)
                    if actors then
                        for _, actor in ipairs(actors) do
                            if actor and actor:IsValid() then
                                local pos = actor:GetPosition()
                                local px, py = ProjectToMinimap(boatPos, pos, boatYaw)
                                if px then
                                    local dot = AcquireDot(color[1], color[2], color[3])
                                    dot:SetPosition(px, py)
                                end
                            end
                        end
                    end
                end
            end
        end

        HideUnusedDots()
    end

    return state
end

return Minimap
