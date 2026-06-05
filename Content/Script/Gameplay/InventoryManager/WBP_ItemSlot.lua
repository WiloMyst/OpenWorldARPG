-- 物品格子交互逻辑
-- 对应蓝图: /Game/Gameplay/InventoryManager/Blueprints/WBP_ItemSlot
-- 替代 C++ UItemSlotWidget 的交互逻辑

local Utils = require("Common.Utils")

---@type WBP_ItemSlot_C
local M = UnLua.Class()

--- 缓存的静态数据
M.CachedItemData = nil

function M:Construct()
    if self.SlotButton then
        self.SlotButton.OnClicked:Add(self, M.OnSlotButtonClicked)
    end
    self:UpdateSlotInfo()
end

function M:Destruct()
    if self.SlotButton then
        self.SlotButton.OnClicked:Remove(self, M.OnSlotButtonClicked)
    end
end

--- 更新格子显示信息
--- 对应 C++ UpdateSlotInfo
function M:UpdateSlotInfo()
    if not self.ItemImage or not self.ItemAmountText then return end

    local InventoryManager = Utils.GetInventoryManager()
    if not InventoryManager then return end

    local ItemData = InventoryManager:GetItemStaticData(self.ItemInstance.ItemID)
    if ItemData then
        self.CachedItemData = ItemData

        if not ItemData.ItemIcon:IsNull() then
            -- 尝试同步获取已加载的图标
            local LoadedIcon = ItemData.ItemIcon:Get()
            if LoadedIcon then
                self.ItemImage:SetBrushFromTexture(LoadedIcon)
                self.ItemImage:SetRenderOpacity(1.0)
            else
                -- 异步加载：UnLua 中使用 StreamableManager
                local SoftPath = ItemData.ItemIcon:ToSoftObjectPath()
                UE.UAssetManager.GetStreamableManager():RequestAsyncLoad(
                    SoftPath,
                    function()
                        self:OnSlotIconLoaded(SoftPath)
                    end
                )
                self.ItemImage:SetRenderOpacity(0.0)
            end
        else
            self.ItemImage:SetRenderOpacity(0.0)
        end
    else
        self.ItemImage:SetRenderOpacity(0.0)
    end

    self.ItemAmountText:SetText(UE.UKismetTextLibrary.Conv_IntToText(self.ItemInstance.Count))
    self.ItemAmountText:SetRenderOpacity(self.ItemInstance.Count > 1 and 1.0 or 0.0)
end

--- 异步图标加载完成回调
---@param LoadedPath FSoftObjectPath
function M:OnSlotIconLoaded(LoadedPath)
    if not self.ItemImage then return end
    local LoadedIcon = UE.UKismetSystemLibrary.LoadAssetBlockin(LoadedPath)
    if LoadedIcon then
        self.ItemImage:SetBrushFromTexture(LoadedIcon)
        self.ItemImage:SetRenderOpacity(1.0)
    end
end

--- 格子按钮点击
function M:OnSlotButtonClicked()
    if self.OnSlotClicked and self.OnSlotClicked:IsBound() then
        self.OnSlotClicked:Broadcast(
            self.ItemArrayIndex,
            self.ItemInstance,
            self.CachedItemData,
            self
        )
    end
end

--- 设置选中状态
---@param bIsSelected boolean
function M:SetSelectionState(bIsSelected)
    if self.SelectionHighlightImage then
        self.SelectionHighlightImage:SetRenderOpacity(bIsSelected and 1.0 or 0.0)
    end
end

--- 获取缓存数据
---@return FItemData
function M:GetCachedItemData()
    return self.CachedItemData
end

return M
