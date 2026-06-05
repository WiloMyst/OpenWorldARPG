-- 物品格子面板交互逻辑
-- 对应蓝图: /Game/Gameplay/InventoryManager/Blueprints/WBP_ItemSlotPanel
-- 替代 C++ UItemSlotPanelWidget 的交互逻辑
-- v2: GUID 驱动 + 排序/筛选接口

local Utils = require("Common.Utils")

---@type WBP_ItemSlotPanel_C
local M = UnLua.Class()

--- 状态变量
M.SelectedItemGUID = nil
M.SelectedItemInstance = nil
M.SelectedItemSlot = nil

function M:Construct()
    local InventoryManager = Utils.GetInventoryManager()
    if InventoryManager then
        InventoryManager.OnInventoryUpdated:Add(self, M.RefreshInventoryGrid)
    end

    -- 不在 Construct 中主动刷新！
    -- 初始刷新由 InventoryWidget::HandleSelectCategoryTab → RefreshInventoryGrid 触发
    -- 避免在 ItemCategory 未设置时创建空白格子
end

function M:Destruct()
    local InventoryManager = Utils.GetInventoryManager()
    if InventoryManager then
        InventoryManager.OnInventoryUpdated:Remove(self, M.RefreshInventoryGrid)
    end
end

--- 刷新背包格子 (使用 GetItemsByFilter 排序+筛选)
function M:RefreshInventoryGrid()
    if not self.ItemWrapBox or not self.ItemSlotClass then return end

    local InventoryManager = Utils.GetInventoryManager()
    if not InventoryManager then return end

    self.ItemWrapBox:ClearChildren()
    self.SelectedItemSlot = nil
    self.SelectedItemGUID = nil

    -- 使用新的排序+筛选接口
    local SortMode = self.SortMode or UE.EItemSortMode.ByRarity
    local RarityFilter = self.RarityFilter or UE.EItemRarity.Star1
    local FilteredItems = {}
    InventoryManager:GetItemsByFilter(self.ItemCategory, SortMode, RarityFilter, FilteredItems)

    for i = 0, #FilteredItems - 1 do
        local Instance = FilteredItems[i + 1]  -- Lua 1-based indexing

        local NewSlot = UE.UWidgetBlueprintLibrary.Create(self, self.ItemSlotClass)
        if NewSlot then
            NewSlot.ItemInstance = Instance
            NewSlot.ItemArrayIndex = i
            NewSlot.OnSlotClicked:Add(self, M.HandleSelectedSlot)
            self.ItemWrapBox:AddChildToWrapBox(NewSlot)
        end
    end

    self:HandleSelectFirstSlot()
end

--- 选中第一个格子
function M:HandleSelectFirstSlot()
    if self.ItemWrapBox and self.ItemWrapBox:GetChildrenCount() > 0 then
        local FirstSlot = self.ItemWrapBox:GetChildAt(0)
        if FirstSlot then
            self:HandleSelectedSlot(
                FirstSlot.ItemArrayIndex,
                FirstSlot.ItemInstance,
                FirstSlot:GetCachedItemData(),
                FirstSlot
            )
        end
    end
end

--- 处理格子选中 (GUID 驱动)
---@param Index int32
---@param Instance FItemInstance
---@param Data FItemData
---@param SlotWidget UItemSlotWidget
function M:HandleSelectedSlot(Index, Instance, Data, SlotWidget)
    -- 使用 GUID 作为选中标识
    self.SelectedItemGUID = Instance.ItemGUID
    self.SelectedItemInstance = Instance

    if self.SelectedItemSlot then
        self.SelectedItemSlot:SetSelectionState(false)
    end

    self.SelectedItemSlot = SlotWidget
    if self.SelectedItemSlot then
        self.SelectedItemSlot:SetSelectionState(true)
    end

    if self.OnItemSelectedInGrid and self.OnItemSelectedInGrid:IsBound() then
        self.OnItemSelectedInGrid:Broadcast(self.SelectedItemGUID, Instance.ItemID, self.SelectedItemInstance)
    end
end

--- 切换排序模式
---@param NewSortMode EItemSortMode
function M:SetSortMode(NewSortMode)
    self.SortMode = NewSortMode
    self:RefreshInventoryGrid()
end

--- 切换稀有度筛选
---@param NewFilter EItemRarity
function M:SetRarityFilter(NewFilter)
    self.RarityFilter = NewFilter
    self:RefreshInventoryGrid()
end

return M
