// Copyright GMTCK CX.
//
// CXMRPanelUI — the look shared by the operator windows: the tuning window and the control panel.
//
// A row is drawn from an FCXMRTunable (what it is: label, kind, range, options) plus a binding (where its value
// lives). The tuning window binds rows to the registry, so edits are clamped and saved; the control panel binds
// its rows straight to the subsystem. Same widgets and palette in both, changed in one place.
//
// 화면 규칙: 세션 상태(MR/VR·정렬)는 상단 그래파이트 띠에만 둠. 작업 영역은 밝은 회색 위 흰 시트 하나.
// 선택은 파란색만 사용함. 구역은 카드로 나누지 않고 제목과 구분선으로 나눔.

#pragma once

#include "CoreMinimal.h"
#include "CXMRTuningSubsystem.h"
#include "Fonts/SlateFontInfo.h"
#include "Input/Reply.h"

class SWidget;
struct FButtonStyle;
struct FCheckBoxStyle;
struct FScrollBarStyle;
struct FSliderStyle;
struct FSpinBoxStyle;
struct FTextBlockStyle;
struct FSlateBrush;

namespace CXMRPanelUI
{
	/** Where a row reads and writes. The renderer only ever calls these — never the tunable's own lambdas. */
	struct FRowBinding
	{
		TFunction<float()> Get;
		/** bCommit is false while a slider is being dragged, true once the edit is done. */
		TFunction<void(float Value, bool bCommit)> Apply;
		TFunction<void(float Direction)> Step;
		TFunction<void()> Invoke;
		TFunction<FText()> Text;
		/** Greys the row out while false. Unset = always enabled. */
		TFunction<bool()> IsEnabled;
		/** Adds a Reset button. Unset = no button. */
		TFunction<void()> Reset;
	};

	struct FRowSpec
	{
		FCXMRTunable Tunable;
		FRowBinding Binding;
	};

	/** 상태 표시등과 알림 색 구분. */
	enum class ETone : uint8 { Neutral, Positive, Attention, Negative };

	// 색
	FLinearColor BackgroundColor();
	FLinearColor SurfaceColor();
	FLinearColor InkColor();
	FLinearColor HeaderColor();
	FLinearColor ReadoutColor();
	FLinearColor FaintColor();
	FLinearColor HairlineColor();
	FLinearColor SelectionColor();
	FLinearColor SelectionTint();
	FLinearColor RailColor();
	FLinearColor RailInkColor();
	FLinearColor RailMutedColor();
	FLinearColor ToneColor(ETone Tone);

	// 글꼴. 엔진 기본 Roboto만 사용함.
	FSlateFontInfo BodyFont(int32 Size = 12);
	FSlateFontInfo StrongFont(int32 Size = 12);
	FSlateFontInfo HeadingFont(int32 Size = 16);

	// 스타일
	const FTextBlockStyle* BodyTextStyle();
	/** 보조 버튼: 흰 바탕과 얇은 테두리. */
	const FButtonStyle* ButtonStyle();
	/** 주 버튼: 파란 바탕. 화면마다 하나만 씀. */
	const FButtonStyle* PrimaryButtonStyle();
	/** 글자만 있는 버튼. 행의 Reset 등에 씀. */
	const FButtonStyle* QuietButtonStyle();
	/** 배경 없이 hover만 표시함. 선택 상태는 내용물이 그림. */
	const FButtonStyle* FlatButtonStyle();
	const FSpinBoxStyle* SpinStyle();
	const FSliderStyle* SliderStyle();
	const FCheckBoxStyle* CheckStyle();
	const FScrollBarStyle* ScrollBarStyle();

	/** Through the registry by id: clamped, saved on commit, Reset button on saved rows. */
	FRowBinding BindToRegistry(UCXMRTuningSubsystem* Tuning, const FCXMRTunable& Tunable);

	/** Straight to the tunable's own lambdas, clamped to its range. For rows that are not registered. */
	FRowBinding BindDirect(const FCXMRTunable& Tunable);

	/** 페이지 내용을 담는 흰 시트. */
	TSharedRef<SWidget> MakeSurface(const TSharedRef<SWidget>& Content);
	TSharedRef<SWidget> MakeDivider();
	TSharedRef<SWidget> MakeHeader(const FText& Category);
	/** 구역 안의 작은 제목(예: 부품 장착 위치). */
	TSharedRef<SWidget> MakeSubheader(const FText& Text);
	TSharedRef<SWidget> MakeCaption(const TAttribute<FText>& Text);
	/** 색 띠가 붙은 안내문. 문구가 비면 접힘. */
	TSharedRef<SWidget> MakeNotice(const TAttribute<FText>& Text, const TAttribute<ETone>& Tone);
	/** 10px 원형 상태 표시등. */
	TSharedRef<SWidget> MakeLamp(const TAttribute<FSlateColor>& Color);

	/** 라디오 행. 목록에서 하나만 고를 때 씀. */
	TSharedRef<SWidget> MakeChoice(const FText& Label, const TAttribute<bool>& IsSelected, TFunction<FReply()> OnClicked,
		const TAttribute<bool>& IsEnabled = true);
	/** 같은 장착 위치의 A/B 옵션 버튼. */
	TSharedRef<SWidget> MakePill(const FText& Label, const TAttribute<bool>& IsSelected, TFunction<FReply()> OnClicked,
		const TAttribute<bool>& IsEnabled = true);
	/** 왼쪽 탐색 항목. 선택되면 흰 바탕과 파란 막대 표시함. */
	TSharedRef<SWidget> MakeNavItem(const FText& Label, const TAttribute<bool>& IsActive, TFunction<FReply()> OnClicked);
	/** 되돌릴 수 없는 동작. 첫 클릭으로 무장하면 빨간 바탕으로 바뀜. */
	TSharedRef<SWidget> MakeDangerButton(const TAttribute<FText>& Label, const TAttribute<bool>& IsArmed, TFunction<FReply()> OnClicked);
	/** 순서가 있는 단계 표시. State 0 잠김, 1 진행 가능, 2 완료. */
	TSharedRef<SWidget> MakeStepMarker(int32 Number, const TAttribute<int32>& State);

	/** 상단 그래파이트 띠 위의 분할 버튼 묶음. */
	TSharedRef<SWidget> MakeRailGroup(const TSharedRef<SWidget>& Segments);
	TSharedRef<SWidget> MakeRailSegment(const FText& Title, const FText& Detail, const TAttribute<bool>& IsActive,
		const TAttribute<bool>& IsEnabled, TFunction<FReply()> OnClicked);

	TSharedRef<SWidget> MakeRow(const FCXMRTunable& Tunable, const FRowBinding& Binding);

	/** Rows under their category headings, categories in order of first appearance. */
	TSharedRef<SWidget> MakeRowList(const TArray<FRowSpec>& Rows, bool bShowHeadings = true);

	/** A segmented row of tabs; the active one is filled. */
	TSharedRef<SWidget> MakeTabBar(const TArray<FText>& Labels, TFunction<int32()> GetActive, TFunction<void(int32)> Select);

	TSharedRef<SWidget> MakeScroll(const TSharedRef<SWidget>& Content);

	/** 창 공통 배경. */
	TSharedRef<SWidget> MakeBackground(const TSharedRef<SWidget>& Content);
}
