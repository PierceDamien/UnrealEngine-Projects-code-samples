// Copyright (c) 2024. Sir Knight title is a property of Quantinum ltd. All rights reserved.

#include "Characters/SKPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Characters/Components/SKInventoryComponent.h"
#include "Characters/Components/SKPhysicsHandleComponent.h"
#include "Core/Interface/SKInterfaceInteractable.h"
#include "Core/SKCoreTypes.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "Props/SKCollectible.h"
#include "UI/SKPlayerHUD.h"
#include "UI/Widgets/SKInventoryWidget.h"

//********************* DEFAULT *********************
ASKPlayerCharacter::ASKPlayerCharacter(const FObjectInitializer &ObjectInitializer) : Super(ObjectInitializer)
{
    PlayerCamera = CreateDefaultSubobject<UCameraComponent>("Player camera");
    PlayerCamera->SetupAttachment(GetRootComponent());
    PlayerCamera->bUsePawnControlRotation = true;

    PhysicsHandle = CreateDefaultSubobject<USKPhysicsHandleComponent>("Physics handle");
}

void ASKPlayerCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

#if !UE_BUILD_SHIPPING
    PrintDebugInfo();
#endif
}

void ASKPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();
    InitializeComponents();
}

//********************* INPUT *********************
void ASKPlayerCharacter::SetupPlayerInputComponent(UInputComponent *PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    ControllerSetup();

    if (UEnhancedInputComponent *Input = CastChecked<UEnhancedInputComponent>(PlayerInputComponent))
    {
        Input->BindAction(InputData.MovingAction, ETriggerEvent::Triggered, this, &ASKPlayerCharacter::MoveAction);
        Input->BindAction(InputData.LookAction, ETriggerEvent::Triggered, this, &ASKPlayerCharacter::LookingAction);
        Input->BindAction(InputData.JumpAction, ETriggerEvent::Triggered, this, &ASKPlayerCharacter::Jump);
        Input->BindAction(InputData.SprintAction, ETriggerEvent::Triggered, this, &ASKPlayerCharacter::StartSprinting);
        Input->BindAction(InputData.SprintAction, ETriggerEvent::Completed, this, &ASKPlayerCharacter::StartRunning);
        Input->BindAction(InputData.WalkAction, ETriggerEvent::Triggered, this, &ASKPlayerCharacter::StartWalking);
        Input->BindAction(InputData.AltAction, ETriggerEvent::Triggered, this,
                          &ASKPlayerCharacter::HandleAlternativeAction);
        Input->BindAction(InputData.InteractionAction, ETriggerEvent::Triggered, this, &ASKPlayerCharacter::Interact);
        Input->BindAction(InputData.InteractionActionHold, ETriggerEvent::Triggered, this,
                          &ASKPlayerCharacter::HandleGrabbing);
    }
}

void ASKPlayerCharacter::ControllerSetup()
{
    if (APlayerController *PC = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem *Subsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            Subsystem->ClearAllMappings();
            Subsystem->AddMappingContext(InputData.InputMapping, 0);
        }
    }
}

void ASKPlayerCharacter::MoveAction(const FInputActionValue &Value)
{
    const FVector2D MovingAxis = Value.Get<FVector2D>();

    AddMovementInput(GetActorForwardVector(), MovingAxis.X);
    AddMovementInput(GetActorRightVector(), MovingAxis.Y);
}

void ASKPlayerCharacter::LookingAction(const FInputActionValue &Value)
{
    const auto LookingAxisX = Value.Get<FVector2D>().X;
    const auto LookingAxisY = Value.Get<FVector2D>().Y * -1;

    if (GetActionType() != EActionType::ERotating)
    {
        AddControllerYawInput(LookingAxisX);
        AddControllerPitchInput(LookingAxisY);
    }
    else
    {
        PhysicsHandle->RotateGrabbedComponent(Value.Get<FVector2D>());
    }
}

//********************* MULTITHREADING *********************
void ASKPlayerCharacter::GetLookedAtActor(TObjectPtr<AActor> &LookedAtActor) const
{
    double BestDotProduct = -1.0f;

    FRWScopeLock ReadLock(DataGuard, SLT_ReadOnly);

    for (const auto &Item : InteractablesInVicinity)
    {
        // get actor bounds
        FVector ActorBoundsOrigin, ActorBoxExtent;
        Item->GetActorBounds(false, ActorBoundsOrigin, ActorBoxExtent);

        // calcuate dot product
        const auto DotProduct = FVector::DotProduct(
            PlayerCamera->GetForwardVector(),
            UKismetMathLibrary::GetDirectionUnitVector(PlayerCamera->GetComponentLocation(), ActorBoundsOrigin));
        if (DotProduct >= BestDotProduct)
            BestDotProduct = DotProduct;
        else
            continue;

        // Minimally required dot product value to be considered
        const auto Threshold = FMath::Clamp(
            (FVector::Distance(PlayerCamera->GetComponentLocation(), ActorBoundsOrigin) / 10000.0f) + 0.95f, 0.0f,
            0.99f);

        if (BestDotProduct < Threshold)
            LookedAtActor = nullptr;
        else
            LookedAtActor = Item;
    }
}

void ASKPlayerCharacter::PrintDebugInfo() // DEBUG
{
    // showing the amount of items in vicinity
    if (DataGuard.TryReadLock())
    {

        GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Blue,
                                         "Items in list: " + FString::FromInt(InteractablesInVicinity.Num()), true);
        DataGuard.ReadUnlock();
    }

    // Show if can interact in the moment
    if (InteractibleActive)
    {
        GEngine->AddOnScreenDebugMessage(2, 0.0f, FColor::Emerald, "I'm looking at: " + InteractibleActive->GetName(),
                                         true);
    }

    // Current player state || This system will be replaced with GAS
    if (GetWorld())
    {
        FString CurrentActionType = UEnum::GetValueAsString(GetActionType());
        FString CurrentMovementType = UEnum::GetValueAsString(GetMovementType());
        GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Blue,
                                         "Current states: " + CurrentMovementType + " | " + CurrentActionType, true);
    }

    // Inventory
    if (Inventory)
    {
        GEngine->AddOnScreenDebugMessage(6, 0.0f, FColor::Cyan,
                                         "Items in inventory: " + FString::FromInt(Inventory->GetInventoryData().Num()),
                                         true);
    }

    // Draw XY arrows for physics handle
    if (PhysicsHandle && PhysicsHandle->GrabbedComponent)
    {
        // �������� ������� ��������� � ������� Physics Handle
        FVector HandleLocation;
        FRotator HandleRotation;
        PhysicsHandle->GetTargetLocationAndRotation(HandleLocation, HandleRotation);

        // ����� �������
        float ArrowLength = 30.0f;

        // ������� �� ��� X (�������)
        FVector XDirection = HandleRotation.RotateVector(FVector::ForwardVector) * ArrowLength;
        FVector XArrowEnd = HandleLocation + XDirection;
        DrawDebugDirectionalArrow(GetWorld(), HandleLocation, XArrowEnd,
                                  25.0f,       // ����� "�������" ������
                                  FColor::Red, // ���� ��� ��� X
                                  false,       // ���������� ���������
                                  -1.0f,       // ����� ����� ������
                                  0,           // ���������
                                  2.0f         // ������� �������
        );

        // ������� �� ��� Z (�����)
        FVector ZDirection = HandleRotation.RotateVector(FVector::UpVector) * ArrowLength;
        FVector ZArrowEnd = HandleLocation + ZDirection;
        DrawDebugDirectionalArrow(GetWorld(), HandleLocation, ZArrowEnd,
                                  25.0f,        // ����� "�������" ������
                                  FColor::Blue, // ���� ��� ��� Z
                                  false,        // ���������� ���������
                                  -1.0f,        // ����� ����� ������
                                  0,            // ���������
                                  2.0f          // ������� �������
        );
    }

    // Interactible active rotation debug info
    if (InteractibleActive)
    {
        // �������� ������� ��������� �������
        FRotator InteractibleRotation = InteractibleActive->GetActorRotation();

        // ������� ������� ��������� ������� �� �����
        GEngine->AddOnScreenDebugMessage(
            -1, // ���������� ID ���������, -1 ��������, ��� ��������� �������� ����� �����
            0,  // ����� ����������� ���������
            FColor::Cyan, // ���� ������
            FString::Printf(TEXT("InteractibleActive Rotation: Pitch: %f, Yaw: %f, Roll: %f"),
                            InteractibleRotation.Pitch, InteractibleRotation.Yaw, InteractibleRotation.Roll));
    }

    // Phys handle rotation debug info
    if (PhysicsHandle)
    {
        // �������� TargetRotation Physics Handle
        FRotator PhysicsHandleTargetRotation;
        FVector t;
        PhysicsHandle->GetTargetLocationAndRotation(t, PhysicsHandleTargetRotation);

        // ������� TargetRotation Physics Handle �� �����
        GEngine->AddOnScreenDebugMessage(
            -1, // ���������� ID ���������, -1 ��������, ��� ��������� �������� ����� �����
            0,  // ����� ����������� ���������
            FColor::Green, // ���� ������
            FString::Printf(TEXT("PhysicsHandle Target Rotation: Pitch: %f, Yaw: %f, Roll: %f"),
                            PhysicsHandleTargetRotation.Pitch, PhysicsHandleTargetRotation.Yaw,
                            PhysicsHandleTargetRotation.Roll));
    }
}

//********************* INTERACTIONS *********************

void ASKPlayerCharacter::HandleInteractionActor()
{
    GetLookedAtActor(InteractibleActive);
    if (!InteractibleActive) return;

    // final check with trace
    FHitResult TraceCheck = TraceToActor(InteractibleActive);

    if (!TraceCheck.bBlockingHit) return;

    // final comparison
    if (TraceCheck.GetActor() == InteractibleActive || TraceCheck.GetActor()->Implements<USKInterfaceInteractable>())

    {
        InteractibleActive = TraceCheck.GetActor();
    }

    else
    {
        InteractibleActive = nullptr;
    }
}

void ASKPlayerCharacter::Interact()
{
    if (GetActionType() == EActionType::EGrabbing)
    {
        PhysicsHandle->ReleaseItem();
        SetActionType(EActionType::ENone);
    }
    else
    {

        FInventoryItemData ItemData;
        auto Item = Cast<ASKCollectible>(InteractibleActive);
        if (!Item) return;
        ItemData.Name = Item->GetInGameName();
        PlayerInventoryWidget->AddToInventoryList(ItemData);
        ASKBaseCharacter::Interact();
    }
}

bool ASKPlayerCharacter::CanGrabItem()
{
    if (!InteractibleActive) return false;

    if (GetActionType() == EActionType::ENone && InteractibleActive->GetRootComponent()->IsSimulatingPhysics())
    {
        return true;
    }
    else
    {
        return false;
    }
}

void ASKPlayerCharacter::HandleGrabbing()
{
    if (CanGrabItem())
    {
        SetActionType(EActionType::EGrabbing);
        PhysicsHandle->GrabItem();
    }
    else if (!CanGrabItem() && PhysicsHandle->GetItemToGrab())
    {
        PhysicsHandle->ReleaseItem();
    }
}

void ASKPlayerCharacter::HandleAlternativeAction()
{
    if (GetActionType() == EActionType::EGrabbing)
    {
        SetActionType(EActionType::ERotating);
    }
    else if (GetActionType() == EActionType::ERotating)
    {
        SetActionType(EActionType::EGrabbing);
    }
    else
    {
        return;
    }
}

//********************* UTILS *********************

void ASKPlayerCharacter::InitializeComponents()
{

    PlayerController = UGameplayStatics::GetPlayerController(this, 0);
    check(PlayerController.Get());
    PlayerHUD = Cast<ASKPlayerHUD>(PlayerController->GetHUD());
    check(PlayerHUD.Get());
    PlayerInventoryWidget = Cast<USKInventoryWidget>(PlayerHUD->GetInventoryWidget());
    check(PlayerInventoryWidget.Get());
}

FHitResult ASKPlayerCharacter::TraceToActor(const TObjectPtr<AActor> &OtherActor) const
{

    FHitResult HitResult;

    GetWorld()->LineTraceSingleByChannel(HitResult, PlayerCamera->GetComponentLocation(),
                                         OtherActor->GetActorLocation(), ECollisionChannel::ECC_Visibility);

    return HitResult;
}

bool ASKPlayerCharacter::TraceFromCamera(FHitResult &HitResult, const float TraceDistance)
{
    FVector TraceStart = PlayerCamera->GetComponentLocation();
    FVector TracecEnd = TraceStart + (PlayerCamera->GetForwardVector() * TraceDistance);

    return GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TracecEnd, ECollisionChannel::ECC_Visibility);
}

bool ASKPlayerCharacter::TraceFromCamera(FHitResult &HitResult, const float TraceDistance,
                                         const TObjectPtr<UMeshComponent> ComponentToIgnore)
{
    FVector TraceStart = PlayerCamera->GetComponentLocation();
    FVector TracecEnd = TraceStart + (PlayerCamera->GetForwardVector() * TraceDistance);

    FCollisionQueryParams TraceParams;
    TraceParams.AddIgnoredComponent(ComponentToIgnore);

    return GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TracecEnd, ECollisionChannel::ECC_Visibility,
                                                TraceParams);
}