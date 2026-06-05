-- 玩家面板交互逻辑
-- 对应蓝图: /Game/UI/Menus/PlayerPanel/WBP_PlayerPanel
-- 替代 C++ UPlayerPanelWidget 的交互逻辑

local Utils = require("Common.Utils")

---@type WBP_PlayerPanel_C
local M = UnLua.Class()

function M:Construct()
    if self.Button_ClosePlayerPanel then
        self.Button_ClosePlayerPanel.OnClicked:Add(self, M.HandleCloseClicked)
    end
end

function M:Destruct()
    if self.Button_ClosePlayerPanel then
        self.Button_ClosePlayerPanel.OnClicked:Remove(self, M.HandleCloseClicked)
    end
end

--- 关闭面板
function M:HandleCloseClicked()
    local UIManager = Utils.GetUIManager()
    if UIManager then
        UIManager:CloseTopUI()
    end
end

return M
