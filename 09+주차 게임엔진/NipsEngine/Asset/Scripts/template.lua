function BeginPlay()
    print("[BeginPlay] " .. obj.UUID)
end

function EndPlay()
    print("[EndPlay] " .. obj.UUID)
end

function Tick(dt)
    -- Actor-specific logic here.
end

function OnOverlap(OtherActor)
    -- Example:
    -- print("[Overlap] " .. obj.UUID .. " with " .. OtherActor.UUID)
end

function OnHit(OtherActor)
    -- Actor-specific hit logic here.
end
