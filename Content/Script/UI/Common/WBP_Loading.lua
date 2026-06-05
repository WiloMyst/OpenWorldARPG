-- 加载界面交互逻辑
-- 对应蓝图: /Game/UI/Common/WBP_Loading
-- 替代 C++ ULoadingScreenWidget 的交互逻辑

local Utils = require("Common.Utils")

---@type WBP_Loading_C
local M = UnLua.Class()

--- 缓存的 Subsystem 引用
M.AssetManagerSubsystem = nil

function M:Construct()
    -- 缓存 Subsystem，避免在 Tick 中每帧查找
    self.AssetManagerSubsystem = Utils.GetAssetManager()
end

--- 对应 C++ NativeTick
---@param MyGeometry FGeometry
---@param InDeltaTime float
function M:Tick(MyGeometry, InDeltaTime)
    if self.AssetManagerSubsystem then
        local Progress = self.AssetManagerSubsystem:GetTotalLoadingProgress()
        self.LoadingProgress = Progress

        if self.LoadingProgressBar then
            self.LoadingProgressBar:SetPercent(Progress)
        end
    end
end

--- 对应 C++ GetProgressBarPercent
---@return number
function M:GetProgressBarPercent()
    return self.LoadingProgress or 0.0
end

return M
