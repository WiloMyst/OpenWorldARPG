-- 物品分类标签页交互逻辑
-- 对应蓝图: /Game/Gameplay/InventoryManager/Blueprints/WBP_ItemCategoryTab
-- 替代 C++ UItemCategoryTabWidget 的交互逻辑

---@type WBP_ItemCategoryTab_C
local M = UnLua.Class()

function M:Construct()
    if self.TabButton then
        self.TabButton.OnClicked:Add(self, M.OnTabButtonClicked)
        self.TabButton.OnHovered:Add(self, M.OnTabButtonHovered)
        self.TabButton.OnUnhovered:Add(self, M.OnTabButtonUnhovered)
    end

    if self.ImageMouseHovered then
        self.ImageMouseHovered:SetRenderOpacity(0.0)
    end

    self:UpdateTabInfo()
end

function M:Destruct()
    if self.TabButton then
        self.TabButton.OnClicked:Remove(self, M.OnTabButtonClicked)
        self.TabButton.OnHovered:Remove(self, M.OnTabButtonHovered)
        self.TabButton.OnUnhovered:Remove(self, M.OnTabButtonUnhovered)
    end
end

--- 更新标签页信息
function M:UpdateTabInfo()
    if self.CategoryNameText then
        self.CategoryNameText:SetText(self.CategoryTabData.CategoryNameText)
    end

    if self.CategoryIcon and self.CategoryTabData.CategoryIcon then
        if not self.CategoryTabData.CategoryIcon:IsNull() then
            local LoadedIcon = self.CategoryTabData.CategoryIcon:Get()
            if LoadedIcon then
                self.CategoryIcon:SetBrushFromTexture(LoadedIcon)
            else
                local SoftPath = self.CategoryTabData.CategoryIcon:ToSoftObjectPath()
                UE.UAssetManager.GetStreamableManager():RequestAsyncLoad(
                    SoftPath,
                    function()
                        self:OnCategoryIconLoaded(SoftPath)
                    end
                )
            end
        end
    end
end

--- 异步图标加载完成
---@param LoadedPath FSoftObjectPath
function M:OnCategoryIconLoaded(LoadedPath)
    if not self.CategoryIcon then return end
    local LoadedIcon = UE.UKismetSystemLibrary.LoadAssetBlockin(LoadedPath)
    if LoadedIcon then
        self.CategoryIcon:SetBrushFromTexture(LoadedIcon)
    end
end

--- 设置选中状态
---@param bIsSelected boolean
function M:SetTabSelectedState(bIsSelected)
    if self.ImageMouseClicked then
        self.ImageMouseClicked:SetRenderOpacity(bIsSelected and 1.0 or 0.0)
    end
end

--- 标签按钮点击
function M:OnTabButtonClicked()
    if self.OnTabClicked and self.OnTabClicked:IsBound() then
        self.OnTabClicked:Broadcast(self, self.CategoryTabData.TabCategory)
    end
end

--- 鼠标悬停
function M:OnTabButtonHovered()
    if self.ImageMouseHovered then
        self.ImageMouseHovered:SetRenderOpacity(1.0)
    end
end

--- 鼠标离开
function M:OnTabButtonUnhovered()
    if self.ImageMouseHovered then
        self.ImageMouseHovered:SetRenderOpacity(0.0)
    end
end

return M
