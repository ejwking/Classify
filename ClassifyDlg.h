
// ClassifyDlg.h : header file
//

#pragma once

#include "Display.h"
#include "Annotations.h"
#include "MoreOptionsDlg.h"
#include "GotoPictureDlg.h"
#include "BitmapFile.h"
#include "InputConfigDlg.h"


struct MYWINDOWPOS
{
	// Used for dual purpose, position of the main window and the display pane (not technically a window).
	int x, y, cx, cy;
	int Maximised;
	int FreeSpaceBelow_cy;	// relating to the display pane position.
};

struct OUTPUT_CONFIG
{
	char ClassifiedCsvFile[MAX_PATH];
	BOOL AutoBackup, Exclude_Refused, Exclude_NotClassified;
	BOOL DeleteRefusedPictures;
};

struct PROGRAMFLAGS
{
	BOOL HideCntrls, HideTitlebar, KeyUp, DoneShowWindow, DspTiling, Dragging, CurFilenameGroupDirty;
};

struct CLASSIFIEDCOPY
{
	CLASSIFIEDLINE *pClsfd;
	int Num, Max;
	int Classified;	// CLASSIFIED_STATUS
};

// CClassifyDlg dialog
class CClassifyDlg : public CDialogEx
{
public:
	CDC           *m_pDC;
	CDisplay       m_Display;
	CPoint         m_MousePoint;
	CAnnotations   m_Annotation;
	FILENAMEGROUP *m_pGrp;
	MYWINDOWPOS    m_Window, m_DspPane;
	PROGRAMFLAGS   m_fgs;
	MOREOPTIONS    m_MO;	// this is used by many different files, it should be global, silly keep passing it around as a function parameter.
	GOTOPICTURE    m_GotoPic;
	CLASSIFIEDCOPY m_CCopy;
	INPUT_CONFIG   m_Input;
	OUTPUT_CONFIG  m_Output;
	int            m_Open, m_UnsavedChanges, m_Cycle_box, m_LastSelectedBoxIdx;
	char           m_CurrentPicture[MAX_PATH], m_TempStr[360];

	CClassifyDlg(CWnd* pParent = nullptr);	// standard constructor
	~CClassifyDlg();

	void EnableCntrl(int nID, int Enable);
	void ToggleShowControls();
	void ResizeDisplay(int cx, int cy);
	void MyPaint(int ForceBackgroundRefresh, int InfoDisplaysUpdate=0);
	int  MyKeyDown(int VkCode);
	int  MyWMChar(int ChrCode);
	void LoadProgramConfig();
	void SaveProgramConfig();
	void UpdateInfoDisplays();
	void LoadFilenameGroup();
	void UpdateFilenameGroupStatus(int NewStatus);
	void StepFilenameGroup(int Forward, int End, int Coarse, int NewStatus=0);
	void EnableControlsAccordingly();
	void UnloadCsv();
	void SaveCsv(int SaveAs);
	void ResetFilenameGroup();
	void Restore_FilenameGroup();
	void ChangeClassId(char *pClassLabel, int indexId);
	int  AddBoxToFilenameGroup(BBOX *pbb=nullptr);
	int  GetBboxIndex(CPoint point);
	void SetInfoBoxText();
	void SetInfoBoxText_WithSelection(int Idx);
	void SetInfoBoxText_NoSelection();
	void GetBoxPixelWdHt(BBOX *pBox, int OnOriginal, int &Wd, int &Ht);
	int  SetDirtyAndMakeCopyBeforeChanges();
	void StartEditingBox(int Idx, CPoint point);
	int  SetSelectedBox_Ex(int Idx, int SelectionType);
	void ToggleTiling();
	void PictureSizingKey();
	void IteratePictureSizingScheme();
	void CycleHighlightBox(int delta);
	void ToggleHideTitlebar();
	void DuplicateAnnotation();
	void DeleteBbox(int Idx);
	void SetupDragging(int DragType);
	char *AiAssistBitmapPath();
	int  SHIFT_PointAndClickReclass(CPoint point, char *pClassLabel);

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_CLASSIFY_DIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	virtual BOOL OnInitDialog();
	virtual BOOL DestroyWindow();
	virtual BOOL PreTranslateMessage(MSG* pMsg);

// Implementation
protected:
public:
	HICON m_hIcon;

	// Generated message map functions
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

	afx_msg void OnWindowPosChanged(WINDOWPOS* lpwndpos);
	afx_msg void OnShowWindow(BOOL bShow, UINT nStatus);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);

	afx_msg void OnBnClickedBtnLoadCsv();
	afx_msg void OnBnClickedBtnSave();
	afx_msg void OnBnClickedBtnSaveAs();
	afx_msg void OnBnClickedBtnCsvToYolo();
	afx_msg void OnBnClickedBtnGotoPicture();
	afx_msg void OnBnClickedBtnCsvStatistics();
	afx_msg void OnBnClickedBtnInstructions();
	afx_msg void OnBnClickedBtnMoreOptions();
	afx_msg void OnBnClickedBtnFiltersRefresh();
	afx_msg void OnBnClickedBtnListSortnow();
	afx_msg void OnClose();
};
