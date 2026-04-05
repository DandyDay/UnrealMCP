#pragma once

#include "CoreMinimal.h"
#include "N1MCPHandlerBase.h"

class FN1MCPDataHandler : public FN1MCPHandlerBase
{
public:
	FN1MCPDataHandler(FN1MCPCommandRegistry& InRegistry);
	virtual void RegisterCommands() override;

private:
	// DataTable
	TSharedPtr<FJsonObject> HandleCreateDataTable(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetDataTableRows(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetDataTableRowNames(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleAddDataTableRow(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleRemoveDataTableRow(const TSharedPtr<FJsonObject>& Params);

	// CurveTable
	TSharedPtr<FJsonObject> HandleGetCurveTableInfo(const TSharedPtr<FJsonObject>& Params);

	// Curve (CurveFloat, CurveLinearColor, CurveVector)
	TSharedPtr<FJsonObject> HandleCreateCurve(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleGetCurveKeys(const TSharedPtr<FJsonObject>& Params);
	TSharedPtr<FJsonObject> HandleSetCurveKeys(const TSharedPtr<FJsonObject>& Params);

	// DataAsset
	TSharedPtr<FJsonObject> HandleGetDataAssetProperties(const TSharedPtr<FJsonObject>& Params);
};
