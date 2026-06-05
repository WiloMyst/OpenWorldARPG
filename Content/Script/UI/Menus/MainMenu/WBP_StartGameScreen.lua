-- 开始游戏界面交互逻辑
-- 对应蓝图: /Game/UI/Menus/MainMenu/WBP_StartGameScreen
-- 替代 C++ UStartGameScreenWidget 的交互逻辑

local Define = require("Common.Define")

---@type WBP_StartGameScreen_C
local M = UnLua.Class()

function M:Construct()
    if self.StartButton then
        self.StartButton.OnClicked:Add(self, M.HandleStartButtonClicked)
    end
end

function M:Destruct()
    if self.StartButton then
        self.StartButton.OnClicked:Remove(self, M.HandleStartButtonClicked)
    end
end

--- 处理开始游戏按钮点击
function M:HandleStartButtonClicked()
    -- 广播蓝图委托
    if self.OnStartButtonClicked then
        self.OnStartButtonClicked:Broadcast()
    end
    -- 事件总线通知
    local EventBus = require("Common.EventBus")
    EventBus.Emit(Define.Events.StartGameClicked)
end

return M
