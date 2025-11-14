
// ClassifyDlg.cpp : implementation file
// CClassifyDlg dialog

#include "pch.h"
#include "framework.h"
#include "Classify.h"
#include "ClassifyDlg.h"
#include "Useful.h"
#include "MyMsgBox.h"
#include "afxdialogex.h"
#include "AIAssistDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


static int g_EnableRegistry = 1;
#define REG_SECTION	"ClassifyWindow"


CClassifyDlg::CClassifyDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_CLASSIFY_DIALOG, pParent)
{
	g_blocking_WM = 0;

	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pDC   = nullptr;
	m_pGrp  = nullptr;
	m_Open  = 0;
	m_Cycle_box = 0;
	m_LastSelectedBoxIdx = -1;
	m_MousePoint.SetPoint(0, 0);
	m_CurrentPicture[0] = 0;
	m_UnsavedChanges = 0;

	memset(&m_fgs, 0, sizeof(m_fgs));
	memset(&m_MO, 0, sizeof(m_MO));
	memset(&m_GotoPic, 0, sizeof(m_GotoPic));
	memset(&m_Input, 0, sizeof(m_Input));
	memset(&m_Output, 0, sizeof(m_Output));
	memset(&m_CCopy, 0, sizeof(m_CCopy));
	m_fgs.KeyUp = 1;
	m_MO.Display.ShowLabels = 1;
	m_Input.DoColourTestForBlanks = 1;
}

CClassifyDlg::~CClassifyDlg()
{
	// it cannot not already have been released, but just incase.
	SetupDragging(0);
	free(m_CCopy.pClsfd);
}

void CClassifyDlg::DoDataExchange(CDataExchange* pDX)
{
	// Rather pointless this function so far. Do away with it and do things one way or the other, MFC or winapi.
	CDialogEx::DoDataExchange(pDX);

	DDX_Text(pDX, IDC_EDIT_SAVE_PATH, m_Output.ClassifiedCsvFile, num_entries(m_Output.ClassifiedCsvFile));
	DDX_Check(pDX, IDC_CHECK_PIC_FILTERS, m_MO.PicFilter.Apply);
	DDX_Check(pDX, IDC_CHECK_INVERSE_FILTERS, m_MO.PicFilter.Inverse);
	DDX_Check(pDX, IDC_CHECK_EXCLUDE_REFUSE, m_Output.Exclude_Refused);
	DDX_Check(pDX, IDC_CHECK_EXCLUDE_NOT_CLSF, m_Output.Exclude_NotClassified);
}

BEGIN_MESSAGE_MAP(CClassifyDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_WINDOWPOSCHANGED()
	ON_WM_SHOWWINDOW()
	ON_WM_SIZE()
	ON_WM_KEYDOWN()
	ON_WM_MBUTTONDOWN()
	ON_WM_LBUTTONDOWN()
	ON_WM_RBUTTONDOWN()
	ON_WM_MOUSEWHEEL()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONUP()

	ON_BN_CLICKED(IDC_BTN_LOAD_CSV, &CClassifyDlg::OnBnClickedBtnLoadCsv)
	ON_BN_CLICKED(IDC_BTN_SAVE, &CClassifyDlg::OnBnClickedBtnSave)
	ON_BN_CLICKED(IDC_BTN_SAVE_AS, &CClassifyDlg::OnBnClickedBtnSaveAs)
	ON_BN_CLICKED(IDC_BTN_CSV_TO_YOLO, &CClassifyDlg::OnBnClickedBtnCsvToYolo)
	ON_BN_CLICKED(IDC_BTN_GOTO_PICTURE, &CClassifyDlg::OnBnClickedBtnGotoPicture)
	ON_BN_CLICKED(IDC_BTN_CSV_STATISTICS, &CClassifyDlg::OnBnClickedBtnCsvStatistics)
	ON_BN_CLICKED(IDC_BTN_INSTRUCTIONS, &CClassifyDlg::OnBnClickedBtnInstructions)
	ON_BN_CLICKED(IDC_BTN_MORE_OPTIONS, &CClassifyDlg::OnBnClickedBtnMoreOptions)
	ON_BN_CLICKED(IDC_BTN_FILTERS_REFRESH, &CClassifyDlg::OnBnClickedBtnFiltersRefresh)
	ON_BN_CLICKED(IDC_BTN_LIST_SORTNOW, &CClassifyDlg::OnBnClickedBtnListSortnow)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_CLOSE()
END_MESSAGE_MAP()


// CClassifyDlg message handlers

BOOL CClassifyDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here
	LoadProgramConfig();
	LoadEditboxFont(GetDlgItem(IDC_EDIT_INFO));
	UpdateData(0); // to dlg
	EnableControlsAccordingly();
	UpdateInfoDisplays();
	return TRUE;  // return TRUE  unless you set the focus to a control
}

// If you add a minimize button to your dialog, you will need the code below to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.
void CClassifyDlg::OnPaint()
{
	if(IsIconic()){
		CPaintDC dc(this); // device context for painting
		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else{
		CDialogEx::OnPaint();
		m_pDC = this->GetDC();
		MyPaint(0);
	}
}

// The system calls this function to obtain the cursor to display while the user drags the minimized window.
HCURSOR CClassifyDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CClassifyDlg::MyPaint(int ForceBackgroundRefresh, int InfoDisplaysUpdate/*=0*/)
{
	if(m_DspPane.cy>100 && m_pGrp && m_pDC){
		static int NumLines=0;
		if(m_fgs.DspTiling){
			m_MO.Display.PictureSizing = 0;
			if(NumLines != m_pGrp->NumClsfdLines){
				ForceBackgroundRefresh = 1;
				NumLines = m_pGrp->NumClsfdLines;
			}
		}
		int FixedSizeError = m_Display.DrawPictureAndAnnotations(m_pDC, m_DspPane.cx, m_DspPane.cy, m_fgs.DspTiling, m_pGrp, &m_MO.Display, ForceBackgroundRefresh);
		if(FixedSizeError){
			if(m_MO.Display.PictureSizing){
				MsgBox2("fixed picture size didnt fit display area ...program will iterate to the next picture sizing scheme");
				IteratePictureSizingScheme(); // might need 2 tries to get back to 'fit display area' if real size fails and user specified fixed size fails.
				FixedSizeError = m_Display.DrawPictureAndAnnotations(m_pDC, m_DspPane.cx, m_DspPane.cy, m_fgs.DspTiling, m_pGrp, &m_MO.Display, 1);
				if(FixedSizeError && m_MO.Display.PictureSizing){
					IteratePictureSizingScheme(); // might need 2 tries if real size fails and user specified fixed size fails.
					m_Display.DrawPictureAndAnnotations(m_pDC, m_DspPane.cx, m_DspPane.cy, m_fgs.DspTiling, m_pGrp, &m_MO.Display, 1);
				}
			}
			else MsgBox2("code error - MyPaint, Display.PictureSizing %d", m_MO.Display.PictureSizing);
		}
	}

	if(InfoDisplaysUpdate)
		UpdateInfoDisplays();
}

void CClassifyDlg::SaveCsv(int SaveAs)
{
	if(m_Open){
		UpdateData(1); // from dlg - for the save option tickboxes.
		m_Output.DeleteRefusedPictures = 0;
		int Ok = 1;
		if(m_Output.Exclude_NotClassified || m_Output.Exclude_Refused || m_Output.DeleteRefusedPictures)
			Ok = MsgBox_YN("Exclude_NotClassified %d\nExcludeRefusedEntries %d\nDeleteRefusedPictures %d\n\n are you sure?", m_Output.Exclude_NotClassified, m_Output.Exclude_Refused, m_Output.DeleteRefusedPictures);

		if(Ok){
			if(m_Annotation.SaveClassifiedCsvFile(SaveAs, m_Output.Exclude_NotClassified, m_Output.Exclude_Refused, m_Output.DeleteRefusedPictures))
				if(m_Output.Exclude_NotClassified || m_Output.Exclude_Refused)
					MsgBox2("I now recommend that you unload this csv file, and reload the classified csv file that you have just saved.\r\nI say this because you saved the classified csv file with 'refuse' pictures excluded, so the list you currently have loaded does not correspond to the classified csv file you just saved.");
			// if user opened a 'model csv' file then we didnt have a 'classified csv' path initially, but after the first 'save as' we have a 'classified csv' path.
			if(m_Annotation.GetClassifiedCsvPath() != nullptr)
				strcpy_s(m_Output.ClassifiedCsvFile, num_entries(m_Output.ClassifiedCsvFile), m_Annotation.GetClassifiedCsvPath());
			UpdateData(0); // to dlg
			EnableControlsAccordingly();
			m_UnsavedChanges = 0;
			MyPaint(0, 1);
		}
	}
}

void CClassifyDlg::OnBnClickedBtnSave()
{
	if(m_Input.AnnotationProcessing)
		MsgBox2("error - must use 'save as' when saving the 'annotation processing' modified csv");
	else SaveCsv(0);
}
void CClassifyDlg::OnBnClickedBtnSaveAs()
{
	int Ok = 1;
	if(m_Input.AnnotationProcessing)
		if(!MsgBox_YN("be sure to not overwrite the original classified csv with the 'annotation processing' csv\r\n\r\ncontinue to 'save as'?"))
			Ok = 0;
	if(Ok)
		SaveCsv(1);
}

void CClassifyDlg::UnloadCsv()
{
	m_Open = 0;
	m_pGrp = nullptr;
	m_UnsavedChanges = 0;
	m_Output.ClassifiedCsvFile[0] = 0;
	m_Annotation.ClearJob();
	m_Display.DeletePictureObjects();
	m_Display.ClearDisplay(m_pDC, m_DspPane.cx, m_DspPane.cy);
	UpdateData(0); // to dlg
	EnableControlsAccordingly();
	UpdateInfoDisplays();
	GetDlgItem(IDC_EDIT_INPUT_PATH)->SetWindowText("");
}

void CClassifyDlg::OnBnClickedBtnLoadCsv()
{
	if(m_Open){
		int Ok = 1;
		if(m_UnsavedChanges)
			if(MsgBox_YN("Unload csv - there are unsaved changes.\n\nContinue with unload csv and lose changes?") == 0)
				Ok = 0;
		if(Ok)
			UnloadCsv();
	}
	else{
		CInputConfigDlg IPDlg;
		IPDlg.SetInputPathsData(m_Input);
		int ret = (int)IPDlg.DoModal();
		// user might have changed the input paths data, so get it back regardless of whether user pressed OK or Cancel.
		m_Input = IPDlg.GetInputPathsData();
		if(ret == IDOK){
			// Why have flag m_Open when we have m_pGrp which can be used as an 'csv open' flag? ...because depending on the filters 
			// selected there might be no pictures in the current selection, in which case m_pGrp will be null and m_Open will be 1.
			m_pGrp = nullptr;
			if(m_Input.RadioCsvFile == 0){
				m_pGrp = m_Annotation.CreateBlankCsvFile(m_Input.PictureFolder, m_Input.BlanksPictureFolder, &m_MO, &m_Input);
			}
			else if(m_Input.RadioCsvFile == 1){
				m_pGrp = m_Annotation.LoadCsvFile(m_Input.CsvFile[m_Input.IdxCsvFile], m_Input.PictureFolder, &m_MO, &m_Input);
				GetDlgItem(IDC_EDIT_INPUT_PATH)->SetWindowText(m_Input.CsvFile[m_Input.IdxCsvFile]);
			}
			else if(m_Input.RadioCsvFile == 2){
				m_pGrp = m_Annotation.LoadCsvFileList(m_Input.CsvFolder[m_Input.IdxCsvFolder], m_Input.PictureFolder, &m_MO, &m_Input);
				GetDlgItem(IDC_EDIT_INPUT_PATH)->SetWindowText(m_Input.CsvFolder[m_Input.IdxCsvFolder]);
			}
			GetDlgItem(IDC_EDIT_DEBUG)->SetWindowText(m_Annotation.m_MemoryReport);
			if(m_pGrp){
				m_Open = 1;
				if(m_Annotation.GetClassifiedCsvPath() != nullptr)	// we'll only have a classified csv path if the csv file opened was a 'classified csv', as opposed to a 'model csv'.
					strcpy_s(m_Output.ClassifiedCsvFile, num_entries(m_Output.ClassifiedCsvFile), m_Annotation.GetClassifiedCsvPath());
				UpdateData(0); // to dlg
				EnableControlsAccordingly();
				LoadFilenameGroup();
			}
			else MsgBox("failed");
		}
	}
}

void CClassifyDlg::OnBnClickedBtnCsvToYolo()
{
	if(m_Open) m_Annotation.ExportForYOLO(&m_MO);
}

int CClassifyDlg::SetDirtyAndMakeCopyBeforeChanges()
{
	// set 'dirty' flag and make a copy of the classified data before any changes are made to it so we can restore this data if necessary.
	if(!m_fgs.CurFilenameGroupDirty){

		if(m_CCopy.Max < m_pGrp->NumClsfdLines){
			m_CCopy.Max = m_pGrp->NumClsfdLines + 100;
			CLASSIFIEDLINE *pOldBuf = m_CCopy.pClsfd;
			if((m_CCopy.pClsfd = (CLASSIFIEDLINE*)realloc((void*)m_CCopy.pClsfd, m_CCopy.Max*sizeof(CLASSIFIEDLINE))) == 0){
				// realloc failed.
				if(pOldBuf)
					free(pOldBuf);
				MsgBox2("error - SetDirtyAndMakeCopyBeforeChanges - realloc failed - %d", m_CCopy.Max);
				m_CCopy.Max = 0;
			}
		}

		if(m_CCopy.Max >= m_pGrp->NumClsfdLines){
			for(int i=0; i<m_pGrp->NumClsfdLines; i++)
				m_CCopy.pClsfd[i] = m_pGrp->pClsfdLines[i];

			m_CCopy.Num = m_pGrp->NumClsfdLines;
			m_CCopy.Classified = m_pGrp->Classified;
		}
		m_fgs.CurFilenameGroupDirty = 1;
		m_pGrp->Classified = 0; // classified status to 'not classified' while user is making changes.

		// return true if a background repaint is needed because we've changed the classified status.
		return (m_pGrp->Classified != m_CCopy.Classified);
	}
	return 0;
}

void CClassifyDlg::Restore_FilenameGroup()
{
	if(m_fgs.CurFilenameGroupDirty){
		for(int i=0; i<m_CCopy.Num; i++)
			m_pGrp->pClsfdLines[i] = m_CCopy.pClsfd[i];

		m_pGrp->NumClsfdLines = m_CCopy.Num;
		m_pGrp->Classified = m_CCopy.Classified; // Classified is changed to zero as soon as any editting starts, so Classified does get dirty as well as the classifiedlines.
		// The same does not apply to Timestamp, Timestamp is never touched until entry is classified, so Timestamp doesnt get dirty and doesnt need restoring.
		m_fgs.CurFilenameGroupDirty = 0; // not dirty anymore.
	}
}

void CClassifyDlg::UpdateFilenameGroupStatus(int NewStatus)
{
	m_pGrp->Classified = NewStatus;
	m_pGrp->Timestamp = GenerateTimestamp();
	m_UnsavedChanges = 1;
	// classified status is now set and the annotation changes are now committed(saved) to this filename group, so group is not dirty anymore.
	m_fgs.CurFilenameGroupDirty = 0;
}

void CClassifyDlg::StepFilenameGroup(int Forward, int End, int Coarse, int NewStatus/*=0*/)
{
	if(m_Open && m_pGrp){
		// get next filenamegroup ptr, but assign to m_pGrp after classification status has been set for the current filenamegroup.
		FILENAMEGROUP *pFnG;
		int BackupInterval = (m_Output.AutoBackup && (NewStatus!=0)) ? 10 : 0;
		if(Forward) pFnG = End ? m_Annotation.GetEnd(1) : m_Annotation.GetNext(BackupInterval, Coarse);
		else        pFnG = End ? m_Annotation.GetEnd(0) : m_Annotation.GetPrevious(Coarse);

		// set classification status for current group.
		int repaint = 0;
		if(NewStatus == CLASSIFIED_REFUSE){
			// ResetClassifiedLines - restore data to csv 'source' state, not essential but theres no point saving changes if user has refused the entry.
			m_Annotation.ResetClassifiedLines(m_pGrp);
			UpdateFilenameGroupStatus(NewStatus);
			repaint = 1;
		}
		else if(NewStatus==CLASSIFIED_ACCEPT || NewStatus==CLASSIFIED_SPECIAL){
			UpdateFilenameGroupStatus(NewStatus);
			repaint = 1;
		}
		else if(m_fgs.CurFilenameGroupDirty && m_pGrp!=pFnG){
			// entry could have previously editted (and given classified status) in this session, so here we need to *not* restore to 'source' 
			// state, but restore to whatever the entry looked like before editing (this time).
			Restore_FilenameGroup();
			repaint = 1;
		}

		// step to next picture.
		if(m_pGrp != pFnG){
			m_pGrp = pFnG;
			LoadFilenameGroup();
		}
		else if(repaint) // for the case where the picture hasnt changed because we're at the end of the list.
			MyPaint(1);
	}
}

int CClassifyDlg::AddBoxToFilenameGroup(BBOX *pbb/*=nullptr*/)
{
	int Ok = 0;
	if(m_pGrp && !m_fgs.DspTiling){
		m_MO.Display.HideAnnotations = 0; // user was able to start new (GUI) box with annotations hidden, unset this flag now the new box is added so all boxes will display.
		int backgroundrepaint = SetDirtyAndMakeCopyBeforeChanges();
		m_Display.ClearBoxSelections(m_pGrp);
		if(m_pGrp->NumClsfdLines == 1)
			if(m_pGrp->pClsfdLines[0].ClassId < 0)
				m_pGrp->NumClsfdLines--;	// this picture is a 'negative', so turn it into a 'positive' by removing the negative annotation, then adding a bbox with AddClassifiedLine.

		Ok = m_Annotation.AddClassifiedLine(m_pGrp, pbb);
		m_LastSelectedBoxIdx = m_pGrp->NumClsfdLines - 1;
		MyPaint(backgroundrepaint, 1);
	}
	return Ok;
}

void CClassifyDlg::ChangeClassId(char *pClassLabel, int indexId)
{
	// use either pClassLabel or indexId to change class id.
	if(m_pGrp && !m_fgs.DspTiling){
		int Idx = m_Display.GetSelectedBox(m_pGrp, ANY_SELECTION_TYPE);
		Idx = (Idx >= 0) ? Idx : m_LastSelectedBoxIdx;	// m_LastSelectedBoxIdx - allow class id to be changed for the currently highlighted box, or the last selected box - even if it isnt selected anymore.
		if(Idx>=0 && Idx<m_pGrp->NumClsfdLines){		// <NumClsfdLines, being extra careful here as we are also using m_LastSelectedBoxIdx
			int Id = -1;
			if(pClassLabel)
				g_CIdM.GetClassId(pClassLabel, &Id);
			else if(indexId>=0 && indexId<g_CIdM.m_NumClasses)
				Id = indexId;

			if(Id >= 0){
				int backgroundrepaint = SetDirtyAndMakeCopyBeforeChanges();
				m_pGrp->pClsfdLines[Idx].Model_Code = MODEL_BYHAND;
				m_pGrp->pClsfdLines[Idx].ClassId = Id;
				MyPaint(backgroundrepaint, 1);
			}
		}
	}
}

BOOL CClassifyDlg::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	// cycle through the accepted class ids, to be applied to the currently selected bbox.
	if(m_pGrp && !m_fgs.DspTiling){
		int Idx = m_Display.GetSelectedBox(m_pGrp, ANY_SELECTION_TYPE);
		Idx = (Idx >= 0) ? Idx : m_LastSelectedBoxIdx;	// m_LastSelectedBoxIdx - allow class id to be changed for the currently highlighted box, or the last selected box - even if it isnt selected anymore.
		if(Idx>=0 && Idx<m_pGrp->NumClsfdLines){		// Idx<NumClsfdLines, being extra careful here as we are also using m_LastSelectedBoxIdx

			int NumClasses = g_CIdM.m_NumClasses_IncRemoveAndBlurr;
			if(NumClasses > 0){
				int incrm = (zDelta > 0) ? 1 : -1;
				int Cycle_id = m_pGrp->pClsfdLines[Idx].ClassId;
				Cycle_id += incrm;
				if(Cycle_id<0 || Cycle_id>=NumClasses)
					Cycle_id = (incrm>0) ? 0 : NumClasses-1;

				int backgroundrepaint = SetDirtyAndMakeCopyBeforeChanges();
				m_pGrp->pClsfdLines[Idx].Model_Code = MODEL_BYHAND;
				m_pGrp->pClsfdLines[Idx].ClassId = Cycle_id;
				MyPaint(backgroundrepaint, 1);
			}
		}
	}
	return CDialogEx::OnMouseWheel(nFlags, zDelta, pt);
}

void CClassifyDlg::IteratePictureSizingScheme()
{
	if(!m_fgs.DspTiling){
		// iterate through 3 sizing options:
		// 0. fit screen
		// 1. original bitmap size
		// 2. target (user specified fixed dimensions, 'FixedDims') bitmap size
		m_MO.Display.PictureSizing++;
		if(m_MO.Display.PictureSizing == 3)
			m_MO.Display.PictureSizing = 0;

		if(m_MO.Display.PictureSizing == 2)
			// m_MO.Display.FixedDims.Enable - if this is FALSE then dont do (2.)
			if(!m_MO.Display.FixedDims.Enable || m_MO.Display.FixedDims.Ht<=0 || m_MO.Display.FixedDims.Wd<=0)
				m_MO.Display.PictureSizing = 0;
	}
}

void CClassifyDlg::PictureSizingKey()
{
	m_Display.ClearBoxSelections(m_pGrp);
	IteratePictureSizingScheme();
	MyPaint(1, 1);
}

void CClassifyDlg::ToggleTiling()
{
	if(m_Display.GetSelectedBox(m_pGrp, SELECTION_EDITING) < 0){
		m_MO.Display.PictureSizing = 0;
		m_MO.Display.HideAnnotations = 0;
		m_fgs.DspTiling = !m_fgs.DspTiling;
		m_Display.ClearBoxSelections(m_pGrp);
		m_LastSelectedBoxIdx = -1;
		MyPaint(1, 1);
	}
}

void CClassifyDlg::ResetFilenameGroup()
{
	// ...warning that changes will be lost? no that would waste time and be annoying.
	if(m_pGrp){
		m_MO.Display.HideAnnotations = 0;
		int backgroundrepaint = 1;
		//SetDirtyAndMakeCopyBeforeChanges(); not this call, restoring to source is not making the data 'dirty', its making it clean.
		m_fgs.CurFilenameGroupDirty = 0;
		// restore to 'source csv' values.
		m_Annotation.ResetClassifiedLines(m_pGrp);
		m_Display.ClearBoxSelections(m_pGrp);
		m_LastSelectedBoxIdx = -1;
		MyPaint(backgroundrepaint, 1);
	}
}

int CClassifyDlg::SetSelectedBox_Ex(int Idx, int SelectionType)
{
	if(m_Display.SetSelectedBox(m_pGrp, SelectionType, Idx)){
		m_LastSelectedBoxIdx = Idx;
		return 1;
	}
	return 0;
}

void CClassifyDlg::StartEditingBox(int Idx, CPoint point)
{
	if(!m_fgs.DspTiling){
		if(SetSelectedBox_Ex(Idx, SELECTION_EDITING)){
			m_Display.SetMovementAnchor(point.x, point.y);
			MyPaint(0, 1);
		}
	}
}

#define DRAGGING_SELECTED_BOX	1
#define DRAGGING_GUI_BOX		2	// calling this a GUI box because it only exists in the UI until its completed, not in the annotations list.
void CClassifyDlg::OnMouseMove(UINT nFlags, CPoint point)
{
	if(m_pGrp && !m_fgs.DspTiling){
		int donePaint = 0;
		m_MousePoint = point;
		if(m_fgs.Dragging == DRAGGING_SELECTED_BOX){
			m_Display.UpdateAnchorPosition(m_pGrp, point.x, point.y, m_fgs.DspTiling);
			MyPaint(0, 1);
			donePaint = 1;
		}
		else if(m_fgs.Dragging == DRAGGING_GUI_BOX){
			m_Display.MoveUIDefinedBox(point.x, point.y);
			MyPaint(0);
			donePaint = 1;
		}
		else{
			int Idx = m_Display.GetSelectedBox(m_pGrp, SELECTION_EDITING);
			if(Idx >= 0){
				if(m_Display.SetMovementAnchor(point.x, point.y)){
					MyPaint(0);
					donePaint = 1;
				}
			}
		}

		if(!donePaint){
			if(m_Display.SetCrossHairsPosition(point.x, point.y))
				MyPaint(0);
			//OutputDbgStr("\n cross hairs");
		}
	}
	CDialogEx::OnMouseMove(nFlags, point);
}

void CClassifyDlg::SetupDragging(int DragType)
{
	if(DragType == 0){
		m_fgs.Dragging = 0;
		if(GetCapture() == this)
			ReleaseCapture();
	}
	else{
		m_fgs.Dragging = DragType;
		SetCapture();
	}
}

void CClassifyDlg::OnRButtonUp(UINT nFlags, CPoint point)
{
	int DefaultHandling = 1;
	if(m_pGrp && m_fgs.Dragging){
		if(m_fgs.Dragging == DRAGGING_SELECTED_BOX){
			m_Display.UpdateAnchorPosition(m_pGrp, point.x, point.y, m_fgs.DspTiling);
		//	m_Display.ClearBoxSelections(m_pGrp); dont un-select, so we can reposition different sides without having to re-select box.
			MyPaint(0, 1);
		}
		else if(m_fgs.Dragging == DRAGGING_GUI_BOX){
			m_Display.MoveUIDefinedBox(point.x, point.y);
			BBOX bb;
			if(m_Display.GetUIDefinedBox(bb, 5))
				AddBoxToFilenameGroup(&bb);
			else MyPaint(0);
		}
		DefaultHandling = 0;
		SetupDragging(0);
    }
	if(DefaultHandling)
		CDialogEx::OnRButtonUp(nFlags, point);
}

#define MOUSE_ACTION_CONDITIONS (m_pGrp && m_fgs.Dragging==0 && !m_MO.Display.HideAnnotations)
void CClassifyDlg::OnRButtonDown(UINT nFlags, CPoint point)
{
//	if(MOUSE_ACTION_CONDITIONS && !m_fgs.DspTiling){
	if(m_pGrp && m_fgs.Dragging==0 && !m_fgs.DspTiling){ // I want to allow new box creation regardless of the flag m_MO.Display.HideAnnotations state.
		int Idx = m_Display.GetSelectedBox(m_pGrp, SELECTION_EDITING);
		if(Idx >= 0){
			// drag selected box.
			int backgroundrepaint = SetDirtyAndMakeCopyBeforeChanges();
			m_pGrp->pClsfdLines[Idx].Model_Code = MODEL_BYHAND;
			SetupDragging(DRAGGING_SELECTED_BOX);
			m_Display.UpdateAnchorPosition(m_pGrp, point.x, point.y, m_fgs.DspTiling);
			MyPaint(backgroundrepaint);
		}
		else if(!SHIFT_PointAndClickReclass(point, g_CIdM.m_Id_MouseRBtnDown)){
			// create new (GUI) box and start dragging.
			if(m_Display.StartUIDefinedBox(point.x, point.y)){
				m_Display.ClearBoxSelections(m_pGrp);
				SetupDragging(DRAGGING_GUI_BOX);
			}
		}
	}
	CDialogEx::OnRButtonDown(nFlags, point);
}

void CClassifyDlg::OnMButtonDown(UINT nFlags, CPoint point)
{
	if(MOUSE_ACTION_CONDITIONS)
		if(!SHIFT_PointAndClickReclass(point, g_CIdM.m_Id_MouseMBtnDown))
			DeleteBbox(GetBboxIndex(point)); // delete clicked on box.
	CDialogEx::OnMButtonDown(nFlags, point);
}

void CClassifyDlg::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	if(MOUSE_ACTION_CONDITIONS)
		SHIFT_PointAndClickReclass(point, g_CIdM.m_Id_MouseLBtnDlbClk);
	CDialogEx::OnLButtonDblClk(nFlags, point);
}

void CClassifyDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default
	CDialogEx::OnLButtonUp(nFlags, point);
}

void CClassifyDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
	if(MOUSE_ACTION_CONDITIONS && !m_fgs.DspTiling){
		if(m_Display.GetSelectedBox(m_pGrp, ANY_SELECTION_TYPE) >= 0){
			// un-select currently selected bbox.
			m_Display.ClearBoxSelections(m_pGrp);
			MyPaint(0, 1);
		}
		else if(!SHIFT_PointAndClickReclass(point, g_CIdM.m_Id_MouseLBtnDown)){
			// select box for editing.
			int Idx = m_Display.GetBboxIdxFromScreenCoords(m_pGrp, point.x, point.y);
			StartEditingBox(Idx, point);
		}
	}
	CDialogEx::OnLButtonDown(nFlags, point);
}

int CClassifyDlg::SHIFT_PointAndClickReclass(CPoint point, char *pClassLabel)
{
	// special case, if shift is held down and mouse button clicked, re-class bbox.
	if(!m_fgs.DspTiling && !m_MO.Display.HideAnnotations){
		short vks = GetKeyState(VK_SHIFT);
		if(vks < 0){
			m_Display.ClearBoxSelections(m_pGrp);
			int Idx = m_Display.GetBboxIdxFromScreenCoords(m_pGrp, point.x, point.y);
			if(Idx >= 0){
				m_LastSelectedBoxIdx = Idx;
				ChangeClassId(pClassLabel, -1);
			}
			return 1;
		}
	}
	return 0;
}

void CClassifyDlg::DeleteBbox(int Idx)
{
	if(Idx>=0 && m_fgs.Dragging==0){
		int backgroundrepaint = SetDirtyAndMakeCopyBeforeChanges();
		if(m_pGrp->NumClsfdLines == 1){
			// user has deleted all the bboxes, therefore turn this last annotation into a 'negative', so picture is now a 'negative'.
			m_pGrp->pClsfdLines[0].ClassId = -1;
			memset(&m_pGrp->pClsfdLines[0].box, 0, sizeof(m_pGrp->pClsfdLines[0].box));
		}
		else{
			// remove bbox at this index.
			for(int i=Idx; i<m_pGrp->NumClsfdLines-1; i++)
				m_pGrp->pClsfdLines[i] = m_pGrp->pClsfdLines[i+1];
			m_pGrp->NumClsfdLines--;
		}
		m_Display.ClearBoxSelections(m_pGrp);
		m_Cycle_box = 0;
		m_LastSelectedBoxIdx = -1;
		MyPaint(backgroundrepaint, 1);
	}
}

int CClassifyDlg::GetBboxIndex(CPoint point)
{
	int Idx = -1;
	if(m_Open && m_pGrp && !m_MO.Display.HideAnnotations){
		if(m_fgs.DspTiling) Idx = m_Display.GetTileIdxFromScreenCoords(point.x, point.y);
		else                Idx = m_Display.GetBboxIdxFromScreenCoords(m_pGrp, point.x, point.y);
	}
	return Idx;
}

void CClassifyDlg::LoadFilenameGroup()
{
	if(m_pGrp && m_fgs.Dragging==0){
		m_fgs.DspTiling = 0;
		m_MO.Display.HideAnnotations = 0;
		m_fgs.CurFilenameGroupDirty = 0;
		m_Cycle_box = 0;
		m_LastSelectedBoxIdx = -1;
		m_Display.ClearBoxSelections(m_pGrp);
		m_Display.HideCrossHairs();
		m_Annotation.GetPicturePath(m_pGrp, m_CurrentPicture, num_entries(m_CurrentPicture));
		GetDlgItem(IDC_EDIT_CUR_PIC_PATH)->SetWindowText(m_CurrentPicture);
		if(m_Display.LoadPicture(m_CurrentPicture, m_pDC)){
			MyPaint(1, 1);
			OutputDbgStr("\n%s   ", m_CurrentPicture);
		}
		else{
			m_Display.ClearDisplay(m_pDC, m_DspPane.cx, m_DspPane.cy);
			UpdateInfoDisplays();
			MsgBox2("error - couldnt open picture\r\n%s\r\n suggested action - refuse this entry and continue.", m_CurrentPicture);
		}
	}
}

void CClassifyDlg::OnBnClickedBtnCsvStatistics()
{
	if(m_Open)
		m_Annotation.StatisticsPopup(&m_MO);
}

static UINT g_ctrlIDS[] = {
	IDC_STATIC_OUTPUT, IDC_BTN_INSTRUCTIONS, IDC_EDIT_INPUT_PATH, IDC_CHECK_EXCLUDE_REFUSE, IDC_CHECK_EXCLUDE_NOT_CLSF,
	IDC_BTN_CSV_TO_YOLO, IDC_BTN_SAVE, IDC_BTN_SAVE_AS, IDC_BTN_LOAD_CSV, IDC_BTN_MORE_OPTIONS, IDC_EDIT_SAVE_PATH, 
	IDC_EDIT_INFO, IDC_EDIT_DEBUG, IDC_BTN_CSV_STATISTICS, IDC_EDIT_CUR_PIC_PATH, IDC_BTN_GOTO_PICTURE, 
	IDC_STATIC_FILTERS, IDC_CHECK_PIC_FILTERS, IDC_CHECK_INVERSE_FILTERS, IDC_BTN_FILTERS_REFRESH, IDC_BTN_LIST_SORTNOW,
};

void CClassifyDlg::ToggleShowControls()
{
//	if(m_Display.GetSelectedBox(m_pGrp, SELECTION_EDITING) < 0) not doing this, instead use ClearBoxSelections

	m_Display.ClearBoxSelections(m_pGrp);
	m_fgs.HideCntrls = !m_fgs.HideCntrls;
	int Num = num_entries(g_ctrlIDS);
	for(int i=0; i<Num; i++)
		GetDlgItem(g_ctrlIDS[i])->ShowWindow(!m_fgs.HideCntrls);
	RECT R;
	GetClientRect(&R);
	ResizeDisplay(R.right-R.left, R.bottom-R.top);
}

void CClassifyDlg::EnableCntrl(int nID, int Enable)
{
	GetDlgItem(nID)->EnableWindow(Enable);
}

void CClassifyDlg::EnableControlsAccordingly()
{
	// ???? - TURN OFF TAB STOP FOR ALL CONTROLS ????
	GetDlgItem(IDC_BTN_LOAD_CSV)->SetWindowText(m_Open ? "unload" : "load..");
	if(m_Open)
		GetDlgItem(IDC_STATIC_FILTERS)->SetFocus();
	EnableCntrl(IDC_CHECK_PIC_FILTERS,     m_Open);
	EnableCntrl(IDC_CHECK_INVERSE_FILTERS, m_Open);
	EnableCntrl(IDC_BTN_FILTERS_REFRESH,   m_Open);
	EnableCntrl(IDC_BTN_LIST_SORTNOW,      m_Open);
	EnableCntrl(IDC_BTN_CSV_TO_YOLO,       m_Open);
	EnableCntrl(IDC_BTN_CSV_STATISTICS,    m_Open);
	EnableCntrl(IDC_BTN_SAVE_AS,           m_Open);
	EnableCntrl(IDC_CHECK_EXCLUDE_REFUSE,  m_Open);
	EnableCntrl(IDC_CHECK_EXCLUDE_NOT_CLSF,m_Open);
	EnableCntrl(IDC_BTN_GOTO_PICTURE,      m_Open);
	EnableCntrl(IDC_BTN_SAVE,              m_Open && m_Output.ClassifiedCsvFile[0]!=0 && !m_Input.AnnotationProcessing);
	// dont actually need to enable/disable the edit boxes below, they do nothing bad when enabled.
	EnableCntrl(IDC_EDIT_SAVE_PATH,        m_Open && m_Output.ClassifiedCsvFile[0]!=0 );
	EnableCntrl(IDC_EDIT_CUR_PIC_PATH,     m_Open);
	EnableCntrl(IDC_EDIT_INPUT_PATH,       m_Open);
}

void CClassifyDlg::UpdateInfoDisplays()
{
	#define SHOWCNTRLSMSG	m_fgs.HideCntrls ? " - KEY 'C' TO SHOW CONTROLS" : ""
	if(m_Open){
		int Wd=0, Ht=0, dspWd=0, dspHt=0, SourceNum=0;
		int NumFnG = m_Annotation.GetFNGCount(&SourceNum);
		if(NumFnG > 0){
			char zoomStr[12] = {0};
			float Scale=0.0f, aspect=0.0f;
			m_Display.GetPictureDims(Wd, Ht, Scale, dspWd, dspHt);
			if(Wd>0 && Ht>0){
				aspect = (float)Wd / (float)Ht;
				if(m_MO.Display.PictureSizing != 2)
					sprintf_s(zoomStr, num_entries(zoomStr), " zoom%d%%", (int)(Scale*100.0f));
			}
			char *pSave = m_UnsavedChanges ? "* ":"";
			sprintf_s(m_TempStr, num_entries(m_TempStr), "%s%d/%d", pSave, m_Annotation.GetFNGIndex()+1, NumFnG);
			if(NumFnG != SourceNum)
				sprintf_s(m_TempStr, num_entries(m_TempStr), "%s/%d", m_TempStr, SourceNum);
			sprintf_s(m_TempStr, num_entries(m_TempStr), "%s - bmp[w%d h%d ratio%.3f] screen[w%d h%d%s] - %s%s", m_TempStr, Wd, Ht, aspect, dspWd, dspHt, zoomStr, m_CurrentPicture, SHOWCNTRLSMSG);
		}
		else
			sprintf_s(m_TempStr, num_entries(m_TempStr), "[%d] **no pictures to display** %s", SourceNum, SHOWCNTRLSMSG);
		// window title bar text.
		SetWindowText(m_TempStr);
	}
	else{
#ifdef _DEBUG
		SetWindowText("Object detection annotation tools - DEBUG");
#else
		SetWindowText("Object detection annotation tools - RELEASE");
#endif
	}
	SetInfoBoxText();
}

void CClassifyDlg::GetBoxPixelWdHt(BBOX *pBox, int OnOriginal, int &Wd, int &Ht)
{
	if(OnOriginal) BboxToPixelWdHt_Inc_Exc(Wd, Ht, m_pGrp->pPicInfo->Width, m_pGrp->pPicInfo->Height, pBox);
	else           BboxToPixelWdHt_Inc_Exc(Wd, Ht, m_MO.General.TargetWidth, m_MO.General.TargetHeight, pBox);
}

//#define UPDOWNMATCHSTR(ori, tar) (ori==tar)?"match":((ori>tar)?"downscale":"upscale")
void CClassifyDlg::SetInfoBoxText()
{
	if(m_pGrp){
		CString str, tmp; // (use 'sprintf_s(m_TempStr,' instead for more efficiency)
		int W, H, Idx = m_Display.GetSelectedBox(m_pGrp, ANY_SELECTION_TYPE);
		if(Idx >= 0){
			CLASSIFIEDLINE *pCL = &m_pGrp->pClsfdLines[Idx];
			if(pCL->Model_Code>=0 && pCL->Model_Code<MODELCODE::MODEL_NUM_ENTRIES)
				str.Format("%s", g_pModelCode[pCL->Model_Code]);

			if(pCL->DebugCode != 0){
				int bits = sizeof(pCL->DebugCode) * 8;
				tmp.Format("\r\nDebugCodes (%dbit) ", bits);
				str += tmp;
				for(int i=0; i<bits; i++)
					if(pCL->DebugCode&(1<<i)){ tmp.Format("%d.", i); str += tmp; }
			}
			float w = pCL->box.x2-pCL->box.x1, h = pCL->box.y2-pCL->box.y1;
			tmp.Format("\r\n%s :\r\n  x[%.4f, %.4f] y[%.4f, %.4f]\r\n  w%.4f h%.4f area%.4f\r\n", g_CIdM.GetLabel(pCL->ClassId), pCL->box.x1, pCL->box.x2, pCL->box.y1, pCL->box.y2, w, h, w*h);
			str += tmp;
			GetBoxPixelWdHt(&pCL->box, 1, W, H);
			tmp.Format("  w%d h%d pix on original (w%d, h%d) - ratio %.3f\r\n", W, H, m_pGrp->pPicInfo->Width, m_pGrp->pPicInfo->Height, (H!=0) ? ((float)W/(float)H) : 0.0f);
			str += tmp;
			if(m_MO.General.TargetWidth>0 && m_MO.General.TargetHeight>0){
				GetBoxPixelWdHt(&pCL->box, 0, W, H);
				tmp.Format("  w%d h%d pix on target (w%d, h%d)\r\n", W, H, m_MO.General.TargetWidth, m_MO.General.TargetHeight);
				str += tmp;
			}
			else{
				tmp.Format("\r\nset target dimensions in options dialog to see target data here");
				str += tmp;
			}
		//	tmp.Format(" original w:%s h:%s to target\r\n", UPDOWNMATCHSTR(m_pGrp->pPicInfo->Width, m_MO.General.TargetWidth), UPDOWNMATCHSTR(m_pGrp->pPicInfo->Height, m_MO.General.TargetHeight));
		//	tmp.Format(" w%d h%d minimum\r\n", min(W1, W2), min(H1, H2));
		//	str += tmp;
		}
		else{
			str.Format("Timestamp: %s, %s, %s\r\nAnnotations %d: ", TimestampToString(m_pGrp->Timestamp), g_pColourCode[m_pGrp->pPicInfo->ColourCode], (m_pGrp->pPicInfo->PictureCode>0)?g_pPictureCode[m_pGrp->pPicInfo->PictureCode]:"", m_pGrp->NumClsfdLines);
			if(m_pGrp->NumClsfdLines>0 && m_pGrp->pClsfdLines[0].ClassId>=0){
				int i, labelCount[_MAX_CLASSES_]={0}, MinHt[_MAX_CLASSES_]={0}, MinWd[_MAX_CLASSES_]={0};
				// dont use g_CIdM.m_NumClasses here because i want to see statistics for the 'debug' classes too.
				CLASSIFIEDLINE *pCL = m_pGrp->pClsfdLines;
				for(i=0; i<m_pGrp->NumClsfdLines; i++, pCL++){
					if(pCL->ClassId>=0 && pCL->ClassId<_MAX_CLASSES_){	// only need to test its not -1 actually.
						labelCount[pCL->ClassId]++;
						GetBoxPixelWdHt(&pCL->box, 1, W, H);
						if(MinHt[pCL->ClassId] == 0) MinHt[pCL->ClassId] = H;
						else MinHt[pCL->ClassId] = min(MinHt[pCL->ClassId], H);
						if(MinWd[pCL->ClassId] == 0) MinWd[pCL->ClassId] = W;
						else MinWd[pCL->ClassId] = min(MinWd[pCL->ClassId], W);
					}
				}
				for(i=0; i<_MAX_CLASSES_; i++)
					if(labelCount[i] > 0){
						tmp.Format("\r\n  %s : %d  -  min(w %d, h %d)", g_CIdM.GetLabel(i), labelCount[i], MinWd[i], MinHt[i]);
						str += tmp;
					}
			}
			else str += "\r\n negative picture";
		}
		GetDlgItem(IDC_EDIT_INFO)->SetWindowText(str);
	}
	else GetDlgItem(IDC_EDIT_INFO)->SetWindowText("when editing annotations have window as big as possible to reduce rounding errors.\r\n\r\nload your annotations csv file to begin..");
}

void CClassifyDlg::OnBnClickedBtnInstructions()
{
	CMyMsgBox MB;
	MB.SetInstructionsText(m_Open);
	MB.DoModal();
}

void CClassifyDlg::LoadProgramConfig()
{
	if(g_EnableRegistry){
		m_MO.General.TargetWidth   = AfxGetApp()->GetProfileInt(REG_SECTION, "General.TargetWidth", 0);
		m_MO.General.TargetHeight  = AfxGetApp()->GetProfileInt(REG_SECTION, "General.TargetHeight", 0);

		m_MO.PicFilter.MinDims.Enable = AfxGetApp()->GetProfileInt(REG_SECTION, "PicFilter.MinDims.Enable", 0);
		m_MO.PicFilter.MinDims.Wd     = AfxGetApp()->GetProfileInt(REG_SECTION, "PicFilter.MinDims.Wd", 0);
		m_MO.PicFilter.MinDims.Ht     = AfxGetApp()->GetProfileInt(REG_SECTION, "PicFilter.MinDims.Ht", 0);

		m_MO.Display.FixedDims.Enable = AfxGetApp()->GetProfileInt(REG_SECTION, "Display.FixedDims.Enable", 0);
		m_MO.Display.FixedDims.Wd     = AfxGetApp()->GetProfileInt(REG_SECTION, "Display.FixedDims.Wd", 0);
		m_MO.Display.FixedDims.Ht     = AfxGetApp()->GetProfileInt(REG_SECTION, "Display.FixedDims.Ht", 0);
		m_MO.Display.IncreaseLineThickness = AfxGetApp()->GetProfileInt(REG_SECTION, "Display.IncreaseLineThickness", 1);
		m_MO.Display.HighlightHeight = AfxGetApp()->GetProfileInt(REG_SECTION, "Display.HighlightHeight", 0);

		// main dialog settings.
		m_Window.cx = AfxGetApp()->GetProfileInt(REG_SECTION, "Window.cx", 0);
		m_Window.cy = AfxGetApp()->GetProfileInt(REG_SECTION, "Window.cy", 0);
		m_Window.x  = AfxGetApp()->GetProfileInt(REG_SECTION, "Window.x",  0);
		m_Window.y  = AfxGetApp()->GetProfileInt(REG_SECTION, "Window.y",  0);

		m_Input.MergeFiles   = AfxGetApp()->GetProfileInt(REG_SECTION, "m_Input.MergeFiles", 0);
		m_Input.RadioCsvFile = AfxGetApp()->GetProfileInt(REG_SECTION, "m_Input.RadioCsvFile", 0);
		m_Input.IdxCsvFile   = AfxGetApp()->GetProfileInt(REG_SECTION, "m_Input.IdxCsvFile", 0);
		m_Input.IdxCsvFolder = AfxGetApp()->GetProfileInt(REG_SECTION, "m_Input.IdxCsvFolder", 0);
		strcpy_s(m_Input.PictureFolder, num_entries(m_Input.PictureFolder), AfxGetApp()->GetProfileString(REG_SECTION, "m_Input.PictureFolder", ""));
		strcpy_s(m_Input.BlanksPictureFolder, num_entries(m_Input.BlanksPictureFolder), AfxGetApp()->GetProfileString(REG_SECTION, "m_Input.BlanksPictureFolder", ""));
		char Str[128];
		for(int i=0; i<MAX_INPATHLIST; i++){
			sprintf_s(Str, num_entries(Str), "m_Input.CsvFile[%d]", i);
			strcpy_s(m_Input.CsvFile[i], num_entries(m_Input.CsvFile[i]), AfxGetApp()->GetProfileString(REG_SECTION, Str, ""));
			sprintf_s(Str, num_entries(Str), "m_Input.CsvFolder[%d]", i);
			strcpy_s(m_Input.CsvFolder[i], num_entries(m_Input.CsvFolder[i]), AfxGetApp()->GetProfileString(REG_SECTION, Str, ""));
		}
	}
}

void CClassifyDlg::SaveProgramConfig()
{
	if(g_EnableRegistry){
		AfxGetApp()->WriteProfileInt(REG_SECTION, "General.TargetWidth",   m_MO.General.TargetWidth);
		AfxGetApp()->WriteProfileInt(REG_SECTION, "General.TargetHeight",  m_MO.General.TargetHeight);

		AfxGetApp()->WriteProfileInt(REG_SECTION, "PicFilter.MinDims.Enable", m_MO.PicFilter.MinDims.Enable);
		AfxGetApp()->WriteProfileInt(REG_SECTION, "PicFilter.MinDims.Wd", m_MO.PicFilter.MinDims.Wd);
		AfxGetApp()->WriteProfileInt(REG_SECTION, "PicFilter.MinDims.Ht", m_MO.PicFilter.MinDims.Ht);

		AfxGetApp()->WriteProfileInt(REG_SECTION, "Display.FixedDims.Enable", m_MO.Display.FixedDims.Enable);
		AfxGetApp()->WriteProfileInt(REG_SECTION, "Display.FixedDims.Wd",  m_MO.Display.FixedDims.Wd);
		AfxGetApp()->WriteProfileInt(REG_SECTION, "Display.FixedDims.Ht", m_MO.Display.FixedDims.Ht);
		AfxGetApp()->WriteProfileInt(REG_SECTION, "Display.IncreaseLineThickness", m_MO.Display.IncreaseLineThickness);
		AfxGetApp()->WriteProfileInt(REG_SECTION, "Display.HighlightHeight", m_MO.Display.HighlightHeight);

		// main dialog settings.
		AfxGetApp()->WriteProfileInt(REG_SECTION, "Window.cx", m_Window.cx);
		AfxGetApp()->WriteProfileInt(REG_SECTION, "Window.cy", m_Window.cy);
		AfxGetApp()->WriteProfileInt(REG_SECTION, "Window.x",  m_Window.x);
		AfxGetApp()->WriteProfileInt(REG_SECTION, "Window.y",  m_Window.y);
		
		AfxGetApp()->WriteProfileInt(REG_SECTION, "m_Input.MergeFiles", m_Input.MergeFiles);
		AfxGetApp()->WriteProfileInt(REG_SECTION, "m_Input.RadioCsvFile", m_Input.RadioCsvFile);
		AfxGetApp()->WriteProfileInt(REG_SECTION, "m_Input.IdxCsvFile", m_Input.IdxCsvFile);
		AfxGetApp()->WriteProfileInt(REG_SECTION, "m_Input.IdxCsvFolder", m_Input.IdxCsvFolder);
		AfxGetApp()->WriteProfileString(REG_SECTION, "m_Input.PictureFolder", m_Input.PictureFolder);
		AfxGetApp()->WriteProfileString(REG_SECTION, "m_Input.BlanksPictureFolder", m_Input.BlanksPictureFolder);
		char Str[128];
		for(int i=0; i<MAX_INPATHLIST; i++){
			sprintf_s(Str, num_entries(Str), "m_Input.CsvFile[%d]", i);
			AfxGetApp()->WriteProfileString(REG_SECTION, Str, m_Input.CsvFile[i]);
			sprintf_s(Str, num_entries(Str), "m_Input.CsvFolder[%d]", i);
			AfxGetApp()->WriteProfileString(REG_SECTION, Str, m_Input.CsvFolder[i]);
		}
	}
}

void CClassifyDlg::DuplicateAnnotation()
{
	if(m_pGrp){
		int Idx = m_Display.GetSelectedBox(m_pGrp, ANY_SELECTION_TYPE);
		if(Idx >= 0){
			int Car, Van, Truck, Bus, cid;
			g_CIdM.GetClassId("car", &Car);
			g_CIdM.GetClassId("van", &Van);
			g_CIdM.GetClassId("truck", &Truck);
			g_CIdM.GetClassId("bus", &Bus);
			cid = g_CIdM.GetPreferredId();
			// best guess suggestion for class id of duplicate, user doesnt have to use suggestion of course.
				 if(m_pGrp->pClsfdLines[Idx].ClassId == Van) cid = Car;
			else if(m_pGrp->pClsfdLines[Idx].ClassId == Car) cid = Van;
			else if(m_pGrp->pClsfdLines[Idx].ClassId == Truck) cid = Van;
			else if(m_pGrp->pClsfdLines[Idx].ClassId == Bus) cid = Van;
			
			if(cid >= 0){
				BBOX bb = m_pGrp->pClsfdLines[Idx].box;
				if(AddBoxToFilenameGroup(&bb))
					m_pGrp->pClsfdLines[m_pGrp->NumClsfdLines-1].ClassId = cid;
				MyPaint(0, 1);
			}
		}
	}
}

void CClassifyDlg::ToggleHideTitlebar()
{
	// can I remove the titlebar, temporarily? i know i can in the resource editor, but not sure i can switch it on and off while the window exists.
	// ShowWindow(SW_SHOWMAXIMIZED)
/*	CRect Rt;
	GetWindowRect(&Rt);
	m_fgs.HideTitlebar = !m_fgs.HideTitlebar;
	int cx = 40;
	if(!m_fgs.HideTitlebar) cx = -cx;
	Rt.top -= cx;
	SetWindowPos(NULL, Rt.left, Rt.top, Rt.Width(), Rt.Height(), 0);*/
}

void CClassifyDlg::OnWindowPosChanged(WINDOWPOS* lpwndpos)
{
	CDialogEx::OnWindowPosChanged(lpwndpos);
	if(m_fgs.DoneShowWindow){
		WINDOWPLACEMENT wm;
		GetWindowPlacement(&wm);
		m_Window.Maximised = (wm.flags == 2);
		if(!m_Window.Maximised){
			int ScreenCX = GetSystemMetrics(SM_CXVIRTUALSCREEN);
			int ScreenCY = GetSystemMetrics(SM_CYVIRTUALSCREEN);
			if(lpwndpos->x>=-20 && lpwndpos->y>=-20 && lpwndpos->cx<=ScreenCX+10 && lpwndpos->cy<=ScreenCY+10 && lpwndpos->cx>=300 && lpwndpos->cy>=200){	// so we dont lose the window off the screen.
				m_Window.x  = lpwndpos->x;
				m_Window.y  = lpwndpos->y;
				m_Window.cx = lpwndpos->cx;
				m_Window.cy = lpwndpos->cy;
			}
		}
		// OutputDbgStr("\n OnWindowPosChanged cx %d cy %d x %d y %d FLAGS %d \n", lpwndpos->cx, lpwndpos->cy, lpwndpos->x, lpwndpos->y, wm.flags);
	}
}

void CClassifyDlg::OnShowWindow(BOOL bShow, UINT nStatus)
{
	CDialogEx::OnShowWindow(bShow,nStatus);
	// TODO: Add your message handler code here
	if(m_Window.cx>0 && m_Window.cy>0){
		int Flags = 0;
		SetWindowPos(NULL, m_Window.x, m_Window.y, m_Window.cx, m_Window.cy, Flags);
	}
	m_fgs.DoneShowWindow = 1;
}

void CClassifyDlg::ResizeDisplay(int cx, int cy)
{
	m_DspPane.FreeSpaceBelow_cy = m_fgs.HideCntrls ? 0 : 155;
	m_DspPane.cx = cx;
	m_DspPane.cy = cy - m_DspPane.FreeSpaceBelow_cy;
	static int prvCY = 0;
	if(m_DspPane.cy < prvCY){
		// Window/display-pane getting smaller vertically, i.e controls are being positioned where the bitmap display was, need to invalidate the controls panel so old-display doesnt remain around the controls.
		RECT Rt;
		Rt.top    = m_DspPane.cy;
		Rt.bottom = cy;
		Rt.left   = 0;
		Rt.right  = cx;
		InvalidateRect(&Rt);
	}
	prvCY = m_DspPane.cy;
	MyPaint(1, m_fgs.DoneShowWindow);
}

void CClassifyDlg::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType,cx,cy);
	// TODO: Add your message handler code here
	ResizeDisplay(cx, cy);
}

BOOL CClassifyDlg::DestroyWindow()
{
	// TODO: Add your specialized code here and/or call the base class
	SaveProgramConfig();
	UnloadCsv();
	return CDialogEx::DestroyWindow();
}

void CClassifyDlg::OnBnClickedBtnGotoPicture()
{
	if(m_Open){
		CGotoPictureDlg dlg;
		dlg.m_GotoPic = m_GotoPic;
		if(dlg.DoModal() == IDOK){
			m_GotoPic = dlg.m_GotoPic;
			FILENAMEGROUP *pfng = nullptr;
				 if(m_GotoPic.RadioIndex == 0) pfng = m_Annotation.GetAtIndex(m_GotoPic.PicIndex-1);	// -1 because annotation indexes are zero based.
			else if(m_GotoPic.RadioIndex == 1) pfng = m_Annotation.GetAtFilename(m_GotoPic.Filename);
			else if(m_GotoPic.RadioIndex == 2) pfng = m_Annotation.GetAtFirstOfType(0, 1);
			else if(m_GotoPic.RadioIndex == 3) pfng = m_Annotation.GetAtFirstOfType(1, 0);
			else if(m_GotoPic.RadioIndex == 4) pfng = m_Annotation.GetLastLatestTimestamp();

			if(pfng){
				if(pfng != m_pGrp){
					Restore_FilenameGroup();
					m_pGrp = pfng;
					LoadFilenameGroup();
				}
			}
			else MsgBox2("find picture\r\n\r\n...no match found");
		}
	}
}

void CClassifyDlg::CycleHighlightBox(int delta)
{
	if(m_Display.GetSelectedBox(m_pGrp, SELECTION_EDITING) < 0){ // check no boxes are already selected for editing.
		m_Display.HideCrossHairs();
		m_MO.Display.HideAnnotations = 0;
		int c=m_Cycle_box, HL=0;	// cycle through (highlight) each box.
		if(delta){
			// '>=' because it was possible for m_Cycle_box to get out of range if the user deleted boxes, however its not actually possible anymore because m_Cycle_box is reset to zero whenever a box is deleted.
			if(m_Cycle_box >= m_pGrp->NumClsfdLines-1) m_Cycle_box = 0;
			else m_Cycle_box++;
			HL = 1;
		}
		else{
			// m_Cycle_box cannot go negative because of the way the cycling is done, but do this safety check anyway.
			if(m_Cycle_box <= 0) m_Cycle_box = m_pGrp->NumClsfdLines-1;
			else m_Cycle_box--;
			HL = 1;
		}
		if(c!=m_Cycle_box || (HL && m_pGrp->NumClsfdLines==1))
			if(SetSelectedBox_Ex(m_Cycle_box, SELECTION_HIGHLIGHT))
				MyPaint(0, 1);
	}
}

char *CClassifyDlg::AiAssistBitmapPath()
{
	int Idx = m_Display.GetSelectedBox(m_pGrp, ANY_SELECTION_TYPE);
	if(Idx >= 0){
		// get current selected box, make temp bitmap of it, return path of temp bitmap.
		CLASSIFIEDLINE *pCL = &m_pGrp->pClsfdLines[Idx];
		float PicWd = (float)m_pGrp->pPicInfo->Width, PicHt = (float)m_pGrp->pPicInfo->Height;
		RECT cz;
		cz.left   = (int)(pCL->box.x1 * PicWd);
		cz.right  = (int)(pCL->box.x2 * PicWd);
		cz.top    = (int)(pCL->box.y1 * PicHt);
		cz.bottom = (int)(pCL->box.y2 * PicHt);
		if(!CreateDirectory("C:\\classify_annotation_tool", NULL))
			if(GetLastError() != ERROR_ALREADY_EXISTS)
				MsgBox2("error - CreateDirectory - C:\\classify_annotation_tool");
		char *pPath = "C:\\classify_annotation_tool\\ai_assist_pic.png";
		if(g_CBmpFile.SaveCroppedBitmap(m_CurrentPicture, cz, pPath, cz.right-cz.left, cz.bottom-cz.top, 0))
			return pPath;
	}
	// if its the full picture then we dont need to make a temp bitmap.
	return m_CurrentPicture;
}

int CClassifyDlg::MyWMChar(int ChrCode)
{
	int Ret = 0;
	if(m_Open){
		// if m_pGrp==nullptr this means a filter has been applied and there are zero pictures to display after the filtering.
		if(m_pGrp && m_fgs.Dragging==0){
			if(m_fgs.KeyUp){
				
				int DoneSomething = 1;
				// m_fgs.KeyUp - operations will only be actioned once even if the key is held down, i.e, one key-down/key-up combination required.
				// 
				// IN THE IF/ELSE'S BELOW DO NOT INCLUDE ANY CONDITIONS OTHER THAN THE ChrCode condition,
				// otherwise it'll mess up the logic for DoneSomething.
				// #####################################################################################

				if(ChrCode=='#')
					StepFilenameGroup(1, 0, 0, CLASSIFIED_REFUSE);
				else if(ChrCode=='/')
					StepFilenameGroup(1, 0, 0, CLASSIFIED_SPECIAL);
				else if(ChrCode>='1' && ChrCode<='9')
					ChangeClassId(nullptr, ChrCode-(int)'1');
				else if(tolower(ChrCode)=='k')
					ResetFilenameGroup();
				else if(tolower(ChrCode)=='s') // see if we have a 'highlighted box', if so change it to an 'editing box'.
					StartEditingBox(m_Display.GetSelectedBox(m_pGrp, SELECTION_HIGHLIGHT), m_MousePoint);
				else if(tolower(ChrCode)=='d')
					DeleteBbox(m_Display.GetSelectedBox(m_pGrp, SELECTION_HIGHLIGHT));
				else if(tolower(ChrCode)=='t')
					ToggleTiling();
				else if(tolower(ChrCode)=='c') // hide controls
					ToggleShowControls();
				else if(tolower(ChrCode)=='v')
					DuplicateAnnotation();
				else if(tolower(ChrCode)=='o')
					PictureSizingKey();
				else if(tolower(ChrCode)=='p')
					ShellExecute(NULL, "open", m_CurrentPicture, NULL, NULL, SW_SHOWNORMAL); 
				else if(tolower(ChrCode)=='i'){
					CAIAssistDlg AIdlg; AIdlg.SetParams(AiAssistBitmapPath());
					AIdlg.DoModal();
				}
				else if(tolower(ChrCode)=='h'){
					m_MO.Display.IncreaseLineThickness = !m_MO.Display.IncreaseLineThickness;
					MyPaint(0, 0);
				}
				else if(tolower(ChrCode)=='l'){
					m_MO.Display.ShowLabels++;
					if(m_MO.Display.ShowLabels > 3) m_MO.Display.ShowLabels = 0;
					// 3 levels for ShowLabels: 0) hide labels, 1) show labels, 2) show labels with extra info included.
					MyPaint(0, 0);
				}
				else DoneSomething = 0;

				if(DoneSomething){
					m_fgs.KeyUp = 0;
					Ret = 1;	// to stop default handling, and therefore stop the system beep.
				}
			}
		}
	}
	return Ret;
}

int CClassifyDlg::MyKeyDown(int VkCode)
{
	int Ret = 0;
	if(m_Open){
		if( VkCode==VK_ESCAPE || VkCode==VK_RETURN || VkCode==VK_TAB || VkCode==VK_SPACE ){
			// These keys must never get default handling regardless of whether we use these keys for classifying, otherwise bad stuff can happen
			// like closing the dialog in the case of VK_ESCAPE / VK_RETURN (user can still use Alt/F4).
			Ret = 1;
		}
		// if m_pGrp==nullptr this means a filter has been applied and there are zero pictures to display after the filtering.
		if(m_pGrp && m_fgs.Dragging==0){
			
			int DoneSomething = 1;
			// m_fgs.KeyUp - operations that will only be actioned once even if the key is held down, i.e, one key-down/key-up combination per operation.
			// 
			// IN THE IF/ELSE'S BELOW DO NOT INCLUDE ANY CONDITIONS OTHER THAN THE VkCode condition and the m_fgs.KeyUp  
			// condition, otherwise it'll mess up the logic for DoneSomething. (recode this, its poor coding?).
			// #########################################################################################################

			if(VkCode==VK_CONTROL && m_fgs.KeyUp){
				if(m_Display.GetSelectedBox(m_pGrp, ANY_SELECTION_TYPE) >= 0)
					m_Display.ClearBoxSelections(m_pGrp);
				else // show/hide annotations
					m_MO.Display.HideAnnotations = !m_MO.Display.HideAnnotations;
				m_Display.HideCrossHairs();
				MyPaint(0, 1);
			}
			else if(VkCode==VK_RETURN && m_fgs.KeyUp)
				StepFilenameGroup(1, 0, 0, CLASSIFIED_ACCEPT);
			else if(VkCode==VK_DOWN || VkCode==VK_UP){
				if(!m_fgs.DspTiling)
					CycleHighlightBox(VkCode==VK_DOWN);
			}
			else if(VkCode==VK_HOME || VkCode==VK_END)
				StepFilenameGroup(VkCode==VK_END, 1, 0);
			else if(VkCode==VK_LEFT || VkCode==VK_PRIOR)	// VK_PRIOR is page up
				StepFilenameGroup(0, 0, VkCode==VK_PRIOR);
			else if(VkCode==VK_RIGHT || VkCode==VK_NEXT)	// VK_NEXT is page down
				StepFilenameGroup(1, 0, VkCode==VK_NEXT);
			else DoneSomething = 0;

			if(DoneSomething){
				m_fgs.KeyUp = 0;
				Ret = 1;	// to stop default handling, and therefore stop the system beep.
			}
		}
	}
	return Ret;
}

void CClassifyDlg::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// as this is a dialog window OnKeyDown is no use here, keydown messages are sent to the individual control that currently has focus, not the dialog window itself.
	// However I can intercept WM_KEYDOWN messages in PreTranslateMessage.
	CDialogEx::OnKeyDown(nChar,nRepCnt,nFlags);
}

BOOL CClassifyDlg::PreTranslateMessage(MSG* pMsg)
{
	// TODO: Add your specialized code here and/or call the base class
	if(m_Open){
		/* if(GetDlgItem(IDC_EDIT_XXXXX)->m_hWnd == GetFocus()->m_hWnd){ */
		// "WM_CHAR and WM_KEYDOWN each give you two parts to the overall input picture, you must handle both to get the full picture. the wParam is a character code for WM_CHAR, but a VK_ code for WM_KEYDOWN"
		int Rtn = 0;
		g_didMessageBox = 0;
		if(pMsg->message == WM_KEYDOWN){
			g_blocking_WM = 1;
			Rtn = MyKeyDown((int)pMsg->wParam);
			g_blocking_WM = 0;
			//OutputDbgStr("\n WM_KEYDOWN %d %c", (int)pMsg->wParam, (char)pMsg->wParam);
		}
		if(pMsg->message == WM_CHAR){
			g_blocking_WM = 1;
			Rtn = MyWMChar((int)pMsg->wParam);
			g_blocking_WM = 0;
			//OutputDbgStr("\n WM_CHAR %d %c", (int)pMsg->wParam, (char)pMsg->wParam);
		}

		if(Rtn)
			// skip default handling so these messages are used for our purpose only.
			return 1;

		// need to set key up if message box was 'blocking' otherwise WM_KEYUP gets missed.
		if(pMsg->message==WM_KEYUP || g_didMessageBox){
			//OutputDbgStr("\n****** WM_KEYUP");
			m_fgs.KeyUp = 1;
		}
	}
	return CDialogEx::PreTranslateMessage(pMsg);
}

void CClassifyDlg::OnBnClickedBtnMoreOptions()
{
	CMoreOptionsDlg Dlg;
	Dlg.SetOptions(m_MO);
	if(Dlg.DoModal() == IDOK){
		m_MO = Dlg.GetOptions();
		MyPaint(1, 1);
	}
}

void CClassifyDlg::OnBnClickedBtnFiltersRefresh()
{
	if(m_Open){
		Restore_FilenameGroup();
		UpdateData(1);	// from dlg
		int UseFilters = m_MO.PicFilter.Apply;
		// if we pass nullptr to ApplyFilters list will be re-made with no filters.
		m_pGrp = m_Annotation.ApplyFilters(UseFilters ? &m_MO.PicFilter : nullptr);
		if(m_pGrp)
			LoadFilenameGroup();
		else{
			m_Display.ClearDisplay(m_pDC, m_DspPane.cx, m_DspPane.cy);
			UpdateInfoDisplays();
		}
	}
}

void CClassifyDlg::OnBnClickedBtnListSortnow()
{
	if(m_Open && m_pGrp){
		int sc;
		if(m_Annotation.GetFNGCount(&sc) > 1){
			Restore_FilenameGroup();
			m_pGrp = m_Annotation.ApplySort(m_MO.PicSort, m_MO.SortByAreaHeightWidth, &m_MO.PicFilter);
			LoadFilenameGroup();
		}
	}
}

void CClassifyDlg::OnClose()
{
	int close = 1;
	if(m_UnsavedChanges)
		if(!MsgBox_YN("unsaved changes, close program anyway?"))
			close = 0;
	if(close)
		CDialogEx::OnClose();
}
