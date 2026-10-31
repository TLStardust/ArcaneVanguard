"""把 EnablePhysicsDrop 整体替换成「碰撞已在 OnConstruction 配好，这里只开物理」的版本。"""
import io

PATH = r'D:/UEProject/ArcaneVanguard/Source/ArcaneVanguard/Weapon/ArcaneVanguardWeapon.cpp'

NEW_FUNC = '''void AArcaneVanguardWeapon::EnablePhysicsDrop()
{
	if (!MeshComponent)
	{
		return;
	}

	// 1) 停 Tick。握持插值每帧调 ApplyCombatAlpha 改 Mesh 的相对位置，
	//    和物理模拟同时作用会互相打架（表现为落地瞬间抖动或瞬移）。
	SetActorTickEnabled(false);

	// 2) 碰撞已经在 OnConstruction 里配好了（QueryAndPhysics + PhysicsBody）。
	//    [!] 不能在这里才改碰撞 —— 2026-10-30 实测：Spawn 之后物理体已经按 NoCollision
	//        建好，此时无论 SetCollisionEnabled / SetCollisionProfileName / RecreatePhysicsState
	//        怎么组合，GetCollisionEnabled() 都返回 0、GetMass() 停在默认 1.0，法杖穿过地面。
	//        正确做法是在 OnConstruction（物理状态创建之前）配好，这里只负责开模拟。
	//    保留兜底分支：万一某个生成路径没走到 OnConstruction，补设一次再重建。
	if (MeshComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Weapon] %s 脱手时碰撞仍未启用（预期 QueryAndPhysics），补设一次"),
			*GetName());

		ApplyWeaponCollisionSetup();
		MeshComponent->RecreatePhysicsState();
	}

	// 3) 开模拟并唤醒（脱离 Attach 后刚体可能处于睡眠状态）
	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->WakeAllRigidBodies();

	// 4) 诊断：CollisionEnabled 必须是 3，Mass 应该是法杖真实重量（不是默认 1.0）
	UE_LOG(LogTemp, Log,
		TEXT("[Weapon] %s 脱手：CollisionEnabled=%d SimulatePhysics=%d Mass=%.2f Mesh=%s BodyInstance=%s"),
		*GetName(),
		static_cast<int32>(MeshComponent->GetCollisionEnabled()),
		MeshComponent->IsSimulatingPhysics() ? 1 : 0,
		MeshComponent->GetMass(),
		MeshComponent->GetStaticMesh() ? *MeshComponent->GetStaticMesh()->GetName() : TEXT("NULL"),
		MeshComponent->GetBodyInstance() ? TEXT("OK") : TEXT("NULL"));

	// 5) 一点随机角速度：否则每次都以完全相同的姿态落地，看起来像"摆上去的"。
	const FVector RandomAngularImpulse(
		FMath::FRandRange(-40.0f, 40.0f),
		FMath::FRandRange(-40.0f, 40.0f),
		FMath::FRandRange(-40.0f, 40.0f));

	MeshComponent->AddAngularImpulseInDegrees(RandomAngularImpulse, NAME_None, /*bVelChange=*/true);

	UE_LOG(LogTemp, Log, TEXT("[Weapon] %s 已脱手，进入自由落体"), *GetName());
}
'''

txt = io.open(PATH, encoding='utf-8').read()
start = txt.index('void AArcaneVanguardWeapon::EnablePhysicsDrop()')
end = txt.index('\n}\n', start) + 3

txt = txt[:start] + NEW_FUNC + txt[end:]
io.open(PATH, 'w', encoding='utf-8', newline='\n').write(txt)

print('已替换，新文件长度 =', len(txt))
print('检查 ApplyWeaponCollisionSetup 出现次数 =', txt.count('ApplyWeaponCollisionSetup'))
