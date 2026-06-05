-- 背包主界面交互逻辑
-- 对应蓝图: /Game/Gameplay/InventoryManager/Blueprints/WBP_Inventory
-- 替代 C++ UInventoryWidget 的交互逻辑
-- v2: GUID 驱动，对标鸣潮

local Utils = require("Common.Utils")
local Define = require("Common.Define")

---@type WBP_Inventory_C
local M = UnLua.Class()

--- 状态变量
M.SelectedCategoryTab = nil
M.CachedSelectedItemGUID = nil  -- 改用 GUID 替代 ItemID

function M:Construct()
    -- 1. 绑定按钮事件
    if self.CloseButton then
        self.CloseButton.OnClicked:Add(self, M.OnCloseButtonClicked)
    end
    if self.DiscardButton then
        self.DiscardButton.OnClicked:Add(self, M.OnDiscardButtonClicked)
    end

    -- 2. 绑定格子容器的点击委托 (GUID 驱动)
    if self.WBP_ItemSlotPanel then
        self.WBP_ItemSlotPanel.OnItemSelectedInGrid:Add(self, M.HandleOnItemSelectedInGrid)
    end

    -- 3. 初始化分类标签页
    self:RefreshCategoryTabBox()
    self:HandleSelectFirstCategoryTab()
end

function M:Destruct()
    if self.CloseButton then
        self.CloseButton.OnClicked:Remove(self, M.OnCloseButtonClicked)
    end
    if self.DiscardButton then
        self.DiscardButton.OnClicked:Remove(self, M.OnDiscardButtonClicked)
    end
    if self.WBP_ItemSlotPanel then
        self.WBP_ItemSlotPanel.OnItemSelectedInGrid:Remove(self, M.HandleOnItemSelectedInGrid)
    end
end

--- 读取数据表并生成所有分类 Tab
function M:RefreshCategoryTabBox()
    if not self.CategoryBox or not self.CategoryTabClass then return end

    -- 自动从 GameAssetManagerSubsystem 加载 CategoryDataTable
    if not self.CategoryDataTable then
        local AssetManager = Utils.GetAssetManager()
        if AssetManager then
            self.CategoryDataTable = AssetManager:GetInventoryCategoryTabDataTable()
        end
    end

    if not self.CategoryDataTable then return end

    self.CategoryBox:ClearChildren()
    self.SelectedCategoryTab = nil

    local RowMap = self.CategoryDataTable:GetRowMap()
    for _, RowData in pairs(RowMap) do
        local NewTab = UE.UWidgetBlueprintLibrary.Create(self, self.CategoryTabClass)
        if NewTab then
            NewTab.CategoryTabData = RowData
            NewTab.OnTabClicked:Add(self, M.HandleOnTabClicked)
            self.CategoryBox:AddChild(NewTab)
        end
    end
end

--- 选中首个分类 Tab
function M:HandleSelectFirstCategoryTab()
    if self.CategoryBox and self.CategoryBox:GetChildrenCount() > 0 then
        local FirstTab = self.CategoryBox:GetChildAt(0)
        if FirstTab then
            self:HandleSelectCategoryTab(FirstTab)
        end
    end
end

--- 处理 Tab 点击
---@param NewCategoryTab UItemCategoryTabWidget
---@param NewTabCategory EItemCategory
function M:HandleOnTabClicked(NewCategoryTab, NewTabCategory)
    if self.WBP_ItemSlotPanel then
        self.WBP_ItemSlotPanel.ItemCategory = NewTabCategory
    end
    self:HandleSelectCategoryTab(NewCategoryTab)
end

--- 处理分类 Tab 切换与状态重置
---@param NewCategoryTab UItemCategoryTabWidget
function M:HandleSelectCategoryTab(NewCategoryTab)
    if self.SelectedCategoryTab then
        self.SelectedCategoryTab:SetTabSelectedState(false)
    end

    self.SelectedCategoryTab = NewCategoryTab

    if self.SelectedCategoryTab then
        self.SelectedCategoryTab:SetTabSelectedState(true)
    end

    if self.WBP_ItemDetailPanel then
        self.WBP_ItemDetailPanel:SetRenderOpacity(0.0)
    end

    if self.WBP_ItemSlotPanel then
        self.WBP_ItemSlotPanel:RefreshInventoryGrid()
    end
end

--- 响应格子点击，更新详情面板 (GUID 驱动)
---@param SelectedItemGUID FGuid
---@param SelectedItemID int32
---@param SelectedItemInstance FItemInstance
function M:HandleOnItemSelectedInGrid(SelectedItemGUID, SelectedItemID, SelectedItemInstance)
    self.CachedSelectedItemGUID = SelectedItemGUID

    if self.WBP_ItemDetailPanel then
        self.WBP_ItemDetailPanel:UpdateDetails(SelectedItemInstance)
        self.WBP_ItemDetailPanel:SetRenderOpacity(1.0)
    end
end

--- 关闭背包
function M:OnCloseButtonClicked()
    local UIManager = Utils.GetUIManager()
    if UIManager then
        UIManager:CloseTopUI()
    end
end

--- 丢弃物品 (GUID 驱动，不再依赖数组索引)
function M:OnDiscardButtonClicked()
    if not self.CachedSelectedItemGUID or not self.CachedSelectedItemGUID:IsValid() then return end

    local PC = self:GetOwningPlayer()
    if PC then
        local PlayerChar = PC:GetPawn()
        if PlayerChar then
            local Backpack = PlayerChar:GetComponentByClass(UE.UBackpackComponent)
            if Backpack then
                Backpack:DropItemByGUID(self.CachedSelectedItemGUID, 1)
            end
        end
    end
end

return M
