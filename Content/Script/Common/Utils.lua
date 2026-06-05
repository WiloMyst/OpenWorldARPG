-- 公共工具函数库

local M = {}

--- 安全获取 Subsystem
---@param SubsystemClass string UE Subsystem 类名 (如 "UInventoryManagerSubsystem")
---@return userdata|nil
function M.GetSubsystem(SubsystemClass)
    local GI = UE.UGameInstance
    if not GI then return nil end
    -- UnLua 中通过 GetGameInstance() 获取
    local GameInstance = UE.UKismetSystemLibrary.GetGameInstance(nil)
    if not GameInstance then return nil end
    return GameInstance:GetSubsystem(SubsystemClass)
end

--- 安全获取 UIManagerSubsystem
function M.GetUIManager()
    return M.GetSubsystem(UE.UUIManagerSubsystem)
end

--- 安全获取 InventoryManagerSubsystem
function M.GetInventoryManager()
    return M.GetSubsystem(UE.UInventoryManagerSubsystem)
end

--- 安全获取 GameAssetManagerSubsystem
function M.GetAssetManager()
    return M.GetSubsystem(UE.UGameAssetManagerSubsystem)
end

return M
