-- 全局事件总线
-- 用于跨 Widget、跨模块的松耦合通信

local M = {}

--- 事件监听表
M.Listeners = {}

--- 注册事件监听
---@param EventName string 事件名
---@param Key string 唯一标识 (通常用 Widget 名)
---@param Callback function 回调函数
function M.On(EventName, Key, Callback)
    if not M.Listeners[EventName] then
        M.Listeners[EventName] = {}
    end
    M.Listeners[EventName][Key] = Callback
end

--- 注销事件监听
---@param EventName string 事件名
---@param Key string 唯一标识
function M.Off(EventName, Key)
    if M.Listeners[EventName] then
        M.Listeners[EventName][Key] = nil
    end
end

--- 广播事件
---@param EventName string 事件名
---@param ... any 参数
function M.Emit(EventName, ...)
    if M.Listeners[EventName] then
        for _, Callback in pairs(M.Listeners[EventName]) do
            Callback(...)
        end
    end
end

--- 清空所有监听
function M.ClearAll()
    M.Listeners = {}
end

return M
