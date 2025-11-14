// FileManager.cpp : implementation file
//

#include "pch.h"
#include "Classify.h"
#include "afxdialogex.h"
#include "InputConfigDlg.h"
#include "Useful.h"


// CInputConfigDlg dialog

IMPLEMENT_DYNAMIC(CInputConfigDlg, CDialogEx)


CInputConfigDlg::CInputConfigDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_INPUT_PATHS, pParent)
{
	memset(&m_IC, 0, sizeof(m_IC));
}

CInputConfigDlg::~CInputConfigDlg()
{
}

void CInputConfigDlg::SetInputPathsData(INPUT_CONFIG INP)
{
	m_IC = INP;
}

INPUT_CONFIG CInputConfigDlg::GetInputPathsData()
{
	return m_IC;
}

void CInputConfigDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);

	DDX_Radio(pDX, IDC_RADIO_CSV_BLANK, m_IC.RadioCsvFile);	// first radio in group (remember it must be marked with - WS_GROUP)

	DDX_Text(pDX, IDC_EDIT_PIC_FOLDER, m_IC.PictureFolder, num_entries(m_IC.PictureFolder));
	DDX_Text(pDX, IDC_EDIT_BLANK_PIC_FOLDER, m_IC.BlanksPictureFolder, num_entries(m_IC.BlanksPictureFolder));

	DDX_Check(pDX, IDC_CHECK_SKIP_ERROR, m_IC.SkipErrorChecks);
	DDX_Check(pDX, IDC_CHECK_MERGE_CSV_LIST, m_IC.MergeFiles);
	DDX_Check(pDX, IDC_CHECK_CSV_FOLDER_EXMODEL, m_IC.FolderExcludeModel);
	DDX_Check(pDX, IDC_CHECK_CSV_FOLDER_EXCLASSIFIED, m_IC.FolderExcludeClassified);
	DDX_Check(pDX, IDC_CHECK_AUTO_ANNO, m_IC.AnnotationProcessing);
	DDX_Check(pDX, IDC_CHECK_DEBUG_AUTO_ANNO, m_IC.DebugAnnotationProcessing);
	DDX_Check(pDX, IDC_CHECK_RGB_SEPARATION_TEST, m_IC.DoColourTestForBlanks);

	DDX_Control(pDX, IDC_COMBO_CSV_FILE, m_ComboBoxFile);
	DDX_Control(pDX, IDC_COMBO_CSV_FOLDER, m_ComboBoxFolder);

	//DDX_CBString(pDX, IDC_COMBO_CSV_SOURCE, m_CsvSourceStr);
}


BEGIN_MESSAGE_MAP(CInputConfigDlg, CDialogEx)
	ON_CBN_SELCHANGE(IDC_COMBO_CSV_FILE, &CInputConfigDlg::OnCbnSelchangeComboCsvFile)
	ON_CBN_SELCHANGE(IDC_COMBO_CSV_FOLDER, &CInputConfigDlg::OnCbnSelchangeComboCsvFolder)
	ON_BN_CLICKED(IDC_BTN_PIC_FOLDER, &CInputConfigDlg::OnBnClickedBtnPicFolder)
	ON_BN_CLICKED(IDC_BUTTON_CSV_FILE, &CInputConfigDlg::OnBnClickedButtonCsvFile)
	ON_BN_CLICKED(IDC_BUTTON_CSV_FOLDER, &CInputConfigDlg::OnBnClickedButtonCsvFolder)
	ON_BN_CLICKED(IDC_RADIO_CSV_FILE, &CInputConfigDlg::OnBnClickedRadioCsvFile)
	ON_BN_CLICKED(IDC_RADIO_CSV_FOLDER, &CInputConfigDlg::OnBnClickedRadioCsvFolder)
	ON_BN_CLICKED(IDC_RADIO_CSV_BLANK, &CInputConfigDlg::OnBnClickedRadioCsvBlank)
	ON_BN_CLICKED(IDC_BUTTON_CSV_FILE_REM, &CInputConfigDlg::OnBnClickedButtonCsvFileRem)
	ON_BN_CLICKED(IDC_BUTTON_CSV_FOLDER_REM, &CInputConfigDlg::OnBnClickedButtonCsvFolderRem)
	ON_BN_CLICKED(IDC_BTN_BLANK_PIC_FOLDER, &CInputConfigDlg::OnBnClickedBtnBlankPicFolder)
END_MESSAGE_MAP()


// CInputConfigDlg message handlers

void CInputConfigDlg::InitComboBox(CComboBox *pCB, char pPath[][MAX_PATH], int &Num, int &Index)
{
	Num = 0;
	pCB->ResetContent();
	int load=1;
	for(int i=0; i<MAX_INPATHLIST; i++){
		if(load && pPath[i][0]!=0){
			pCB->AddString(pPath[i]);
			Num++;
		}
		else{
			load = 0;// cant have gaps in the array.
			pPath[i][0] = 0;
		}
	}
	if(Index<0 || Index>=Num)	// keep existing index, provided nothing has gone wrong and its still valid.
		Index = 0;
	pCB->SetCurSel(Index);
}

BOOL CInputConfigDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	// TODO:  Add extra initialization here

	InitComboBox(&m_ComboBoxFile, m_IC.CsvFile, m_IC.NumCsvFile, m_IC.IdxCsvFile);
	InitComboBox(&m_ComboBoxFolder, m_IC.CsvFolder, m_IC.NumCsvFolder, m_IC.IdxCsvFolder);
	EnableControlsAccrordingly();

	GetDlgItem(IDC_CHECK_AUTO_ANNO)->SetWindowText("Annotation processing with object crop and export, see FixAndCropPictures.cpp.\nProcesses special annotations 'remove' and 'blurr' - sections of the picture are cropped or blurred accordingly. \
Pictures which have undergone this process are exported to folder 'cp_exports' (in base folder). Processing also does exports for augmentation and to optimise object size according to parameters specified in FixAndCropPictures.cpp");

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

void CInputConfigDlg::OnOK()
{
	// TODO: Add your specialized code here and/or call the base class
	CDialogEx::OnOK();
}

void CInputConfigDlg::OnCbnSelchangeComboCsvFile()
{
//	CString ss;
//	m_ComboBoxFile.GetLBText(m_ComboBoxFile.GetCurSel(), ss);
	m_IC.IdxCsvFile = m_ComboBoxFile.GetCurSel();
}

void CInputConfigDlg::OnCbnSelchangeComboCsvFolder()
{
//	CString ss;
//	m_ComboBoxFolder.GetLBText(m_ComboBoxFolder.GetCurSel(), ss);
	m_IC.IdxCsvFolder = m_ComboBoxFolder.GetCurSel();
}

void CInputConfigDlg::OnBnClickedBtnPicFolder()
{
	if(BrowseForFolder(m_hWnd, m_IC.PictureFolder, "Select base picture folder"))
		UpdateData(0); // to dlg
}

void CInputConfigDlg::OnBnClickedBtnBlankPicFolder()
{
	if(BrowseForFolder(m_hWnd, m_IC.BlanksPictureFolder, "Select picture folder"))
		UpdateData(0); // to dlg
}

void CInputConfigDlg::AddPathToList(CComboBox *pCB, char *pNewPath, char pPath[][MAX_PATH], int SingleEntrySize, int &Num, int &Index)
{
	if(pNewPath == nullptr){
		// remove current selection from list.
		pCB->DeleteString(Index);
		for(int i=Index; i<Num-1; i++)
			strcpy_s(pPath[i], SingleEntrySize, pPath[i+1]);
		pPath[Num-1][0] = 0;
		Num--;
		if(Num == 0)
			Index = -1;
		else if(Index >= Num)
			Index = Num - 1;
		pCB->SetCurSel(Index);
	}
	else{
		// add pNewPath to list.
		for(int i=0; i<Num; i++){
			// CHECK IF PATH IS ALREADY IN LIST, if so no need to add pNewPath to list.
			if(strcmp(pPath[i], pNewPath) == 0){
				Index = i;
				pCB->SetCurSel(Index);
				return;
			}
		}

		if(Num < MAX_INPATHLIST){
			strcpy_s(pPath[Num], SingleEntrySize, pNewPath);
			pCB->AddString(pPath[Num]);
			Num++;
		}
		else{
			pCB->DeleteString(0);
			//pCB->ResetContent();
			for(int i=0; i<Num-1; i++)	// shunt list, put new on end.
				strcpy_s(pPath[i], SingleEntrySize, pPath[i+1]);
			strcpy_s(pPath[Num-1], SingleEntrySize, pNewPath);

			//for(int i=0; i<Num; i++) pCB->AddString(pPath[i]);
			pCB->AddString(pPath[Num-1]);
		}
		Index = Num - 1;
		pCB->SetCurSel(Index);
	}
}

void CInputConfigDlg::OnBnClickedButtonCsvFile()
{
	CFileDialog FileDlg(TRUE, NULL, NULL, OFN_HIDEREADONLY, CSV_FILE_FILTERS);
	if(FileDlg.DoModal() == IDOK){
		char File[MAX_PATH];
		sprintf_s(File, num_entries(File), "%s\\%s", (const char*)FileDlg.GetFolderPath(), (const char*)FileDlg.GetFileName());
		AddPathToList(&m_ComboBoxFile, File, m_IC.CsvFile, num_entries(m_IC.CsvFile[0]), m_IC.NumCsvFile, m_IC.IdxCsvFile);
		UpdateData(0); // to dlg
	}
}

void CInputConfigDlg::OnBnClickedButtonCsvFolder()
{
	char Folder[MAX_PATH];
	if(BrowseForFolder(m_hWnd, Folder, "Classify window - Select csv folder")){
		AddPathToList(&m_ComboBoxFolder, Folder, m_IC.CsvFolder, num_entries(m_IC.CsvFolder[0]), m_IC.NumCsvFolder, m_IC.IdxCsvFolder);
		UpdateData(0); // to dlg
	}
}

void CInputConfigDlg::OnBnClickedButtonCsvFileRem()
{
	AddPathToList(&m_ComboBoxFile, nullptr, m_IC.CsvFile, num_entries(m_IC.CsvFile[0]), m_IC.NumCsvFile, m_IC.IdxCsvFile);
	UpdateData(0); // to dlg
}

void CInputConfigDlg::OnBnClickedButtonCsvFolderRem()
{
	AddPathToList(&m_ComboBoxFolder, nullptr, m_IC.CsvFolder, num_entries(m_IC.CsvFolder[0]), m_IC.NumCsvFolder, m_IC.IdxCsvFolder);
	UpdateData(0); // to dlg
}

void CInputConfigDlg::EnableControlsAccrordingly()
{
	int idx = 0;
	GetDlgItem(IDC_EDIT_BLANK_PIC_FOLDER)->EnableWindow(m_IC.RadioCsvFile == idx);
	GetDlgItem(IDC_BTN_BLANK_PIC_FOLDER)->EnableWindow(m_IC.RadioCsvFile == idx);
	GetDlgItem(IDC_CHECK_RGB_SEPARATION_TEST)->EnableWindow(m_IC.RadioCsvFile == idx);

	idx = 1;
	GetDlgItem(IDC_COMBO_CSV_FILE)->EnableWindow(m_IC.RadioCsvFile == idx);
	GetDlgItem(IDC_BUTTON_CSV_FILE)->EnableWindow(m_IC.RadioCsvFile == idx);
	GetDlgItem(IDC_BUTTON_CSV_FILE_REM)->EnableWindow(m_IC.RadioCsvFile == idx);
	
	idx = 2;
	GetDlgItem(IDC_CHECK_CSV_FOLDER_EXMODEL)->EnableWindow(m_IC.RadioCsvFile == idx);
	GetDlgItem(IDC_CHECK_CSV_FOLDER_EXCLASSIFIED)->EnableWindow(m_IC.RadioCsvFile == idx);
	GetDlgItem(IDC_COMBO_CSV_FOLDER)->EnableWindow(m_IC.RadioCsvFile == idx);
	GetDlgItem(IDC_CHECK_MERGE_CSV_LIST)->EnableWindow(m_IC.RadioCsvFile == idx);
	GetDlgItem(IDC_BUTTON_CSV_FOLDER)->EnableWindow(m_IC.RadioCsvFile == idx);
	GetDlgItem(IDC_BUTTON_CSV_FOLDER_REM)->EnableWindow(m_IC.RadioCsvFile == idx);
}

void CInputConfigDlg::OnBnClickedRadioCsvFile()
{
	UpdateData(1); // from dlg
	EnableControlsAccrordingly();
}

void CInputConfigDlg::OnBnClickedRadioCsvFolder()
{
	UpdateData(1); // from dlg
	EnableControlsAccrordingly();
}

void CInputConfigDlg::OnBnClickedRadioCsvBlank()
{
	UpdateData(1); // from dlg
	EnableControlsAccrordingly();
}

