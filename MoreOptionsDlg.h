
#pragma once

#include "afxdialogex.h"

struct WH_ENABLE
{
	int Wd, Ht, Enable;
};
struct XY_ENABLE
{
	int X, Y, Enable;
};
struct MINMAX_ENABLE
{
	int Min, Max, Enable;
};
struct STR_ENABLE
{
	int Enable;
	char Str[256];
};

#define CSVENTRY_MAX_VALS	32
struct CSVENTRY
{
	BOOL Enable;
	BOOL AnyValues;	// radio control, either 'all values'(0) or 'any values'(1).
	char CsvStr[256];
	int  Values[CSVENTRY_MAX_VALS];
	int  NumValues;
};

struct REMOVALFLAGS
{
	BOOL Negatives, Positives;
	BOOL ClassifiedAccept, ClassifiedRefuse, ClassifiedSpecial, NotClassified;
	BOOL ColourUnknown, ColourYes, ColourNo;
	BOOL SingleProbability;
};

struct PICTUREFILTERS
{
	BOOL Apply;		// set in CClassifyDlg not CMoreOptionsDlg
	BOOL Inverse;	// set in CClassifyDlg not CMoreOptionsDlg
	REMOVALFLAGS Remove;

	CSVENTRY ClassIDs;
	CSVENTRY Debug_Codes;

	WH_ENABLE MinDims;
	XY_ENABLE Ratio;
	STR_ENABLE PartialFileName;
	MINMAX_ENABLE BBoxCount;// also have one for bbox size so we can see 
};

enum PICSORT
{
	// todo properly, somehow.
	// these are just ordered in the same order as the radio controls TAB order, if the TAB order changed these wouldnt correlate, need to be linked to the radio control ID ?
	PICSORT_RANDOMISE=0,
	PICSORT_BBOX_COUNT,
	PICSORT_TIMESTAMP,
	PICSORT_PICTURE_AREA,
	PICSORT_BBOX_PROPORTION_SML,
	PICSORT_BBOX_PROPORTION_LRG,
	PICSORT_BBOX_PIXEL_SML,
	PICSORT_BBOX_PIXEL_LRG,
};

struct GENERALSETTINGS
{
	int TargetWidth, TargetHeight;
};

struct DISPLAYOPTIONS
{
	WH_ENABLE FixedDims;
	BOOL HideAnnotations, IncreaseLineThickness;
	int  ShowLabels;
	int  PictureSizing; // see CClassifyDlg::IteratePictureSizingScheme
	int  HighlightHeight; // highlight annotations <= to this height.
};

struct MOREOPTIONS	// probably rename this!
{
	// we might want a program config file to keep with the training data so we know how it was generated and can reproduce it.
	// therefore all important settings to go in here.

	int PicSort;	// PICSORT
	int SortByAreaHeightWidth;
	GENERALSETTINGS General;
	DISPLAYOPTIONS  Display;
	PICTUREFILTERS  PicFilter;
};


// CMoreOptionsDlg dialog

class CMoreOptionsDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CMoreOptionsDlg)

private:
	MOREOPTIONS m_MO;
	int CsvToClassIdList(int numeric_values, CSVENTRY *pCsvEnt);

public:
	CMoreOptionsDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CMoreOptionsDlg();

	void SetOptions(MOREOPTIONS MO);
	MOREOPTIONS GetOptions();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_MORE_OPTIONS };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedOk();
};
