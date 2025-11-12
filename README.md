# Open World ARPG

[![license](https://img.shields.io/badge/license-MIT-blue)](https://github.com/WiloMyst/OpenWorldARPG/blob/master/LICENSE) [![GitHub repo size](https://img.shields.io/github/repo-size/WiloMyst/OpenWorldARPG)](https://github.com/WiloMyst/OpenWorldARPG)



## 概述

基于UE5 C++与蓝图混合开发的开放世界ARPG原型。（只提供Content目录结构，无第三方资源）



## 演示

[Video01](assets/Video01.mp4)

[Video02](assets/Video02.mp4)

[Video03](assets/Video03.mp4)

[Video04](assets/Video04.mp4)



## 功能列表

- **基于 Gameplay Ability System (GAS) 的角色行为/技能实现与状态管理：**

  - **能力驱动**: 使用GAS来驱动所有探索与战斗行为，包括跳跃、冲刺、行走、攀爬、滑行、钩索、倒地，普攻、重击、下落攻击、瞄准射击，确保了状态管理的严谨性和逻辑的模块化。

  <img src="assets\Image01.png" />

  

- **可扩展的动画架构:**

  - **分层设计**: 构建了L1-基础层到L5-修饰层的多层动画蓝图架构，确保了逻辑的清晰和可维护性。
  - **继承与复用**: 采用**父子动画蓝图**和**动画层接口**，实现动画逻辑的复用，为角色不同行为逻辑提供了独立的、可插拔的动画逻辑层和蒙太奇插槽，实现**蒙太奇攻击模式**与**瞄准攻击模式**。
  - **多线程动画更新**: 参考**Lyra初学者游戏包**示例项目，实现多线程安全的动画更新，提高性能。

  <img src="assets\Image02.png" />

  

- **模块化的多角色管理系统:**

  - **架构设计**: 实现配队角色**后台共存**机制，确保了角色切换的**零延迟**和高性能。通过将角色核心数据 `ACharacterData` 与视觉表现 `APlayerCharacter` 分离，构建**数据-表现分离**架构。
  - **数据驱动**: 采用**数据表格 (DataTable)** 结合**数据资产 (DataAsset)** 来配置所有角色基本数据，使策划可以不通过修改代码，即可轻松添加和配置新角色。进入主世界时，读取数据生成配队角色。
  - **角色切换**: 非Spawn的方式，实现了流畅的角色入场/离场逻辑，为协作技能的实现奠定了基础。

  <img src="assets\Image03.png" />



- **数据驱动的背包与物品系统:**

  - **模块化设计**: 基于**游戏实例子系统** 设计了全局背包管理器，实现了与角色状态解耦的、可跨关卡持久化的模块化背包系统。
  - **数据驱动**: 采用**数据表格 (DataTable)** 结合**数据资产 (DataAsset)** 来定义所有物品，使策划可以不通过修改代码，即可轻松添加和配置新物品。
  - **UMG交互界面**: 使用**UMG**创建背包UI，支持物品的查看、使用和丢弃等交互操作。

  <img src="assets\Image04.png" style="zoom:70%;" />



- **敌人AI系统：**

  - **敌人AI**: 利用**行为树**和 **AI感知组件**，实现了敌人从巡逻、发现、追踪到攻击的AI逻辑。
  - **动态UI反馈**: 使用**Widget Component**为敌人实现了与距离动态缩放的头顶血条，监听GAS的**属性变化委托**，实时更新UI的屏幕**朝向**。

  <img src="assets\Image05.png" style="zoom:70%;" />

  

  

- **异步资源加载与关卡切换:**

  - **异步加载**: 广泛使用**软对象指针** 和 **异步加载**，将核心资产引用与即时加载解耦，实现**按需加载**，为开放世界体验提供了性能保障。
  - **转场管理**: 开发了加载界面，在后台异步加载资源的同时更新**加载进度条**，为玩家提供流畅的视觉过渡。

  <img src="assets\Image06.png" style="zoom:70%;" />



## 版本

- Unreal Engine 5.2+

- Visual Studio 2022, MSVC 14.34+




## **资源**

Plugins：

- [KawaiiPhysics](https://github.com/pafuhana1213/KawaiiPhysics)
- [SPCRJointDynamics](https://github.com/SPARK-inc/SPCRJointDynamics)
- [UnLua](https://github.com/Tencent/UnLua)

Models：

- [PlayBox](https://www.aplaybox.com/)

