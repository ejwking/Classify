// YoloExport.cpp : implementation file
//

#include "pch.h"
#include "Classify.h"
#include "afxdialogex.h"
#include "YoloExportDlg.h"
#include "Useful.h"
#include "YoloAnchors.h"

// CYoloExportDlg dialog

IMPLEMENT_DYNAMIC(CYoloExportDlg, CDialogEx)


CYoloExportDlg::CYoloExportDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_YOLO_EXPORT, pParent)
{
	m_NumSourceFilenameGroups = 0;
	m_CacheFolder[0] = 0;
	strcpy_s(m_CacheBitmapExtention, num_entries(m_CacheBitmapExtention), "png");
	m_pFiltrd = nullptr;
	m_ShowClassIdx = 1;
	m_RemoveAbsentClasses = 0;
	m_Error = 0;
	m_InitClassList = 0;
}

CYoloExportDlg::~CYoloExportDlg()
{
}

void CYoloExportDlg::SetParams(char *pPictureFolder, int NumSourceFilenameGroups, FILENAMEGROUP_FILTERED *pFiltrd, float TargetWd, float TargetHt)
{
	// should probably get rid of this public function and pass the params to the constructor.
	m_pPictureFolder = pPictureFolder;
	sprintf_s(m_PathListPath, num_entries(m_PathListPath), "%s\\path_list.txt", pPictureFolder);
	m_pFiltrd  = pFiltrd;
	m_TargetWd = TargetWd;
	m_TargetHt = TargetHt;
	m_NumSourceFilenameGroups = NumSourceFilenameGroups;
}

void CYoloExportDlg::DoDataExchange(CDataExchange* pDX)
{
	if(!m_InitClassList){
		UpdateClassListStr();
		m_InitClassList = 1;
	}

	CDialogEx::DoDataExchange(pDX);

	DDX_Text(pDX, IDC_EDIT_CACHE_FOLDER, m_CacheFolder, num_entries(m_CacheFolder));
	DDX_Check(pDX, IDC_CHECK_PIC_CACHE, m_EnableCacheFolder);

	DDX_Check(pDX, IDC_CHECK_YE_SHOW_IDX, m_ShowClassIdx);
	DDX_Text(pDX, IDC_EDIT_YE_CLASS_LIST, m_ClassList);
	DDX_Check(pDX, IDC_CHECK_REMOVE_ASBENT_IDS, m_RemoveAbsentClasses);
}

BEGIN_MESSAGE_MAP(CYoloExportDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_EXPORT_PATHLIST, &CYoloExportDlg::OnBnClickedBtnExportPathlist)
	ON_BN_CLICKED(IDC_BTN_EXPORT_ANNOTATIONS, &CYoloExportDlg::OnBnClickedBtnExportAnnotations)
	ON_BN_CLICKED(IDC_CHECK_YE_SHOW_IDX, &CYoloExportDlg::OnBnClickedCheckYeShowIdx)
	ON_BN_CLICKED(IDC_BUTTON_YOLO_ANCHORS, &CYoloExportDlg::OnBnClickedButtonYoloAnchors)
	ON_BN_CLICKED(IDC_BTN_CACHE_FOLDER, &CYoloExportDlg::OnBnClickedBtnCacheFolder)
	ON_BN_CLICKED(IDC_CHECK_REMOVE_ASBENT_IDS, &CYoloExportDlg::OnBnClickedCheckRemoveAbsentClasses)
END_MESSAGE_MAP()

// CYoloExportDlg message handlers

BOOL CYoloExportDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	// TODO:  Add extra initialization here
	strcpy_s(m_CacheFolder, num_entries(m_CacheFolder), AfxGetApp()->GetProfileString("YoloExport", "m_CacheFolder", ""));
	if(strlen(m_PathListPath) > 0){
		CString msg;
		msg.Format("picture folder: %s\r\npicture path list file: %s\r\nTargetWd: %d\r\nTargetHt: %d", m_pPictureFolder, m_PathListPath, (int)m_TargetWd, (int)m_TargetHt);
		GetDlgItem(IDC_EDIT_YE_INFO)->SetWindowText(msg);
	}
	GetDlgItem(IDC_CHECK_PIC_CACHE)->SetWindowText("Use picture cache.\r\nAs well as writting the annotation files to the cache folder, when export annotations button below is pressed, all bitmaps will be copied to the cache folder at target dimensions.");

	if(m_Error){
		GetDlgItem(IDC_BTN_EXPORT_PATHLIST)->EnableWindow(0);
		GetDlgItem(IDC_BTN_EXPORT_ANNOTATIONS)->EnableWindow(0);
		GetDlgItem(IDC_CHECK_YE_SHOW_IDX)->EnableWindow(0);
		GetDlgItem(IDC_BUTTON_YOLO_ANCHORS)->EnableWindow(0);
		GetDlgItem(IDC_BTN_CACHE_FOLDER)->EnableWindow(0);
		GetDlgItem(IDC_CHECK_REMOVE_ASBENT_IDS)->EnableWindow(0);
		GetDlgItem(IDC_CHECK_PIC_CACHE)->EnableWindow(0);
	}
	else{
#ifdef ALLOW_ANNOTATION_FILTER
		// zero(reset) filters when dialog is opened.
		ApplyAnnotationFilters(0);
#endif
	}
	return TRUE;  // return TRUE unless you set the focus to a control
}

void CYoloExportDlg::OnOK()
{
	// TODO: Add your specialized code here and/or call the base class
	AfxGetApp()->WriteProfileString("YoloExport", "m_CacheFolder", m_CacheFolder);
	CDialogEx::OnOK();
}

int CYoloExportDlg::GotNonObjectClasses()
{
	if(m_pFiltrd){
		FILENAMEGROUP *pFnG;
		for(int g=0; g<m_pFiltrd->Num; g++){
			pFnG = m_pFiltrd->ppFNG[g];
			for(int x=0; x<pFnG->NumClsfdLines; x++)
				if(pFnG->pClsfdLines[x].ClassId >= g_CIdM.m_NumDetectionClasses)
					return 1;
		}
	}
	else MsgBox2("error - m_pFiltrd==null");
	return 0;
}

void CYoloExportDlg::ReindexClassesWithoutAbsentClasses()
{
	if(m_RemoveAbsentClasses){
		// this is optional, with UI tickbox, because dataset might not include certain classes, that doesnt necessarily mean they are unwanted, just that they are currently placeholders in the YOLO network for when training data is ready.
		if(m_pFiltrd){
			FILENAMEGROUP *pFnG;
			int cid, ConvertedId = 0;
			for(cid=0; cid<g_CIdM.m_NumDetectionClasses; cid++){
				/* if(g_CIdM.Id_inCategory(cid, CATEGORY_DETECTIONS)) */
				int GotId = 0;
				for(int g=0; g<m_pFiltrd->Num && !GotId; g++){
					pFnG = m_pFiltrd->ppFNG[g];
					for(int x=0; x<pFnG->NumClsfdLines; x++)
						if(pFnG->pClsfdLines[x].ClassId == cid)
							GotId = 1;
				}
				g_CIdM.m_Labels[cid].ConvertedId = GotId ? ConvertedId++ : -1;
				g_CIdM.m_Labels[cid].Present = GotId;
			}
		}
		else MsgBox2("error - m_pFiltrd==null");
	}
	else{
		for(int i=0; i<g_CIdM.m_NumDetectionClasses; i++){
			g_CIdM.m_Labels[i].ConvertedId = i;
			g_CIdM.m_Labels[i].Present = 1;
		}
	}
}

void CYoloExportDlg::UpdateClassListStr()
{
	if(GotNonObjectClasses()){
		MsgBox2("error\r\n\r\nThis csv contains non-object classes, classes used to define regions of the picture to crop and blur.\r\nThis csv must first be processed by FixAndCropPictures.cpp");
		m_ClassList = "error";
		m_Error = 1;
	}
	if(!m_Error)
	{
		ReindexClassesWithoutAbsentClasses();
		CString addstr;
		m_ClassList.Empty();
		for(int i=0; i<g_CIdM.m_NumDetectionClasses; i++){
			if(m_ShowClassIdx)
				addstr.Format("[%d]%s%s\r\n", g_CIdM.m_Labels[i].ConvertedId, g_CIdM.GetLabel(i), g_CIdM.m_Labels[i].Present?"":" - absent and excluded class");
			else
				addstr.Format("%s%s\r\n", g_CIdM.GetLabel(i), g_CIdM.m_Labels[i].Present?"":" - absent and excluded class");
			m_ClassList += addstr;
		}
	}
}

void CYoloExportDlg::OnBnClickedCheckRemoveAbsentClasses()
{
	OnBnClickedCheckYeShowIdx();
}

void CYoloExportDlg::OnBnClickedCheckYeShowIdx()
{
	UpdateData(1); // from dlg
	UpdateClassListStr();
	UpdateData(0); // to dlg
}

int CYoloExportDlg::CacheSetupOk()
{
	UpdateData(1); // from dlg, to update m_EnableCacheFolder
	if(m_EnableCacheFolder){
		if(!FolderExists(m_CacheFolder)){
			MsgBox2("error - invalid cache folder");
			return 0;
		}
		if(!CacheFolderOk())
			return 0;
		MsgBox2("WARNING - using cache folder\r\n ..be sure to use a separate folder for the cache bitmaps, do not mix them with the original bitmaps.");
	}
	return 1;
}

static char *g_pReminders = "reminders: \n\nis class id list correct\nfilter classified refuse/special entries\nrandomise list if you're using different bitmap sources";
void CYoloExportDlg::OnBnClickedBtnExportPathlist()
{
	if(CacheSetupOk()){
		if(MsgBox_YN("Export path list, continue.. ?\n\n%s", g_pReminders)){
			if(m_pFiltrd && m_pFiltrd->Num>0 && m_pPictureFolder[0]!=0)
				ExportPathList();
			else MsgBox2("Export path list - failed");
		}
	}
}

void CYoloExportDlg::OnBnClickedBtnExportAnnotations()
{
	if(CacheSetupOk()){
		if(MsgBox_YN("Export yolo annotation files, continue.. ?\n\n%s", g_pReminders)){
			if(m_pFiltrd && m_pFiltrd->Num>0 && m_pPictureFolder[0]!=0)
				ExportPictureAnnotations();
			else MsgBox2("Export annotations - failed");
		}
	}
}

void CYoloExportDlg::OnBnClickedBtnCacheFolder()
{
	if(BrowseForFolder(m_hWnd, m_CacheFolder, "Select cache folder")){
		if(!CacheFolderOk()){
			m_CacheFolder[0] = 0;
			return;
		}
		UpdateData(0); // to dlg
	}
}

int CYoloExportDlg::CacheFolderOk()
{
	// Keep the cache folder separate from the original pictures folder.
	// yes but actually its not this simple, because the paths in the csv include sub folders, so actually it would be ok for cache folder to go inside m_pPictureFolder.
/*	char cf[MAX_PATH];
	strcpy_s(cf, num_entries(cf), m_CacheFolder);
	size_t len_c = strlen(cf);
	size_t len_p = strlen(m_pPictureFolder);
	if(len_c >= len_p){
		cf[len_p] = 0; // null terminate so m_CacheFolder length matches m_pPictureFolder.
		if(strcmp(cf, m_pPictureFolder) == 0){
			MsgBox2("error - cache folder must not be the same as, or inside the original picture folder");
			return 0;
		}
	}*/
	return 1;
}

#ifdef ALLOW_ANNOTATION_FILTER
I removed this code as I dont use it, dont want it, and really I think its not a good idea to have un-annotated objects in a picture no matter what their size.

SetWindowText("Annotation filters are optional and temporarily applied to this YOLO export. They are applied (or removed) when the 'Refresh list' button is pressed. \
After refreshing the list press 'Show statistics' to see export statistics with the current filter criteria. (note, the filters exclude annotations, if no annotations are left the picture is excluded)");
int CYoloExportDlg::WantAnnotation(CLASSIFIEDLINE *pLne)
{
	if(pLne->ClassId >= 0){
		// Not negatives, a negative is not an annotation as such, its a picture status.
		// Negatives are filtered using picture filters, not annotation filters.
		if(m_Bbox.EnableMinHeight || m_Bbox.EnableMinWidth){
			float ConvertedWd, ConvertedHt;
			ScaleToTarget(&pLne->box, ConvertedWd, ConvertedHt);

			if(m_Bbox.EnableMinHeight)
				if(ConvertedHt < (float)m_Bbox.MinHeight)
					return 0;
			if(m_Bbox.EnableMinWidth)
				if(ConvertedWd < (float)m_Bbox.MinWidth)
					return 0;
		}
	}
	return 1; // want annotation
}

int CYoloExportDlg::PictureHasUnfilteredAnnotations(FILENAMEGROUP *pFnG)
{
	int AnnotationsCount = 0;
	CLASSIFIEDLINE *pCL = pFnG->pClsfdLines;
	for(int s=0; s<pFnG->NumClsfdLines; s++, pCL++)
		if(pCL->ClassId<0 || pCL->AnnotationFilter==0)	// negatives or unfiltered.
			AnnotationsCount++;
	return (AnnotationsCount > 0);	// true if picture still has annotations left after filtering.
}

void CYoloExportDlg::ApplyAnnotationFilters(int Apply)
{
	if(m_pFiltrd){
		for(int i=0; i<m_pFiltrd->Num; i++){
			FILENAMEGROUP  *pFnG = m_pFiltrd->ppFNG[i];
			CLASSIFIEDLINE *pLne = pFnG->pClsfdLines;
			for(int x=0; x<pFnG->NumClsfdLines; x++, pLne++){
				if(Apply && WantAnnotation(pLne)==0)
					pLne->AnnotationFilter = 1;	// annotation will be filtered, in other words, removed.
				else
					pLne->AnnotationFilter = 0; // normal state, not filtered.
			}
		}
	}
	else MsgBox2("error - ApplyAnnotationFilters, m_pFiltrd == null");
}

void CYoloExportDlg::OnBnClickedButtonRefreshList()
{
	UpdateData(1); // from dlg
	// below, must call this whether filters are enabled or not, because when filters are not enabled it 
	// is resetting the list (i.e. user could have turned filters on and then off).
	ApplyAnnotationFilters(1);
	MsgBox("list refreshed");
}

void CYoloExportDlg::OnBnClickedButtonYoloStats()
{
	StatisticsPopup(1);
}
#endif

int CYoloExportDlg::ExportPathList()
{
	int   Ok = 1;
	FILE *pFile;
	// todo - add 'save as' dlg so user can chose where to put path_list file, for now it'll go in pPictureFolder.
	fopen_s(&pFile, m_PathListPath, "wt");
	if(!pFile){
		MsgBox2("error - fopen failed \r\n%s", m_PathListPath);
		Ok = 0;
	}
	else{
		char Str[MAX_PATH];
		for(int i=0; i<m_pFiltrd->Num && Ok; i++){
			FILENAMEGROUP *pFnG = m_pFiltrd->ppFNG[i];
#ifdef ALLOW_ANNOTATION_FILTER
			if(PictureHasUnfilteredAnnotations(pFnG))
#endif
			{
				// make picture path
				if(m_EnableCacheFolder){
					FolderAndFilenameToPath(Str, num_entries(Str), m_CacheFolder, pFnG->pPicInfo->pfile_name);
					ChangeFileExtension(Str, m_CacheBitmapExtention);
				}
				else
					FolderAndFilenameToPath(Str, num_entries(Str), m_pPictureFolder, pFnG->pPicInfo->pfile_name);
				// add path to list
				int Size = sprintf_s(Str, num_entries(Str), "%s\n", Str);
				Ok &= (fwrite(Str, sizeof(char), strlen(Str), pFile) == Size);
			}
		}
		fclose(pFile);
	}
	MsgBox2("ExportPathList - Ok - %d\r\n\r\n exported to - %s\r\n number paths in the path list - %d\r\n", Ok, m_PathListPath, m_pFiltrd->Num);
	return Ok;
}

void CYoloExportDlg::InitWriteCacheBitmap()
{
	m_PrevCF[0] = 0;
}

/* darknet DISCORD STEPHANE
if you always save them as PNG, then none of your images will have JPG artifacts. And if you run .jpg files through, you can get lower scores.  
Same way if you always train with JPG, and then pass in some PNG files.

This is something I discovered a while ago.  All my training images were PNG.  Then when i went to deploy to the field, we were using JPG files.  
Even though the images were high quality, the results were extremely poor.  Investigation showed that the compression artifacts from JPG files were 
skewing the results.  Since then, DarkMark randomly mixes up .png and .jpg, and also randomly chooses the jpeg compression values.*/

// PNG only for now, remember originals will be of all types, but mainly jpg/compressed. So even if saving as png here theres still gonna be artefacts.

int CYoloExportDlg::WriteCacheBitmap(FILENAMEGROUP *pFnG, char *pPicPath)
{
	char tempA[MAX_PATH], temp_B[MAX_PATH];
	FullPathToFolderAndFilename(pPicPath, tempA, MAX_PATH, temp_B, MAX_PATH);
	int Ok = 0;
	if(strcmp(m_PrevCF, tempA) == 0)
		Ok = 1; // (nested) cache folder structure is the same as the last picture and has already been created.
    else if(CreateNestedFolders(tempA, m_CacheFolder)){
		Ok = 1;
		strcpy_s(m_PrevCF, num_entries(m_PrevCF), tempA);
	}

	if(Ok){
		sprintf_s(tempA, num_entries(tempA), "%s%s", tempA, temp_B);
		if(ChangeFileExtension(tempA, m_CacheBitmapExtention)){
			FolderAndFilenameToPath(temp_B, num_entries(temp_B), m_pPictureFolder, pFnG->pPicInfo->pfile_name);
			RECT Rt={0};
			Rt.right  = pFnG->pPicInfo->Width;
			Rt.bottom = pFnG->pPicInfo->Height;
			if(g_CBmpFile.SaveCroppedBitmap(temp_B, Rt, tempA, (int)m_TargetWd, (int)m_TargetHt, 0))
				return 1;
		}
	}
	return 0;
}

int CYoloExportDlg::WriteYoloAnnotation(FILE *pFile, BBOX *pBox, int class_id)
{
	if(BboxError(pBox) == 0){
		float center_x, center_y, width, height;
		char  Str[64];
		center_x = (pBox->x1 + pBox->x2) / 2.0f;
		center_y = (pBox->y1 + pBox->y2) / 2.0f;
		width    =  pBox->x2 - pBox->x1;
		height   =  pBox->y2 - pBox->y1;
		int Size = sprintf_s(Str, num_entries(Str), "%d %f %f %f %f\n", class_id, center_x, center_y, width, height);
		return (fwrite(Str, sizeof(char), strlen(Str), pFile) == Size);
	}
	else MsgBox2("WriteYoloAnnotation - BboxError \r\n %f, %f, %f, %f", pBox->x1, pBox->y1, pBox->x2, pBox->y2);
	return 0;
}

int CYoloExportDlg::ExportPictureAnnotations()
{
	int  i, s, Ok = 1, NumYoloFiles = 0, NumNegatives = 0;
	char PicPath[MAX_PATH];
	InitWriteCacheBitmap();
	for(i=0; i<m_pFiltrd->Num && Ok; i++){
		FILENAMEGROUP *pFnG = m_pFiltrd->ppFNG[i];
#ifdef ALLOW_ANNOTATION_FILTER
		if(PictureHasUnfilteredAnnotations(pFnG))
#endif
		{
			Ok = 1;
			FolderAndFilenameToPath(PicPath, num_entries(PicPath), m_EnableCacheFolder ? m_CacheFolder : m_pPictureFolder, pFnG->pPicInfo->pfile_name);
			if(m_EnableCacheFolder)
				Ok &= WriteCacheBitmap(pFnG, PicPath);
			Ok &= ChangeFileExtension(PicPath, "txt");
			if(Ok){
				// write the yolo annotations file for this picture.
				FILE *pFile;
				fopen_s(&pFile, PicPath, "wt");
				if(!pFile){
					MsgBox2("error - fopen failed \r\n%s", PicPath);
					Ok = 0;
				}
				else{
					// as we will be in 'viewer mode' we could use either SOURCELINE or CLASSIFIEDLINE, they will contain the same data.
					if(pFnG->pClsfdLines->ClassId < 0){
						// negative picture - file will be empty as there is no annotations.
						NumNegatives++;
					}
					else{
						// picture with annotations.
						CLASSIFIEDLINE *pCL = pFnG->pClsfdLines;
						for(s=0; s<pFnG->NumClsfdLines && Ok; s++, pCL++){
#ifdef ALLOW_ANNOTATION_FILTER
							if(pCL->AnnotationFilter == 0)
#endif
							{
								if(g_CIdM.m_Labels[pCL->ClassId].Present)
									Ok &= WriteYoloAnnotation(pFile, &pCL->box, g_CIdM.m_Labels[pCL->ClassId].ConvertedId); // g_CIdM.IdToConvertedId(pCL->ClassId));
								else{
									Ok = 0; // if this happened, then it would be a coding error, not a user/data error. We should not be trying to export a class that is not 'Present'.
									MsgBox2("code error - trying to export an excluded class");
								}
							}
						}
					}
					NumYoloFiles++;
					fclose(pFile);
				}
			}
		}
		// UI progress display
		if((i%10)==0 || i==m_pFiltrd->Num-1)
			ProgressUpdate_SetWindowText(this, "export pic annotations", i, m_pFiltrd->Num);
	}
	MsgBox2("ExportPictureAnnotations - Ok - %d\r\n\r\n Number of annotation files - %d (negatives %d)", Ok, NumYoloFiles, NumNegatives);
	return Ok;
}

void CYoloExportDlg::ScaleToTarget(BBOX *pBBox, float &ConvertedWd, float &ConvertedHt)
{
	// BboxToPixelWdHt_Inc_Exc(
	ConvertedWd = (pBBox->x2 - pBBox->x1) * (float)m_TargetWd;
	ConvertedHt = (pBBox->y2 - pBBox->y1) * (float)m_TargetHt;
}

static void MinMaxAvg(MINMAXAVG &a, double b, int PicIdx)
{
	if(a.Min == 0)
		a.Min = 100000;
	if(b < a.Min){
		a.Min = b;
		a.Min_PicIdx = PicIdx;
	}
	if(b > a.Max){
		a.Max = b;
		a.Max_PicIdx = PicIdx;
	}
	a.Avg += b;
	a.Num++;
}

static char *GetMMAString(MINMAXAVG *pMMA)
{
	static char Str[128];
	double Av = pMMA->Avg / (double)pMMA->Num;
	int NotZeroBased = 1;	// index displayed in UI is one-based, hence added one to the indexes.
	sprintf_s(Str, num_entries(Str), " - Min %d[%d], Max %d[%d], Avg %d", (int)pMMA->Min, pMMA->Min_PicIdx+NotZeroBased, (int)pMMA->Max, pMMA->Max_PicIdx+NotZeroBased, (int)Av);
	return Str;
}

static int BBoxIdentical(BBOX *pA, BBOX *pB)
{
	float tol = 0.001f; // identical to 3 decimal points.
	return ( fabs(pA->x1-pB->x1)<tol && fabs(pA->x2-pB->x2)<tol &&
			 fabs(pA->y1-pB->y1)<tol && fabs(pA->y2-pB->y2)<tol );
}

int CYoloExportDlg::HasMultiProbs(FILENAMEGROUP *pFnG, BOOL UseAnnotationFilter)
{
	UseAnnotationFilter = UseAnnotationFilter; // stop unreferrenced param warning

//	int NumObjectsWithMultipleProbs = 0; 
//  TODO - this is more complicated, need to use function MakeAllProbabilitiesGroupings.

	CLASSIFIEDLINE *pLne = pFnG->pClsfdLines;
	for(int x=0; x<pFnG->NumClsfdLines-1; x++, pLne++){
#ifdef ALLOW_ANNOTATION_FILTER
		if(!UseAnnotationFilter || pLne->AnnotationFilter==0)
#endif
		{
			int B = x+1;
			CLASSIFIEDLINE *pLne_B = &pFnG->pClsfdLines[B];
			for(; B<pFnG->NumClsfdLines; B++, pLne_B++)
#ifdef ALLOW_ANNOTATION_FILTER
				if(!UseAnnotationFilter || pLne_B->AnnotationFilter==0)
#endif
					if(BBoxIdentical(&pLne->box, &pLne_B->box))
						return 1;
		}
	}
	return 0;
}

void CYoloExportDlg::LoadStatistics(STATISTICS &stats, BOOL UseAnnotationFilter)
{
	for(int i=0; i<m_pFiltrd->Num; i++){
		FILENAMEGROUP *pFnG = m_pFiltrd->ppFNG[i];
		MinMaxAvg(stats.PicWd, (float)pFnG->pPicInfo->Width, i);
		MinMaxAvg(stats.PicHt, (float)pFnG->pPicInfo->Height, i);
		
		stats.Cnt.MultipleProbsPics += HasMultiProbs(pFnG, UseAnnotationFilter);

		int PicHasAnnotations = 0;
		BOOL ClassInPic[_MAX_CLASSES_]={0};
		CLASSIFIEDLINE *pLne = pFnG->pClsfdLines;
		for(int x=0; x<pFnG->NumClsfdLines; x++, pLne++){
#ifdef ALLOW_ANNOTATION_FILTER
			if(!UseAnnotationFilter || pLne->AnnotationFilter==0)
#endif
			{
				PicHasAnnotations = 1;

				if(pLne->Model_Code < MODELCODE::MODEL_NUM_ENTRIES)
					stats.Cnt.ModelCode[pLne->Model_Code]++;

				if(pLne->ClassId >= 0){	// because a negative picture has class id = -1
					ClassInPic[pLne->ClassId] = 1;
					CLASS_STATS *pClassStats = &stats.Class[pLne->ClassId];
					pClassStats->Count++;
					if(m_TargetWd>0.0f && m_TargetHt>0.0f){
						float ConvertedWd, ConvertedHt;
						ScaleToTarget(&pLne->box, ConvertedWd, ConvertedHt);
						MinMaxAvg(pClassStats->Wd, (double)ConvertedWd, i);
						MinMaxAvg(pClassStats->Ht, (double)ConvertedHt, i);

						int dp = (int)((pLne->box.y2 - pLne->box.y1) * (float)DISTRIBUTION_COUNT);
						dp = min(dp, DISTRIBUTION_COUNT-1); // because box size is inclusive from 0.0 to 1.0, not 0.0 to 0.9999 (i.e <1.0). Therefore if box is 1.0 this is the start of the DISTRIBUTION_COUNT+1 distribution interval, i.e, 1.0 to 1.1 if DISTRIBUTION_COUNT is 10.
						pClassStats->Distribution_H[dp]++;
						dp = (int)((pLne->box.x2 - pLne->box.x1) * (float)DISTRIBUTION_COUNT);
						dp = min(dp, DISTRIBUTION_COUNT-1);
						pClassStats->Distribution_W[dp]++;
					}
					stats.Cnt.Annotations++;
				}
			}
		}

		if(PicHasAnnotations){
			stats.Cnt.Pictures++;

			for(int c=0; c<_MAX_CLASSES_; c++)
				if(ClassInPic[c]) stats.Class[c].OnePerPicCount++;

			if(pFnG->pClsfdLines[0].ClassId < 0) stats.Cnt.Negative++;
			else stats.Cnt.Positive++;

			if(pFnG->pPicInfo->ColourCode < COLOURCODE::COLOUR_NUM_ENTRIES)
				stats.Cnt.ColourCode[pFnG->pPicInfo->ColourCode]++;
			if(pFnG->pPicInfo->PictureCode < PICTURECODE::PIC_NUM_ENTRIES)
				stats.Cnt.PictureCode[pFnG->pPicInfo->PictureCode]++;

			if(pFnG->Classified == 0) stats.Cnt.NotClsfd++;
			if(pFnG->Classified == CLASSIFIED_ACCEPT) stats.Cnt.Accept++;
			if(pFnG->Classified == CLASSIFIED_REFUSE) stats.Cnt.Refuse++;
			if(pFnG->Classified == CLASSIFIED_SPECIAL) stats.Cnt.Special++;
		}
	}
}

void CYoloExportDlg::StatisticsPopup(BOOL UseAnnotationFilter)
{
	if(m_pFiltrd && m_pFiltrd->Num>0 && m_pFiltrd->ppFNG){

		STATISTICS stats={0};
		LoadStatistics(stats, UseAnnotationFilter);

		CString Str, add;
		float avgapp = (float)stats.Cnt.Annotations / (float)stats.Cnt.Pictures;

		Str.Format("Pictures in source csv file(s): %d\r\n______________________________________________________\r\nStatistics that follow are for the current picture filter selection...\r\n______________________________________________________\r\nPICTURES : %d (%.2f%%)\r\n", 
			m_NumSourceFilenameGroups, stats.Cnt.Pictures, MakePercentage(stats.Cnt.Pictures, m_NumSourceFilenameGroups));
		add.Format("\r\nWd %s", GetMMAString(&stats.PicWd));
		Str += add;
		add.Format("\r\nHt %s", GetMMAString(&stats.PicHt));
		Str += add;
		add.Format("\r\n\r\nPositives : %d (%.2f%%)\r\nNegatives : %d (%.2f%%)\r\n\r\nNot classified : %d (%.2f%%)\r\nClassified accept : %d (%.2f%%)\r\nClassified refuse : %d (%.2f%%)\r\nClassified special : %d (%.2f%%)\r\n\r\nPics with objects with multiple probabilities : %d (%.2f%%)\r\n", 
			stats.Cnt.Positive, MakePercentage(stats.Cnt.Positive, stats.Cnt.Pictures), stats.Cnt.Negative, MakePercentage(stats.Cnt.Negative, stats.Cnt.Pictures), 
			stats.Cnt.NotClsfd, MakePercentage(stats.Cnt.NotClsfd, stats.Cnt.Pictures), stats.Cnt.Accept, MakePercentage(stats.Cnt.Accept, stats.Cnt.Pictures), stats.Cnt.Refuse, MakePercentage(stats.Cnt.Refuse, stats.Cnt.Pictures), stats.Cnt.Special, MakePercentage(stats.Cnt.Special, stats.Cnt.Pictures),
			stats.Cnt.MultipleProbsPics, MakePercentage(stats.Cnt.MultipleProbsPics, stats.Cnt.Pictures));
		Str += add;
		int i;
		for(i=0; i<COLOURCODE::COLOUR_NUM_ENTRIES; i++){
			if(stats.Cnt.ColourCode[i] > 0){
				add.Format("\r\n%s : %d (%.2f%%)", g_pColourCode[i], stats.Cnt.ColourCode[i], MakePercentage(stats.Cnt.ColourCode[i], stats.Cnt.Pictures));
				Str += add;
			}
		}
		Str += "\r\n";
		for(i=0; i<PICTURECODE::PIC_NUM_ENTRIES; i++){
			if(stats.Cnt.PictureCode[i] > 0){
				add.Format("\r\n%s : %d (%.2f%%)", g_pPictureCode[i], stats.Cnt.PictureCode[i], MakePercentage(stats.Cnt.PictureCode[i], stats.Cnt.Pictures));
				Str += add;
			}
		}
		Str += "\r\n";
		for(i=0; i<MODELCODE::MODEL_NUM_ENTRIES; i++){
			if(stats.Cnt.ModelCode[i] > 0){
				add.Format("\r\n%s : %d (%.2f%%)", g_pModelCode[i], stats.Cnt.ModelCode[i], MakePercentage(stats.Cnt.ModelCode[i], stats.Cnt.Annotations));
				Str += add;
			}
		}
		char *pWarn = "";
		if(m_TargetWd<=0.0f || m_TargetHt<=0.0f)
			pWarn = "\r\n\r\n***SET TARGET DIMENSIONS IN Options DIALOG***\r\n\r\n";

		add.Format("\r\n______________________________________________________\r\nANNOTATIONS : %d (avg per picture : %.2f)\r\n\r\nTarget size - Wd %d, Ht %d%s\r\n", stats.Cnt.Annotations, avgapp, (int)m_TargetWd, (int)m_TargetHt, pWarn);
		Str += add;
		Str += "Below, min/max - box pixel size scaled to target.\r\nNumber in square brackets after min and max is an example picture index.\r\nDistribution - relative size (0.0 to 1.0) in 0.1 increments.";
		CLASS_STATS *pClassStats = stats.Class;
		for(i=0; i<_MAX_CLASSES_; i++, pClassStats++){
			if(pClassStats->Count > 0){
				add.Format("\r\n\r\n[%d:%s] : %d (%.2f%%), pictures with : %d (%.2f%%)", i, g_CIdM.GetLabel(i), pClassStats->Count, MakePercentage(pClassStats->Count, stats.Cnt.Annotations), pClassStats->OnePerPicCount, MakePercentage(pClassStats->OnePerPicCount, stats.Cnt.Pictures));
				Str += add;
				if(m_TargetWd>0.0f && m_TargetHt>0.0f){
					AddSizeDistributionString("Wd", &pClassStats->Wd, pClassStats->Distribution_W, Str, add);
					AddSizeDistributionString("Ht", &pClassStats->Ht, pClassStats->Distribution_H, Str, add);
				}
			}
		}
		MsgBox2_buf(Str.GetBuffer(Str.GetLength()));
		Str.ReleaseBuffer();
	}
	else MsgBox2("nothing to display\r\n\r\nNum pictures : %d", m_pFiltrd->Num);
}

void CYoloExportDlg::AddSizeDistributionString(char *pLabel, MINMAXAVG *pMMA, int *pDistribution, CString &Str, CString &add)
{
	add.Format("\r\n    %s %s,  Distribution : ", pLabel, GetMMAString(pMMA));
	Str += add;
	for(int d=0; d<DISTRIBUTION_COUNT; d++){
		add.Format("%d,", pDistribution[d]);
		Str += add;
	}
}

void CYoloExportDlg::OnBnClickedButtonYoloAnchors()
{
	CalculateYoloAnchors(this, m_pFiltrd, m_TargetWd, m_TargetHt);
}
