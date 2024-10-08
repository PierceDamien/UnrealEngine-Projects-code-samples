// Copyright (c) 2024. Sir Knight title is a property of Quantinum ltd. All rights reserved.

#include "Props/SKInteractableBase.h"
#include "Characters/SKBaseCharacter.h"
#include "Components/CapsuleComponent.h"

ASKInteractableBase::ASKInteractableBase()
{
    PrimaryActorTick.bCanEverTick = false;

    BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>("Object mesh");
    SetRootComponent(BaseMesh);
   
    BaseMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    BaseMesh->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Block);
    BaseMesh->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Ignore);
}

void ASKInteractableBase::BeginPlay() { Super::BeginPlay(); }

void ASKInteractableBase::Tick(float DeltaTime) { Super::Tick(DeltaTime); }

void ASKInteractableBase::OnInteraction_Implementation(const AActor *TriggeredActor)
{
    // YOU SHOULD NOT BE HERE
    checkNoEntry();
}
