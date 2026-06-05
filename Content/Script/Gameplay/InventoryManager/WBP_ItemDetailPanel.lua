-- 物品详情面板交互逻辑
-- 对应蓝图: /Game/Gameplay/InventoryManager/Blueprints/WBP_ItemDetailPanel
-- 替代 C++ UItemDetailPanelWidget 的交互逻辑

local Utils = require("Common.Utils")

---@type WBP_ItemDetailPanel_C
local M = UnLua.Class()

--- 更新详情面板
---@param ItemInstance FItemInstance
function M:UpdateDetails(ItemInstance)
    local InventoryManager = Utils.GetInventoryManager()
    if not InventoryManager then return end

    local StaticData = InventoryManager:GetItemStaticData(ItemInstance.ItemID)
    if not StaticData then
        -- 查询失败，清空面板
        if self.NameText then self.NameText:SetText(UE.FText.GetEmpty()) end
        if self.AvatarImage then self.AvatarImage:SetRenderOpacity(0.0) end
        if self.CategoryText then self.CategoryText:SetText(UE.FText.GetEmpty()) end
        if self.AmountText then self.AmountText:SetText(UE.FText.GetEmpty()) end
        if self.FunctionText then self.FunctionText:SetText(UE.FText.GetEmpty()) end
        if self.DetailText then self.DetailText:SetText(UE.FText.GetEmpty()) end
        if self.SourceText then self.SourceText:SetText(UE.FText.GetEmpty()) end
        return
    end

    -- 物品名称
    if self.NameText then
        self.NameText:SetText(StaticData.ItemName)
    end

    -- 物品图标 (异步加载)
    if self.AvatarImage then
        if not StaticData.ItemIcon:IsNull() then
            local LoadedIcon = StaticData.ItemIcon:Get()
            if LoadedIcon then
                self.AvatarImage:SetBrushFromTexture(LoadedIcon)
                self.AvatarImage:SetRenderOpacity(1.0)
            else
                local SoftPath = StaticData.ItemIcon:ToSoftObjectPath()
                UE.UAssetManager.GetStreamableManager():RequestAsyncLoad(
                    SoftPath,
                    function()
                        self:OnDetailIconLoaded(SoftPath)
                    end
                )
            end
        else
            self.AvatarImage:SetRenderOpacity(0.0)
        end
    end

    -- 物品分类
    if self.CategoryText then
        local EnumPtr = UE.UKismetSystemLibrary.GetEnum(UE.EItemCategory)
        if EnumPtr then
            local CategoryTextName = EnumPtr:GetDisplayNameTextByValue(StaticData.ItemCategory)
            self.CategoryText:SetText(CategoryTextName)
        end
    end

    -- 拥有数量
    if self.AmountText then
        local AmountFormat = UE.UKismetTextLibrary.IntToText(ItemInstance.Count)
        local FinalText = UE.UKismetTextLibrary.Format(UE.FText.fromString("拥有 {0}"), AmountFormat)
        self.AmountText:SetText(FinalText)
    end

    -- 功能描述
    if self.FunctionText then
        self.FunctionText:SetText(StaticData.ItemFunctionDescription)
    end

    -- 详细描述
    if self.DetailText then
        self.DetailText:SetText(StaticData.ItemDescription)
    end

    -- 来源说明 (TODO)
    if self.SourceText then
        -- 预留
    end
end

--- 异步图标加载完成
---@param LoadedPath FSoftObjectPath
function M:OnDetailIconLoaded(LoadedPath)
    if not self.AvatarImage then return end
    local LoadedIcon = UE.UKismetSystemLibrary.LoadAssetBlockin(LoadedPath)
    if LoadedIcon then
        self.AvatarImage:SetBrushFromTexture(LoadedIcon)
        self.AvatarImage:SetRenderOpacity(1.0)
    end
end

return M
