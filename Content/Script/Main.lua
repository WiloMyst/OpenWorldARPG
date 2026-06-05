-- Lua 入口脚本
-- 在 UnLuaSettings 中设置 StartupModuleName = "Main"
-- 此文件负责初始化全局模块、注册事件监听

local EventBus = require("Common.EventBus")
local Define = require("Common.Define")

local M = {}

--- 初始化
function M.Initialize()
    print("[Main.lua] OpenWorldARPG Lua 环境初始化完成")

    -- 注册全局事件监听
    EventBus.On(Define.Events.LoginClicked, "Main", function()
        print("[Main.lua] 登录按钮点击")
    end)

    EventBus.On(Define.Events.StartGameClicked, "Main", function()
        print("[Main.lua] 开始游戏按钮点击")
    end)
end

return M
