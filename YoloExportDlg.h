
#pragma once

#include "afxdialogex.h"
#include "Annotations.h"


// CYoloExportDlg dialog

#ifdef ALLOW_ANNOTATION_FILTER
struct BBOXFILTER
{
	int MinHeight, MinWidth;
	int EnableMinHeight, EnableMinWidth;
};
#endif

struct MINMAXAVG
{
	double Min, Max, Avg;
	int Num;
	int Min_PicIdx, Max_PicIdx;
};

#define DISTRIBUTION_COUNT	10
struct CLASS_STATS
{
	int Count;
	MINMAXAVG Ht, Wd;
	int OnePerPicCount;
	int Distribution_H[DISTRIBUTION_COUNT];
	int Distribution_W[DISTRIBUTION_COUNT];
};

struct STATS_COUNT
{
	int Pictures, NegativePics, PositivePics;
	int PositiveAnnotations;
	int MultipleProbsPics;
	int NotClsfd, Accept, Refuse, Special;
	int ColourCode[COLOURCODE::COLOUR_NUM_ENTRIES];
	int PictureCode[PICTURECODE::PIC_NUM_ENTRIES];
	int ModelCode[MODELCODE::MODEL_NUM_ENTRIES]; // positive annotation count per model.
};

struct STATISTICS
{
	STATS_COUNT Cnt;
	MINMAXAVG   PicHt, PicWd;
	CLASS_STATS Class[_MAX_CLASSES_];
};

class CYoloExportDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CYoloExportDlg)

private:
	char m_CacheFolder[MAX_PATH], m_CacheBitmapExtention[8], m_PrevCF[MAX_PATH];
	int  m_EnableCacheFolder;
	char m_PathListPath[MAX_PATH];
	char *m_pPictureFolder;
	CString m_ClassList;
	int m_ShowClassIdx, m_RemoveAbsentClasses, m_Error, m_InitClassList;

	// other filters we might need
	// exclude certain classes, bbox min area, 

	FILENAMEGROUP_FILTERED *m_pFiltrd;
	int m_NumSourceFilenameGroups;
	float m_TargetWd, m_TargetHt;
	
	int  CacheSetupOk();
	int  CacheFolderOk();
	int  WriteYoloAnnotation(FILE *pFile, BBOX *pBox, int class_id);
	int  ExportPathList();
	int  ExportPictureAnnotations();
	void InitWriteCacheBitmap();
	int  WriteCacheBitmap(FILENAMEGROUP *pFnG, char *pPicPath);
#ifdef ALLOW_ANNOTATION_FILTER
	int  WantAnnotation(CLASSIFIEDLINE *pLne);
	void ApplyAnnotationFilters(int Apply);
	int  PictureHasUnfilteredAnnotations(FILENAMEGROUP *pFnG);
#endif
	void ScaleToTarget(BBOX *pBBox, float &ConvertedWd, float &ConvertedHt);
	void UpdateClassListStr();
	void ReindexClassesWithoutAbsentClasses();
	int  GotNonObjectClasses();
	void AddSizeDistributionString(char *pLabel, MINMAXAVG *pMMA, int *pDistribution, CString &Str, CString &add);
	void LoadStatistics(STATISTICS &stats, BOOL UseAnnotationFilter);
	int  HasMultiProbs(FILENAMEGROUP *pFnG, BOOL UseAnnotationFilter);

public:
	CYoloExportDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~CYoloExportDlg();
	void SetParams(char *pPictureFolder, int NumSourceFilenameGroups, FILENAMEGROUP_FILTERED *pFiltrd, float TargetWd, float TargetHt);
	void StatisticsPopup(BOOL UseAnnotationFilter);

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_DIALOG_YOLO_EXPORT };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	DECLARE_MESSAGE_MAP()

public:
	virtual BOOL OnInitDialog();
	virtual void OnOK();
	afx_msg void OnBnClickedBtnExportPathlist();
	afx_msg void OnBnClickedBtnExportAnnotations();
	afx_msg void OnBnClickedCheckYeShowIdx();
	afx_msg void OnBnClickedButtonYoloAnchors();
	afx_msg void OnBnClickedBtnCacheFolder();
	afx_msg void OnBnClickedCheckRemoveAbsentClasses();
};
