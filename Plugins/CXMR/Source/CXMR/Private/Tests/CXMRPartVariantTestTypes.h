#pragma once
#include "CXMRPartAssembly.h"
#include "CXMRPartVariantTestTypes.generated.h"
// 자동화 월드에서 실제 메시·USB 수명 검증함.
UCLASS(Transient, NotBlueprintable)
class ACXMRPartVariantTestAssembly : public ACXMRPartAssembly
{
	GENERATED_BODY()
public:
	ACXMRPartVariantTestAssembly();
};
UCLASS(Transient, NotBlueprintable)
class ACXMRPartForeignTestAssembly : public ACXMRPartVariantTestAssembly
{
	GENERATED_BODY()
public:
	virtual void OnConstruction(const FTransform& Transform) override;
};
UCLASS(Transient, NotBlueprintable)
class ACXMRPartUnattachedTestAssembly : public ACXMRPartVariantTestAssembly
{
	GENERATED_BODY()
public:
	virtual void OnConstruction(const FTransform& Transform) override;
};
UCLASS(Transient, NotBlueprintable)
class ACXMRPartAuthoredActivityTestAssembly : public ACXMRPartVariantTestAssembly
{
	GENERATED_BODY()
public:
	ACXMRPartAuthoredActivityTestAssembly();
	virtual void OnConstruction(const FTransform& Transform) override;
};
UCLASS(Transient, NotBlueprintable)
class ACXMRPartNestedTestAssembly : public ACXMRPartVariantTestAssembly
{
	GENERATED_BODY()
public:
	ACXMRPartNestedTestAssembly();
};
