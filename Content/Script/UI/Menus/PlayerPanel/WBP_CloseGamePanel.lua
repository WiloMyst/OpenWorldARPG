-- 关闭游戏确认面板交互逻辑
-- 对应蓝图: /Game/UI/Menus/PlayerPanel/WBP_CloseGamePanel
-- 替代 C++ UCloseGamePanelWidget 的交互逻辑

local Utils = require("Common.Utils")

---@type WBP_CloseGamePanel_C
local M = UnLua.Class()

function M:Construct()
    if self.Button_Cancel then
        self.Button_Cancel.OnClicked:Add(self, M.HandleCancelClicked)
    end
    if self.Button_Confirm then
        self.Button_Confirm.OnClicked:Add(self, M.HandleConfirmClicked)
    end
end

function M:Destruct()
    if self.Button_Cancel then
        self.Button_Cancel.OnClicked:Remove(self, M.HandleCancelClicked)
    end
    if self.Button_Confirm then
        self.Button_Confirm.OnClicked:Remove(self, M.HandleConfirmClicked)
    end
end

--- 取消关闭 -> 关闭当前 UI
function M:HandleCancelClicked()
    local UIManager = Utils.GetUIManager()
    if UIManager then
        UIManager:CloseTopUI()
    end
end

--- 确认关闭 -> 退出游戏
function M:HandleConfirmClicked()
    local PC = self:GetOwningPlayer()
    UE.UKismetSystemLibrary.QuitGame(self, PC, UE.EQuitPreference.Quit, false)
end

return M
