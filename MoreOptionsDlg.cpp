// MoreOptions.cpp : implementation file
//

#include "pch.h"
#include "Classify.h"
#include "afxdialogex.h"
#include "MoreOptionsDlg.h"
#include "Useful.h"
#include "CsvFile.h"


// CMoreOptionsDlg dialog

IMPLEMENT_DYNAMIC(CMoreOptionsDlg, CDialogEx)

CMoreOptionsDlg::CMoreOptionsDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_MORE_OPTIONS, pParent)
{
	memset(&m_MO, 0, sizeof(m_MO));
}

CMoreOptionsDlg::~CMoreOptionsDlg()
{
}

void CMoreOptionsDlg::SetOptions(MOREOPTIONS MO)
{
	m_MO = MO;
}

MOREOPTIONS CMoreOptionsDlg::GetOptions()
{
	return m_MO;
}

void CMoreOptionsDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);

	// display
	DDX_Check(pDX, IDC_CHECK_FIXED_DIMENSIONS, m_MO.Display.FixedDims.Enable);
	DDX_Text(pDX, IDC_EDIT_FIXED_WIDTH, m_MO.Display.FixedDims.Wd);
	DDX_Text(pDX, IDC_EDIT_FIXED_HEIGHT, m_MO.Display.FixedDims.Ht);
	DDX_Check(pDX, IDC_CHECK_ONE_PIXEL_THICK, m_MO.Display.IncreaseLineThickness);
	DDX_Text(pDX, IDC_EDIT_HILIGHT_HEIGHT, m_MO.Display.HighlightHeight);

	// general
	DDX_Text(pDX, IDC_EDIT_TARGET_WIDTH, m_MO.General.TargetWidth);
	DDX_Text(pDX, IDC_EDIT_TARGET_HEIGHT, m_MO.General.TargetHeight);

	// picture filters
	DDX_Check(pDX, IDC_CHECK_PIC_COLOUR_UNKNOWN, m_MO.PicFilter.Remove.ColourUnknown);
	DDX_Check(pDX, IDC_CHECK_PIC_COLOUR_YES, m_MO.PicFilter.Remove.ColourYes);
	DDX_Check(pDX, IDC_CHECK_PIC_COLOUR_NO, m_MO.PicFilter.Remove.ColourNo);

	DDX_Check(pDX, IDC_CHECK_PIC_NEGATIVES, m_MO.PicFilter.Remove.Negatives);
	DDX_Check(pDX, IDC_CHECK_PIC_POSITIVES, m_MO.PicFilter.Remove.Positives);
	DDX_Check(pDX, IDC_CHECK_PIC_CLSFD_ACCEPT, m_MO.PicFilter.Remove.ClassifiedAccept);
	DDX_Check(pDX, IDC_CHECK_PIC_CLSFD_REFUSE, m_MO.PicFilter.Remove.ClassifiedRefuse);
	DDX_Check(pDX, IDC_CHECK_PIC_CLSFD_SPECIAL, m_MO.PicFilter.Remove.ClassifiedSpecial);
	DDX_Check(pDX, IDC_CHECK_PIC_CLSFD_NOT, m_MO.PicFilter.Remove.NotClassified);
	DDX_Check(pDX, IDC_CHECK_MULTI_PROB, m_MO.PicFilter.Remove.SingleProbability);

	DDX_Check(pDX, IDC_CHECK_CLASS_ID_CSV, m_MO.PicFilter.ClassIDs.Enable);
	DDX_Text(pDX,  IDC_EDIT_CLASS_ID_CSV, m_MO.PicFilter.ClassIDs.CsvStr, num_entries(m_MO.PicFilter.ClassIDs.CsvStr));
	DDX_Radio(pDX, IDC_RADIO_CLASS_ID_CSV_ALL, m_MO.PicFilter.ClassIDs.AnyValues);	// first radio in group (remember it must be marked with - WS_GROUP)

	DDX_Check(pDX, IDC_CHECK_PIC_DEBUGCODE, m_MO.PicFilter.Debug_Codes.Enable);
	DDX_Text(pDX, IDC_EDIT_PIC_DEBUGCODE, m_MO.PicFilter.Debug_Codes.CsvStr, num_entries(m_MO.PicFilter.Debug_Codes.CsvStr));

	DDX_Check(pDX, IDC_CHECK_PARTIAL_FILENAME, m_MO.PicFilter.PartialFileName.Enable);
	DDX_Text(pDX, IDC_EDIT_PARTIAL_FILENAME, m_MO.PicFilter.PartialFileName.Str, num_entries(m_MO.PicFilter.PartialFileName.Str));

	DDX_Check(pDX, IDC_CHECK_PIC_MIN_DIMS, m_MO.PicFilter.MinDims.Enable);
	DDX_Text(pDX, IDC_EDIT_PIC_MIN_WIDTH, m_MO.PicFilter.MinDims.Wd);
	DDX_Text(pDX, IDC_EDIT_PIC_MIN_HEIGHT, m_MO.PicFilter.MinDims.Ht);

	DDX_Check(pDX, IDC_CHECK_MIN_MAX_RATIO, m_MO.PicFilter.Ratio.Enable);
	DDX_Text(pDX, IDC_EDIT_PIC_X_RATIO, m_MO.PicFilter.Ratio.X);
	DDX_Text(pDX, IDC_EDIT_PIC_Y_RATIO, m_MO.PicFilter.Ratio.Y);

	DDX_Check(pDX, IDC_CHECK_PIC_MINMAX_BBOX, m_MO.PicFilter.BBoxCount.Enable);
	DDX_Text(pDX, IDC_EDIT_PIC_MIN_BBOX, m_MO.PicFilter.BBoxCount.Min);
	DDX_Text(pDX, IDC_EDIT_PIC_MAX_BBOX, m_MO.PicFilter.BBoxCount.Max);

	// sort pictures	
	DDX_Radio(pDX, IDC_RADIO_PICSORT_RANDOMISE, m_MO.PicSort); // first radio in group (remember it must be marked with - WS_GROUP)

	DDX_Radio(pDX, IDC_RADIO_SORT_BOX_AREA, m_MO.SortByAreaHeightWidth); // first radio in group (remember it must be marked with - WS_GROUP)
}

BEGIN_MESSAGE_MAP(CMoreOptionsDlg, CDialogEx)
	ON_BN_CLICKED(IDOK, &CMoreOptionsDlg::OnBnClickedOk)
END_MESSAGE_MAP()


// CMoreOptionsDlg message handlers

int CMoreOptionsDlg::CsvToClassIdList(int numeric_values, CSVENTRY *pCsvEnt)
{
	pCsvEnt->NumValues = 0;
	memset(pCsvEnt->Values, 0, num_entries(pCsvEnt->Values));

	if(pCsvEnt->Enable){
		CCsvFile CSV;
		if(numeric_values){
			// comma separated values are numbers, for example; 4,10,2,1
			// translate into class id integer list
			int NumVals = CSV.BufToIntegerList(pCsvEnt->CsvStr, pCsvEnt->Values, num_entries(pCsvEnt->Values));
			if(NumVals == -1)
				return 0; // error
			pCsvEnt->NumValues = NumVals;
		}
		else{
			// comma separated values are char strings, for example; car,bus,truck,motorcycle
			// translate into class id integer list
			char *ppFields[CSVENTRY_MAX_VALS];
			int   NumFields = CSV.GetCommaSeparatedFields(pCsvEnt->CsvStr, ppFields, num_entries(ppFields));
			if(NumFields > 0){
				int MaxValues = num_entries(pCsvEnt->Values);
				for(int i=0; i<NumFields; i++){
					if(i < MaxValues){
						if(!g_CIdM.GetClassId(ppFields[i], &pCsvEnt->Values[i]))
							return 0; // error
					}
					else{
						MsgBox2("error - CsvToClassIdList\r\n integer list overflow (%d)", MaxValues);
						return 0; // error
					}
				}
				pCsvEnt->NumValues = NumFields;
			}
		}
	}
	return 1; // no error
}

void CMoreOptionsDlg::OnBnClickedOk()
{
	UpdateData(1); // from dlg

	int Ok = 1;
	Ok &= CsvToClassIdList(0, &m_MO.PicFilter.ClassIDs);
	Ok &= CsvToClassIdList(1, &m_MO.PicFilter.Debug_Codes);
	if(m_MO.Display.FixedDims.Enable){
		if(m_MO.Display.FixedDims.Ht<=0 || m_MO.Display.FixedDims.Wd<=0){
			MsgBox2("error - Fixed picture dimensions must be greater than zero");
			Ok = 0;
		}
	}

	if(!Ok){
		MsgBox2("error(s) on dialog, changes will not be saved, try again");
		CDialogEx::OnCancel();
		return;
	}

	// TODO: Add your control notification handler code here
	CDialogEx::OnOK();
}
