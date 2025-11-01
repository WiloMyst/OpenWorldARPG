SoldierCharacterNew为士兵

站姿：ESoldierBasePose::SBP_Stand
蹲姿：ESoldierBasePose::SBP_Crouch
卧姿：ESoldierBasePose::SBP_Prone

休息姿态：ESoldierArmPose::SAP_free

瞄准姿态：ESoldierArmPose::SAP_aimTarget

切换基础姿态：
void ChangeBasePose(ESoldierBasePose SoldierBasePose)

切换手部姿态
void ChangeArmPose(ESoldierArmPose ArmPose)

设置敌人
void SetEnemy(AActor* Enemy)；

开火：
void Fire()

移动：
void MoveToLocation(FVector TargetLocation)


士兵蓝图类：
Blueprint'/Game/testSoldier.testSoldier_C'


