#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Rules/FistCombatRules.h"
#include "Rules/ComboRules.h"
#include "Progress/CampaignProgress.h"
#include "Rules/PortalUpgrades.h"
#include "Enemies/EnemyTypes.h"
#include "Enemies/EnemyMovesetState.h"
#include "ArenaFighter.generated.h"

class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UAnimSequence;
class UPointLightComponent;
class UTextRenderComponent;
class AEnemySpawnPoint;
class UBossBehavior;
struct FCombatImpactBudget;

UCLASS(Blueprintable)
class HELLGIRL_API AArenaFighter : public ACharacter
{
    GENERATED_BODY()
public:
    AArenaFighter();
    bool SetOutfit(int32 Outfit);
    int32 GetOutfit() const { return SelectedOutfit; }
    void Special();
    void SelectWeapon(int32 Weapon);
    void EquipFists() { SelectWeapon(0); }
    void EquipSword() { SelectWeapon(1); }
    int32 GetWeapon() const { return SelectedWeapon; }
    float GetWeaponMenuTime() const { return WeaponMenuClock; }
    float GetUltimateTime() const { return UltimateClock; }
    // Ultimates awaken in Stage 3, when the Goblin Queen is down to 30% (Progress/CampaignProgress.h).
    bool HasUltimate() const { return HellgirlProgress::UltimatesUnlocked() && (SelectedOutfit == 0 || SelectedOutfit == 1 || SelectedOutfit == 4); }
    float GetSpeedMultiplier() const { return UltimateClock > 0.f ? (ActiveUltimate == 0 ? 2.f : ActiveUltimate == 1 ? 1.5f : 1.f) : 1.f; }
    float GetDashMultiplier() const { return UltimateClock > 0.f && ActiveUltimate == 0 ? 6.f : 1.f; }
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual void Landed(const FHitResult& Hit) override;
    virtual void MoveBlockedBy(const FHitResult& Impact) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float MaxHealth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float AttackDamage = 34.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float AttackRange = 220.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float WalkSpeed = 560.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") float Health = 100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") float Stamina = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat|Energy", meta=(ClampMin="40.0")) float MaxEnergy = 100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat|Energy") float Energy = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") bool bEnemy = false;
    UFUNCTION(BlueprintCallable, Category="Combat") void Attack();
    UFUNCTION(BlueprintCallable, Category="Combat") void HeavyAttack();
    UFUNCTION(BlueprintCallable, Category="Combat") void ReleaseHeavyAttack();
    UFUNCTION(BlueprintCallable, Category="Combat") void Dodge();
    void MakeEnemy(int32 Wave, bool Flying = false);
    void PrepareForPause();
    void ApplyCombatLaunch(const FVector& Velocity, int32 ChainDepth = 0, AArenaFighter* IgnoreEnemy = nullptr, TSharedPtr<FCombatImpactBudget> ImpactBudget = nullptr);
    void ApplyPhysicsDamage(float Damage, const FVector& ImpulseVelocity);
    void ResetAfterRecovery();
    bool IsCombatLaunched() const { return bCombatLaunched; }
    bool IsDeathRagdollActive() const { return bDeathRagdollActive; }
    float GetCombatMomentumSpeed() const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Enemy") EHellgirlEnemyType EnemyType = EHellgirlEnemyType::Imps;
    void SetEnemyType(EHellgirlEnemyType Type);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") bool bFlyingEnemy = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float MaxChargeSeconds = 1.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float ShortAirDashRange = 350.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float MediumAirDashRange = 600.f;
    void ReceiveHit(float Damage, const FVector& Direction, float Knockback = 160.f, float Knockdown = 0.f, TSharedPtr<FCombatImpactBudget> ImpactBudget = nullptr);
    const HellgirlCombo::FMeter& GetComboMeter() const { return ComboMeter; }
    // Soul portal upgrades bought this level (Rules/PortalUpgrades.h); enemies keep the neutral defaults.
    HellgirlUpgrades::FStats Upgrades;
    // Stage 1 opening: Hellgirl lies on the floor (no control) until released, then plays the get-up.
    void BeginWakeUp() { WakeUpTime = 0.f; bWakeHeld = true; }
    void ReleaseWakeUp() { bWakeHeld = false; }
    bool IsWakingUp() const { return WakeUpTime >= 0.f; }
    // Previews only: start the meter at a given number of points.
    void SetComboPointsForPreview(float Points) { ComboMeter.Points = Points; ComboMeter.SinceHit = 0.f; }
    bool IsBossAttackArmored() const;
    bool bBossEncounter = false;
    bool bStorySurrendered = false;
    // Boss rules (phases, summons, damage gates); set by SetEnemyType for boss types, otherwise null.
    UBossBehavior* GetBossBehavior() const { return BossBehavior; }
    template<class T> T* GetBoss() const { return Cast<T>(BossBehavior); }
    bool IsBossHidden() const;
    float FilterEnemyDamage(float Damage);
    void UpdateGoblinTactics(float Dt, AArenaFighter* Player);
    EEnemyMove GetEnemyMove() const { return EnemyMove; }
    // Checks only: the clip an enemy shows now, and the time left in its current attack.
    const UAnimSequence* GetEnemyActiveAnimation() const { return EnemyActiveAnimation; }
    float GetAttackClock() const { return AttackClock; }
    // Energy Hellgirl spends counts toward the level's Soul Coin bonus (Rules/SoulRewards.h).
    void NoteEnergySpent(float Amount);
    // Checks only: whether a perfect dodge right now would counter Enemy.
    bool CouldCounter(const AArenaFighter* Enemy) const { return CanCounter(Enemy); }
    void RunBossDesignCheck();
    FString MoveLabel = TEXT("FISTS READY");
    int32 GetComboStep() const { return ComboClock > 0.f ? Combo : 0; }
    bool IsAlive() const { return Health > 0.f; }
    bool IsBlocking() const;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat") float PerfectDodgeWindow = .45f;
    FVector HomePosition = FVector::ZeroVector;
    bool bGuardHome = false;
    TWeakObjectPtr<AEnemySpawnPoint> EncounterSite;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat") float Riposte = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Sword;
    // Placeholder blade placement relative to the RightHand bone (the cube's long axis is X).
    UPROPERTY(EditAnywhere, Category="Combat|Sword") FVector SwordGripOffset = FVector(0.f, 50.f, 0.f);
    UPROPERTY(EditAnywhere, Category="Combat|Sword") FRotator SwordGripRotation = FRotator(0.f, 90.f, 0.f);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Head;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> RightHand;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> LeftHand;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> RightFoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> LeftFoot;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USpringArmComponent> CameraArm;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCameraComponent> Camera;
private:
    int32 SelectedOutfit = 0;
    int32 SelectedWeapon = 0;
    UPROPERTY() TObjectPtr<UBossBehavior> BossBehavior;
    // Boss behaviours drive the enemy move state (clocks, moves, cooldowns) directly.
    friend class UGoblinQueenBehavior;
    friend class UImpCommanderBehavior;
    int32 ActiveUltimate = -1;
    float UltimateClock = 0.f;
    float UltimatePulseClock = 0.f;
    float ParalysisClock = 0.f;
    float WeaponMenuClock = 0.f;
    bool bLastComboHeavy = false;
    bool bSecondHitResolved = true;
    TSet<TWeakObjectPtr<AArenaFighter>> JabHitTargets;
    void UpdateUltimate(float Dt);
    void UltimatePulse(float Damage, float Range);
    void RunRevisedCombatCheck();
    UPROPERTY() TObjectPtr<USkeletalMesh> RagsMesh;
    UPROPERTY() TObjectPtr<USkeletalMesh> ArmorMesh;
    void BeginEnemyMove(EEnemyMove Move, AArenaFighter* Player);
    void UpdateEnemyMoveMotion(float Dt);
    bool UpdateImpTactics(float Dt, AArenaFighter* Player);
    bool CanBeginEnemyMove(const AArenaFighter* Player) const;
    bool IsEnemyGroundAheadSafe(const FVector& Direction, float Distance = 130.f) const;
    void CancelEnemyMove();
    void DrawEnemyMoveTelegraph();
    void RunEnemyMovesetCheck(float Dt);
    void RunAttackAnimationPreview(float Dt);
    void RunCombatBalanceCheck(float Dt);
    TSharedPtr<FCombatImpactBudget> ActiveAttackImpactBudget;
    TSharedPtr<FCombatImpactBudget> CombatLaunchImpactBudget;
    EEnemyMove EnemyMove = EEnemyMove::None;
    float EnemyMoveCooldown = 0.f;
    float GoblinQuickSlashClock = 0.f; // until this goblin may quick-slash again
    float EnemyDecisionClock = 0.f;
    int32 EnemyAttackSerial = 0;
    FVector EnemyMoveStart = FVector::ZeroVector;
    FVector EnemyMoveTarget = FVector::ZeroVector;
    FVector EnemyFacing = FVector::ForwardVector;
    float EnemyOrbitSign = 1.f;
    float EnemyLastAttackTime = -100.f;
    bool bEnemyMoveMotionStopped = false;
    float EnemyMoveMotionProgress = 0.f;
    void HandleDeath(const FVector& ImpulseVelocity);
    // Hit feedback: sparks (and dust on heavy hits) at the contact point, a flash, a split-second hit-stop,
    // and a short camera shake for the player. Hit-stop and shake stay off during automated checks.
    void PlayImpact(const FVector& Direction, float Damage, bool Heavy, bool Blocked);
    // Style combo meter (Rules/ComboRules.h): varied landed hits raise a 1.1x-2.0x damage bonus.
    HellgirlCombo::FMeter ComboMeter;
    float WakeUpTime = -1.f;
    bool bWakeHeld = false;
    bool bComboCredited = false;
    float ComboMultiplier() const;
    void RunComboCheck();
    void AddCameraShake(float Strength, float Duration);
    void UpdateCameraShake(float Dt);
    float ShakeTime = 0.f, ShakeDuration = 0.f, ShakeStrength = 0.f;
    void StartDeathRagdoll(const FVector& ImpulseVelocity);
    void UpdateCombatPhysics(float Dt);
    void RunPhysicsCheck(float Dt);
    void RunEnergyCheck();
    bool bAttackEnergyGranted = false;
    bool bDeathHandled = false;
    bool bDeathRagdollActive = false;
    bool bCombatLaunched = false;
    float CombatLaunchClock = 0.f;
    float LaunchAirControl = .45f;
    int32 LaunchChainDepth = 0;
    bool bWorldImpactQueued = false;
    TSet<TWeakObjectPtr<AActor>> LaunchHitActors;
    struct FQueuedCombatImpact { FHitResult Hit; FVector Velocity; float ClosingSpeed; int32 ChainDepth; TSharedPtr<FCombatImpactBudget> Budget; };
    TArray<FQueuedCombatImpact> QueuedCombatImpacts;
    friend class FImpAnimationPlaybackTest;
    friend class FPlayerAnimationPlaybackTest;
    UPROPERTY() TObjectPtr<UPointLightComponent> AttackFlash;
    UPROPERTY() TObjectPtr<UTextRenderComponent> EnemyNameLabel;
    bool bCounterDodge = false;
    bool CanCounter(const AArenaFighter* Enemy) const;
    bool TryPerfectCounter();
    void RunPerfectCounterCheck();
    UPROPERTY() TObjectPtr<UAnimSequence> NeutralIdleAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> WalkAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> RunAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> JumpAnimation;
    float AirAnimationTime = 0.f;
    float LandingAnimationTime = .25f;
    bool bAnimationWasAirborne = false;
    UPROPERTY() TObjectPtr<UAnimSequence> ActiveAnimation;
    // Per-outfit combat clips from Tools/Animations (see CombatClipNames); missing clips use the neutral pose.
    UPROPERTY() TMap<FName, TObjectPtr<UAnimSequence>> CombatAnimations;
    UAnimSequence* FindAttackAnimation(FistCombat::Move Move) const;
    // Maps attack progress to clip time so the clip's contact frame lands on the attack's damage moment.
    float AttackClipPosition(float Progress, const UAnimSequence* Clip) const;
    float PlayerHitAnimationTime = 100.f;
    // The roll outlasts the 0.25 s dodge dash so it can play at a natural speed; a follow-up
    // attack or a hit takes over immediately.
    float DodgeAnimationTime = 100.f;
    static constexpr float DodgeAnimationDuration = .55f;
    float PlayerDeathAnimationTime = 0.f;
    float PlayerKnockdownDuration = 1.f;
    UPROPERTY() TObjectPtr<UAnimSequence> EnemyIdleAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> EnemyMoveAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> EnemyAttackAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> EnemyQuickAttackAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> EnemyHitAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> EnemyDeathAnimation;
    UPROPERTY() TObjectPtr<UAnimSequence> EnemyActiveAnimation;
    float EnemyAnimationTime = 0.f;
    float EnemyHitAnimationTime = 100.f;
    void UpdateEnemyAnimation(float DeltaSeconds);
    float LocomotionPhase = 0.f;
    bool bWalkHeld = false;
    bool bBlockHeld = false;
    bool bSprintHeld = false;
    void BlockPressed();
    void BlockReleased() { bBlockHeld = false; }
    void SprintPressed() { bSprintHeld = true; }
    void SprintReleased() { bSprintHeld = false; }
    void WalkPressed() { bWalkHeld = true; }
    void WalkReleased() { bWalkHeld = false; }
    void Forward(float Value);
    void Right(float Value);
    void Turn(float Value);
    void Look(float Value);
    void TurnStick(float Value);
    void LookStick(float Value);
    void ZoomWheel(float Value);
    void ZoomTrigger(float Value);
    float DesiredCameraDistance = 420.f;
    void JumpPressed();
    void JumpReleased();
    void QueueAttack(bool Heavy);
    void StartAttack(bool Heavy);
    void UpdatePose(float DeltaSeconds);
    void StartAirMove();
    void FinishAirMove();
    void CancelCharge();
    // Keep APawn::Restart available for possession and movement initialization.
    void RestartArena();
    void ReleaseMouse();
    void ResolveAttack();
    void UpdateAttackTiming(float Dt);
    void RunAttackCancelCheck(float Dt);
    void PrepareCombatMovement();
    void StartDodgeMomentum();
    void UpdatePlayerMomentum(float Dt);
    void ResetPlayerMomentum();
    void LandedPlayerMomentum();
    float GroundDashSpeed = 0.f;
    FVector GroundDashLastLocation = FVector::ZeroVector;
    float PlayerMomentumClock = 0.f;
    float PlayerMomentumSpeedLimit = 0.f;
    float AttackMomentumSpeed = 0.f;
    bool bMomentumAirborne = false;
    bool bAirDodgeBoostUsed = false;
    void RunCombatMovementCheck();
    void UpdateCombatMovement(float Dt);
    AArenaFighter* FindCombatTarget() const;
    TWeakObjectPtr<AArenaFighter> CombatTarget;
    float CombatFocusClock = 0.f;
    float ManualCameraClock = 0.f;
    float GroundDashClock = 0.f;
    float GroundDashRemaining = 0.f;
    FVector GroundDashDirection = FVector::ForwardVector;
    bool bTargetedGroundDash = false;
    float AttackClock = 0.f;
    FistCombat::AttackSpec CurrentAttack{FistCombat::Move::RightPunch, .32f, .4f, 12.f, 175.f, 60.f, 0.f, 1};
    float BufferClock = 0.f;
    bool bBufferedHeavy = false;
    int32 AirCombo = 0;
    bool bAirFinisherUsed = false;
    bool bHeavyHeld = false;
    float ChargeClock = 0.f;
    float PendingCharge = 0.f;
    float AirHangClock = 0.f;
    float AirHangBudget = 2.4f;
    float AirDashClock = 0.f;
    bool bGroundImpactPending = false;
    float ImpactTimeout = 0.f;
    float NormalGravity = 1.f;
    TWeakObjectPtr<AArenaFighter> AirTarget;
    float PostDodgeClock = 0.f;
    float KnockdownClock = 0.f;
    float MoveLabelClock = 0.f;
    float DodgeClock = 0.f;
    float DodgeCooldown = 0.f;
    float HitClock = 0.f;
    float ComboClock = 0.f;
    int32 Combo = 0;
    bool bHitResolved = true;
    FVector DodgeDirection = FVector::ForwardVector;
};
