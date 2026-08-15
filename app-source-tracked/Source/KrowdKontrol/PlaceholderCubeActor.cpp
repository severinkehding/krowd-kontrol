// Fill out your copyright notice in the Description page of Project Settings.

#include "PlaceholderCubeActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

APlaceholderCubeActor::APlaceholderCubeActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
	RootComponent = MeshComponent;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMeshFinder(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMeshFinder.Succeeded())
	{
		MeshComponent->SetStaticMesh(CubeMeshFinder.Object);
	}
}
