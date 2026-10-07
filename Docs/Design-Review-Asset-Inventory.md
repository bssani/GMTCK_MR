# 변경된 바이너리 애셋 분류 (2026-10-08)

HEAD 대비 전체 변경 40개. 이번 리뷰 반영에서는 바이너리 애셋을 수정하거나 되돌리지 않았다.
분류는 요청한 유지 대상과 기존 마이그레이션 기록에 따른 것이다. 덤 재저장의 의미적 무변경을 바이너리 diff로 증명한 것은 아니다. 되돌릴지는 사용자가 결정한다.

## 이번 변경에 필요한 것 (14개)

| 애셋 경로 | 이유 |
|---|---|
| `Content/Map/L_Main.umap` | 배치된 Loader의 옛 AttachTarget을 VehicleAnchor로 이관한 맵. |
| `Content/Vehicle/Example/BP_VehicleRoot_TestCar.uasset` | 최소 Root 구조와 Loader 부착 기준을 반영한 차량 Root. |
| `Content/Vehicle/Example/DA_Vehicle_TestCarA.uasset` | 옛 트림/CMF 필드 제거와 디자인 옵션·눈 기준·정렬 프로필 구조를 반영한 재저장. |
| `Content/Vehicle/Example/DA_Vehicle_TestCarB.uasset` | 옛 트림/CMF 필드 제거와 디자인 옵션·눈 기준·정렬 프로필 구조를 반영한 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_CXMR_CycleTrim.uasset` | 제거한 트림 순환 액션 삭제. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_CXMR_PanelClick.uasset` | 제거한 손목 패널 클릭 액션 삭제. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_CXMR_SpinLeft.uasset` | 제거한 좌회전 토글 액션 삭제. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_CXMR_SpinRight.uasset` | 제거한 우회전 토글 액션 삭제. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_CXMR_TurntableAxis.uasset` | 제거한 턴테이블 축 액션 삭제. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_Varjo_DynamicTrackingToggle.uasset` | 사용하지 않는 전체 마커 Dynamic 전환 액션 삭제. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_Varjo_FoveatedRenderingVisualizationToggle.uasset` | 제거한 foveation 디버그 표시 액션 삭제. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_Varjo_GazeVisualizationToggle.uasset` | 제거한 시선 점 표시 액션 삭제. |
| `Plugins/CXMR/Content/Core/Input/IMC_Varjo.uasset` | 제거한 기능의 입력 매핑 8개를 정리한 입력 컨텍스트. |
| `Plugins/CXMR/Content/Core/Pawn/BP_CXMRPawn.uasset` | 제거한 기본 컴포넌트·클릭 바인딩을 정리하고 통합 창 구조를 반영한 Pawn. |

## 마이그레이션 첫 패스의 덤 재저장 (26개)

| 애셋 경로 | 이유 |
|---|---|
| `Content/Vehicle/Example/BP_MaskCube.uasset` | 마스킹 기능은 유지됨. 첫 패스 Blueprint 재컴파일·재저장. |
| `Content/Vehicle/Example/BP_TestCarMesh.uasset` | 고정 더미 차량 모델은 유지됨. 첫 패스 Blueprint 재컴파일·재저장. |
| `Content/Vehicle/Example/DA_Catalog_Test.uasset` | 차량 카탈로그는 유지됨. 첫 패스 패키지 재저장. |
| `Content/Vehicle/Example/DA_Ergonomics_TestCar.uasset` | 선택 Ergonomics 데이터는 유지됨. 첫 패스 패키지 재저장. |
| `Content/Vehicle/Example/DA_MarkerProfile_TestCar.uasset` | 기존 마커 설정은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/BP_CXMRGameMode.uasset` | Pawn 지정 경로는 유지됨. 첫 패스 Blueprint 재컴파일·재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_CXMR_CycleVehicle.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_CXMR_OffsetAdjustX.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_CXMR_OffsetAdjustY.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_CXMR_OffsetAdjustZ.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_CXMR_OffsetReset.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_CXMR_OffsetSave.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_Varjo_DepthTestRangeFarZ.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_Varjo_DepthTestRangeNearZ.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_Varjo_DepthTestRangeToggle.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_Varjo_DepthTestToggle.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_Varjo_EnvironmentDepthEstimationToggle.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_Varjo_HandVisualizationToggle.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_Varjo_MRBackgroundToggle.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_Varjo_MRMaskToggle.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_Varjo_MRToggle.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_Varjo_PlaceVehicle.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_Varjo_Recalibrate.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_Varjo_VarjoMarkerToggle.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Input/Actions/IA_Varjo_ViewOffsetToggle.uasset` | 입력 액션 기능·매핑은 유지됨. 첫 패스 패키지 재저장. |
| `Plugins/CXMR/Content/Core/Materials/PP_MRParameters.uasset` | MR 알파·마스킹 재질 기능은 유지됨. 첫 패스 패키지 재저장. |
