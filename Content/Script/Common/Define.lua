-- 全局常量与枚举定义
-- 与 C++ SharedTypes.h 中的枚举保持同步

local M = {}

--- 物品分类枚举 (对应 EItemCategory)
M.ItemCategory = {
    Weapon   = 0,
    Artifact = 1,
    Material = 2,
    Food     = 3,
    Quest    = 4,
}

--- 物品稀有度枚举 (对应 EItemRarity)
M.ItemRarity = {
    Star1 = 0,
    Star2 = 1,
    Star3 = 2,
    Star4 = 3,
    Star5 = 4,
}

--- Widget 输入模式枚举 (对应 EWidgetInputMode)
M.WidgetInputMode = {
    UIOnly         = 0,
    GameAndUI      = 1,
    GameOnly       = 2,
    UIOnlyNoCursor = 3,
}

--- UI 事件名常量
M.Events = {
    LoginClicked       = "LoginClicked",
    StartGameClicked   = "StartGameClicked",
    CloseGameRequested = "CloseGameRequested",
    InventoryOpened    = "InventoryOpened",
    InventoryClosed    = "InventoryClosed",
    ItemSelected       = "ItemSelected",
    ItemDiscarded      = "ItemDiscarded",
}

return M
