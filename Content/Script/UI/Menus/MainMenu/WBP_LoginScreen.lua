-- 登录界面交互逻辑
-- 对应蓝图: /Game/UI/Menus/MainMenu/WBP_LoginScreen
-- 替代 C++ ULoginScreenWidget 的交互逻辑

local Define = require("Common.Define")

---@type WBP_LoginScreen_C
local M = UnLua.Class()

--- 对应蓝图的 Event Construct / C++ NativeConstruct
function M:Construct()
    -- 绑定 LunchButton 的点击事件
    if self.LunchButton then
        self.LunchButton.OnClicked:Add(self, M.HandleLoginButtonClicked)
    end
end

--- 对应蓝图的 Event Destroy / C++ NativeDestruct
function M:Destruct()
    -- 解绑事件，防止内存泄漏
    if self.LunchButton then
        self.LunchButton.OnClicked:Remove(self, M.HandleLoginButtonClicked)
    end
end

--- 处理登录按钮点击
--- 对应 C++ HandleLoginButtonClicked -> OnLoginButtonClicked.Broadcast()
function M:HandleLoginButtonClicked()
    -- 广播蓝图委托
    if self.OnLoginButtonClicked then
        self.OnLoginButtonClicked:Broadcast()
    end
    -- 同时通过事件总线通知
    local EventBus = require("Common.EventBus")
    EventBus.Emit(Define.Events.LoginClicked)
end

return M
