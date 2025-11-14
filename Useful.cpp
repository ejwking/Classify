
#include "pch.h"
#include "Useful.h"
#include "MyMsgBox.h"


//AfxGetApp()->m_pMainWnd
//CWnd* pWnd = CWnd::FromHandle(AfxGetMainWnd()->m_hWnd);


int g_blocking_WM=0, g_didMessageBox=0;


BOOL FolderExists(char *pPath)
{
	DWORD dwAttrib = GetFileAttributes(pPath);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

BOOL FileExists(char *pPath)
{
	DWORD dwAttrib = GetFileAttributes(pPath);
	return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

char *PointerToFilename(char *pPath) // 'pointer to filename', NOT copy of.
{
	// also see _splitpath_s
	size_t l = strlen(pPath) - 1;
	while(l > 0){
		if(pPath[l] == '\\')
			return &pPath[l+1];
		l--;
	}
	return nullptr;
}

char *PointerToExtension(char *pPath) // 'pointer to extension', NOT copy of.
{
	// also see _splitpath_s
	size_t l = strlen(pPath) - 1;
	while(l > 0){
		if(pPath[l] == '.')
			return &pPath[l+1];
		l--;
	}
	return nullptr;
}

void FolderAndFilenameToPath(char *pPath, int PathCount, char *pFolder, char *pFilename)
{
	if(pFolder[0]!=0 && pFilename[0]!=0)
		sprintf_s(pPath, PathCount, "%s\\%s", pFolder, pFilename);
	else MsgBox2("error - FolderAndFilenameToPath");
}

void FullPathToFolderAndFilename(char *pFullPath, char *pFolder, int FolderCount, char *pFilename, int FilenameCount)
{
	char *pFn = PointerToFilename(pFullPath);
	strcpy_s(pFilename, FilenameCount, pFn);
	char restorechar = *pFn;
	*pFn = 0; // null terminate at the end of the folder string.
	strcpy_s(pFolder, FolderCount, pFullPath);
	*pFn = restorechar;
}

/*void toLowerStr(char *str)
{
	while(*str){ // MUST BE NULL TERMINATED
		*str = tolower((unsigned char)*str);
		str++;
	}
}*/

int FileHasBitmapExtension(char *pFileName)
{
	#define EXTSIZE 5
	size_t len = strlen(pFileName); // remember strlen doesnt include null.
	if(len >= EXTSIZE){
		char ext[EXTSIZE+1]; // +1 for null, so with 5 chars it will look like this "0.jpg" or ".jpeg".
		ext[EXTSIZE] = 0; // null
		for(int c=0; c<EXTSIZE; c++)
			ext[c] = tolower(pFileName[(len-5) + c]);
		
		if(strcmp(&ext[0], ".jpeg") == 0)
			return 1;
		if(strcmp(&ext[1], ".jpg") == 0)
			return 1;
		if(strcmp(&ext[1], ".bmp") == 0)
			return 1;
		if(strcmp(&ext[1], ".png") == 0)
			return 1;
	}
	return 0;
}

int FileHasCsvExtension(char *pFileName)
{
	size_t len = strlen(pFileName);
	if(len > 4){
		if(         pFileName[len-4]  == '.' && 
			tolower(pFileName[len-3]) == 'c' && 
			tolower(pFileName[len-2]) == 's' && 
			tolower(pFileName[len-1]) == 'v' ) return 1;
	}
	return 0;
}

int FileHasExtension( char *pFileName, char *pExtension )
{
	// this function really is as slow as it looks, hence version above.
	char LocalPath[_MAX_PATH], *pIns;
	strcpy_s( LocalPath, num_entries(LocalPath), pFileName );
	_strlwr_s( LocalPath, num_entries(LocalPath) );
	/* Check for extensions - at the end of the string */
	pIns = strstr( LocalPath, pExtension );
	if (pIns != NULL)
		if (strlen(pIns) == strlen(pExtension))
			return 1;
	return 0;
}

BOOL ChangeFileExtension(char *pPath, char *pExt)
{
	//size_t lext = strlen(pExt);
	size_t l = strlen(pPath) - 1;
	while(l > 0){
		if(pPath[l] == '.'){
			char *pApp = &pPath[l+1];
			strcpy_s(pApp, num_entries(pApp), pExt);
			return 1;
		}
		l--;
	}
	MsgBox2("failed - ChangeFileExtension\r\n %s\r\n %s", pPath, pExt);
	return 0;
}

int CreateNestedFolders(char *folderPath, char *pFirstFolderThatExists)
{
	// folderPath - FOLDER PATH WITHOUT FILENAME.
	if(!FolderExists(folderPath)){
		// created by ChatGPT (with some adjustments).
		char tempPath[MAX_PATH];
		char *p = NULL;
		size_t len;

		strcpy_s(tempPath, num_entries(tempPath), folderPath);
		len = strlen(tempPath);

		// Remove trailing slash, if any
		if(tempPath[len-1]=='\\' || tempPath[len-1]=='/')
			tempPath[len-1] = '\0';

		len = strlen(pFirstFolderThatExists) + 1;

		// Walk through each folder level (from pFirstFolderThatExists onwards) and create if not exist.
		for(p=tempPath+len; *p; p++){
			if(*p=='\\' || *p=='/'){
				*p = '\0'; // Temporarily end string here
				if(!CreateDirectory(tempPath, NULL)){ // Create directory if it doesn't exist
					if(GetLastError() != ERROR_ALREADY_EXISTS){
						MsgBox2("error - CreateNestedFolders \r\n%s\r\n%s", folderPath, pFirstFolderThatExists);
						return 0; // Failed for a reason other than already existing
					}
				}
				*p = '\\'; // Restore character
			}
		}
		// Create the final directory
		if(!CreateDirectory(tempPath, NULL)){
			if(GetLastError() != ERROR_ALREADY_EXISTS){
				MsgBox2("error - CreateNestedFolders \r\n%s\r\n%s", folderPath, pFirstFolderThatExists);
				return 0;
			}
		}
	}
	return 1;
}

int myGetFileSize(FILE *f)
{
	fseek(f, 0, SEEK_END);	// seek to end of file
	int Size = ftell(f);	// get current file pointer
	fseek(f, 0, SEEK_SET);	// seek back to beginning of file
	return Size;
}

//int BrowseForFolder( HWND hDlg, PTCHAR pPath, LPCTSTR szTitle )
int BrowseForFolder( HWND hDlg, char *pPath, char *szTitle )
{
	BROWSEINFO bi;
	LPITEMIDLIST lp;
	LPMALLOC lpmalloc;
	BOOL ret;
	ZeroMemory(&bi, sizeof(BROWSEINFO));
	bi.hwndOwner      = hDlg;
	bi.pidlRoot       = NULL;
	bi.lpszTitle      = (LPCTSTR)szTitle;
	bi.lpfn           = NULL;
	bi.ulFlags        = BIF_RETURNONLYFSDIRS;
	bi.pszDisplayName = pPath;
	lp = SHBrowseForFolder(&bi);
	if (lp != NULL){
		SHGetPathFromIDList(lp,pPath);
		ret = TRUE;
	}
	else
		ret = FALSE;
	SHGetMalloc(&lpmalloc);
	#ifdef __cplusplus
		lpmalloc->Free(lp);
		lpmalloc->Release();
	#else
		lpmalloc->lpVtbl->Free(lpmalloc,lp);
		lpmalloc->lpVtbl->Release(lpmalloc);
	#endif
	return ret;
}

void ProgressUpdate_SetWindowText(CWnd *pCWnd, char *pMsg, int idx, int num)
{
	static char Str[128];
	sprintf_s(Str, num_entries(Str), "%s - %d/%d", pMsg, idx, num);
	SetWindowText(pCWnd->m_hWnd, Str);
}

static char g_MsgText[1024]={0};

char *OutputDbgStr(const char *pFmt, ...)
{
#ifdef _DEBUG
	memset(g_MsgText, 0, num_entries(g_MsgText));
	va_list Ap;
	if(pFmt == NULL) return nullptr;
	va_start(Ap, pFmt);
		vsprintf_s(g_MsgText, num_entries(g_MsgText), pFmt, Ap);
	va_end(Ap);
	OutputDebugStringA(g_MsgText);
	return g_MsgText;
#else
	return nullptr;
#endif
}

int MsgBox_YN(const char *pFmt, ...)
{
	g_didMessageBox = 1;
	memset(g_MsgText, 0, num_entries(g_MsgText));
	va_list Ap;
	if(pFmt == NULL) return 0;
	va_start(Ap, pFmt);
		vsprintf_s(g_MsgText, num_entries(g_MsgText), pFmt, Ap);
	va_end(Ap);
	return (AfxMessageBox(g_MsgText, MB_YESNO, 0) == 6);
}

void MsgBox(const char *pFmt, ...)
{
	g_didMessageBox = 1;
	memset(g_MsgText, 0, num_entries(g_MsgText));
	va_list Ap;
	if(pFmt == NULL) return;
	va_start(Ap, pFmt);
		vsprintf_s(g_MsgText, num_entries(g_MsgText), pFmt, Ap);
	va_end(Ap);
	AfxMessageBox(g_MsgText);
}

void MsgBox2(const char *pFmt, ...)
{
	g_didMessageBox = 1;
	memset(g_MsgText, 0, num_entries(g_MsgText));
	va_list Ap;
	if(pFmt == NULL) return;
	va_start(Ap, pFmt);
		vsprintf_s(g_MsgText, num_entries(g_MsgText), pFmt, Ap);
	va_end(Ap);
	if(!g_blocking_WM){
		CMyMsgBox MMB;
		MMB.SetText(g_MsgText);
		MMB.DoModal();
	}
	else AfxMessageBox(g_MsgText);	// still not sure why AfxMessageBox doesnt need the message loop as well.
}

void MsgBox2_buf(char *pText)
{
	g_didMessageBox = 1;
	CMyMsgBox MMB;
	MMB.SetText(pText);
	MMB.DoModal();
}

float MakePercentage(int Val, int Total)
{
	return (((float)Val / (float)Total) * 100.0f);
}

double GetDistanceBetweenPoints(double x1, double y1, double x2, double y2)
{
	return sqrt(pow(x1 - x2, 2) + pow(y1 - y2, 2));
}

#define __RANDOM_NORMALISED	((float)rand() / (float)RAND_MAX)
int GetRandomInRange(int Min, int Max)
{
	static int seed=0;
	if(seed == 0){
		srand(1234);
		seed = 1;
	}
	float Range = (float)(Max - Min);
	int random = (int)(Range * __RANDOM_NORMALISED) + Min;
	return random;
}

int LoadEditboxFont(CWnd *pEditbox)
{
	// change the text size.
	// https://learn.microsoft.com/en-us/answers/questions/1307887/how-to-set-font-for-single-control-in-dialog

/*	// this doesnt work properly, 
	// lf.lfFaceName doesnt do anything, nor does changing lfHeight
	LOGFONT lf{};
	CFont NewFont, *pFont = pEditbox->GetFont(); // Get initial font
	pFont->GetLogFont(&lf); // Use LOGFONT to get font information
	lf.lfHeight = 120; // Use to create 12 point font
//	lf.lfFaceName = "Courier"
	NewFont.CreatePointFontIndirect(&lf); // Create a 12 point font
	pEditbox->SetFont(&NewFont, 0); // Have edit control use the 12 point font
	*/

	CFont m_font;
	int Point = 16;
	// wont let me change the font, size just seems to change space between lines, oh well its an improvement anyway
	m_font.CreateFont(Point,0,0,0,100,FALSE,FALSE,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,FF_SWISS,"Courier");
	pEditbox->SetFont(&m_font,FALSE);
	m_font.DeleteObject();
	return Point;
}

int GenerateTimestamp()
{
	char timestamp[10];  // 9 characters + null terminator
    SYSTEMTIME st;
    GetLocalTime(&st);  // Retrieves the current local time
	// Format: YMMDDHHMM, must be in this order so it can be put in an integer where the most recent timestamp has the biggest number, and therefore works for sorting pictures.
	// 
	// For simplisity I have chosen this timestamp format so it fits in a signed integer. So I could only have one digit of the year otherwise there would be too many digits 
	// for a signed integer. To include both digits of the year, which would be better, i should use an unsigned integer for timestamp.
	// (or save the timestamp in its component parts, year, month etc, but that wouldnt be so convenient when sorting the pictures).
	
	//snprintf(timestamp, num_entries(timestamp), "%02d%02d%02d%02d%02d",
	snprintf(timestamp, num_entries(timestamp), "%d%02d%02d%02d%02d",
             // (st.wYear % 100), last two digits of the year, 2025 to 25
			 (st.wYear % 100) % 10, // last one digit of the year, 2025 to 5
             st.wMonth,
             st.wDay,
             st.wHour,
			 st.wMinute);
	if(timestamp[0] == '0')
		MsgBox2("error - GenerateTimestamp, year has wrapped"); // will need to redo format. this will only happen in year 2100!
	int TS = atoi(timestamp);
	return TS;
}

char *TimestampToString(int ts)
{
	if(ts > 0){
		static char timestamp[24];
		char temp[16];
		sprintf_s(temp, num_entries(temp), "%d", ts);
		size_t len = strlen(temp);
		if(len == 9) // new format
			sprintf_s(timestamp, num_entries(timestamp), "%c%c/%c%c/x%c %c%c:%c%c", temp[1],temp[2], temp[3],temp[4], temp[0], temp[5],temp[6], temp[7],temp[8]);
		else if(len == 8) // old format.
			sprintf_s(timestamp, num_entries(timestamp), "%c%c/%c%c/x%c %c%c:%cx", temp[1],temp[2], temp[3],temp[4], temp[0], temp[5],temp[6], temp[7]);
		else return "format_error";
		return timestamp;
	}
	return "not_set";
}

// ################################################################################################################################################
// ################################################################################################################################################
// CGrowingMemoryPool

CGrowingMemoryPool g_MemPool;

//#define GBDBGMSG
char *CGrowingMemoryPool::__GetBuffer(int Num)
{
	if(m_MaxBuffers > 0){
		if(m_UsedElements+Num < m_MaxElements){
			char *pBuf = &m_ppBuf[m_BufIndex][m_UsedElements];
			m_UsedElements += Num;
			return pBuf;
		}
	}
	return nullptr;
}

int CGrowingMemoryPool::Grow()
{
	// 1. allocate next buffer if the last one is full up.
	// 2. increase number of buffer pointers (i.e. increase list size) with realloc if pointer list is full.

	if(m_BufIndex < m_NumBuffers-1){
		// this is the case when buffer is reused after a Reset. 
		// In other words, we are going through the buffers previously allocated in last usage, because memory is kept for the lifetime of the program.
		m_BufIndex++;
		m_UsedElements = 0;
		return 1;
	}
		
	if(m_NumBuffers < m_MaxBuffers){
#ifdef GBDBGMSG
		MsgBox("CGrowingMemoryPool, malloc block, m_NumBuffers %d(%d), m_MaxElements %d", m_NumBuffers, m_MaxBuffers, m_MaxElements);
#elif defined(SHOW_MEMORY_MSGS)
		//if(m_NumBuffers>0 && (m_NumBuffers%10)==0) MsgBox("CGrowingMemoryPool, malloc block\r\n\r\n m_NumBuffers %d(%d), m_MaxElements %d", m_NumBuffers, m_MaxBuffers, m_MaxElements);
#endif
		m_ppBuf[m_NumBuffers] = (char*)malloc(sizeof(char) * m_MaxElements);
		if(m_ppBuf[m_NumBuffers]){
			m_BufIndex = m_NumBuffers;
			m_UsedElements = 0;
			m_NumBuffers++;
			return 1;
		}
		MsgBox("error - CGrowingMemoryPool - malloc m_ppBuf[x] ");
	}
	else{
		// there should be a fairly small number (maybe 20 max) of big blocks of memory in our pool, so we should not ever be doing a second realloc here.
		m_MaxBuffers += 200;
		char **ppOldBuf = m_ppBuf;
		if((m_ppBuf = (char**)realloc((void*)m_ppBuf, sizeof(char*) * m_MaxBuffers)) == 0){
			// realloc failed.
			if(ppOldBuf)
				free(ppOldBuf);
			m_ppBuf = nullptr;
			MsgBox2("error - CGrowingMemoryPool - m_MaxBuffers realloc (%d)", m_MaxBuffers);
		}
		else{
			// recursive call. run this function again now the number of pointers in the list has been increased.
#ifdef GBDBGMSG
			MsgBox("CGrowingMemoryPool, realloc list, m_MaxBuffers %d", m_MaxBuffers);
#endif
			return Grow();
		}
	}
	return 0;
}

void CGrowingMemoryPool::FreeMem()	// make this public if we need to call Initialise() a second time with re-calculated constants.
{
	if(m_ppBuf){
		for(int i=0; i<m_NumBuffers; i++)
			free(m_ppBuf[i]);
		free(m_ppBuf);
	}
	m_ppBuf = nullptr;
}

CGrowingMemoryPool::CGrowingMemoryPool()
{
	m_ppBuf = nullptr;
}

CGrowingMemoryPool::~CGrowingMemoryPool()
{
	FreeMem();
}

int CGrowingMemoryPool::Initialise(int NumElementsPerBlock)
{
	if(m_ppBuf == nullptr){
		m_MaxBuffers   = 0;
		m_MaxElements  = NumElementsPerBlock;
		m_NumBuffers   = 0;
		m_BufIndex     = 0;
		m_UsedElements = 0;
		return 1;
	}
	else MsgBox("error - CGrowingMemoryPool - already initialised");
	return 0;
}

void CGrowingMemoryPool::Reset()
{
	m_BufIndex = 0;
	m_UsedElements = 0;
}

char *CGrowingMemoryPool::GetBuffer(int NumChars)
{
	if(NumChars+1000 < m_MaxElements){
		char *pBuf = __GetBuffer(NumChars);
		if(pBuf)
			return pBuf;
		if(Grow())
			return __GetBuffer(NumChars);
	}
	else MsgBox("error - CGrowingMemoryPool bad usage \n\n GetBuffer(%d), block size %d", NumChars, m_MaxElements);
	return nullptr;
}

void CGrowingMemoryPool::GetUsageStatistics(int &NumBuffers, int &MaxElements)
{
	NumBuffers = m_NumBuffers;
	MaxElements = m_MaxElements;
}

// ################################################################################################################################################
// ################################################################################################################################################
// CClassIdManager

CClassIdManager g_CIdM;

void CClassIdManager::AddClass(int Category, char *pLabel)
{
	if(m_NumClasses < _MAX_CLASSES_){
		memset(&m_Labels[m_NumClasses], 0, sizeof(CLASS_LABEL));
		m_Labels[m_NumClasses].Category = Category;
		m_Labels[m_NumClasses].pLabel = pLabel;
		m_NumClasses++;
	}
	else MsgBox("error - add class");
}

void CClassIdManager::AddDetectionClasses()
{
	AddClass(CATEGORY_DETECTIONS, "car");
	AddClass(CATEGORY_DETECTIONS, "van");
	AddClass(CATEGORY_DETECTIONS, "pickup");
	AddClass(CATEGORY_DETECTIONS, "truck");
	AddClass(CATEGORY_DETECTIONS, "bus");
	AddClass(CATEGORY_DETECTIONS, "moto");
	//AddClass(CATEGORY_DETECTIONS, "bicycle");


	// what are these below? ..in addition to the other methods of assigning a box with a class id you can also hold down SHIFT and 
	// hover the mouse over a box and use the mouse buttons to assign a class as per these values below.. 
	//
	m_Id_MouseLBtnDown   = "remove"; // left mouse button down, etc etc
	m_Id_MouseLBtnDlbClk = "blurr";
	m_Id_MouseRBtnDown   = "van";
	m_Id_MouseMBtnDown   = "pickup";
}

CClassIdManager::CClassIdManager()
{
	for(int i=0; i<_MAX_CLASSES_; i++)
		memset(&m_Labels[i], 0, sizeof(CLASS_LABEL));

	// category/class: this is not equivalent to class/subclass, we dont do class/subclass in this program.
	//  category is used in the annotation merging rules (FixAndCropPictures.cpp). For example, to check whether a licence plate is located inside a vehicle.
	m_NumClasses = 0;

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// CATEGORY_DETECTIONS
	// must come first.
	AddDetectionClasses();
	m_NumDetectionClasses = m_NumClasses;

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// CATEGORY_REMOVE 
	// allows the user to specify regions of the picture that should be removed or blurred, these annotations are processed by FixAndCropPicture.cpp.
	AddClass(CATEGORY_REMOVE, "remove");
	AddClass(CATEGORY_REMOVE, "blurr");
	m_NumClasses_IncRemoveAndBlurr = m_NumClasses;

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// CATEGORY_CZ_DEBUG
	// For debugging the 'crop zone' and object blurring code in FixAndCropPicture.cpp
	AddClass(CATEGORY_CZ_DEBUG, "cz_augm_only");
	AddClass(CATEGORY_CZ_DEBUG, "cz_contain_all");
	AddClass(CATEGORY_CZ_DEBUG, "cz_multi");
	AddClass(CATEGORY_CZ_DEBUG, "cz_multi_2nd");
	AddClass(CATEGORY_CZ_DEBUG, "cz_multi_noblr");
	AddClass(CATEGORY_CZ_DEBUG, "cz_single");
	AddClass(CATEGORY_CZ_DEBUG, "cz_single_noblr");
	AddClass(CATEGORY_CZ_DEBUG, "cz_fullpic_GOT");
	AddClass(CATEGORY_CZ_DEBUG, "cz_obj_blurred");

	m_FirstGotoId = 0;	// when we create a new annotation in the UI this will be the default class id.
}

int CClassIdManager::GetClassId(char *plabel, int *pclass_id)
{
	// excludes CATEGORY_CZ_DEBUG
	*pclass_id = 0;
	if(strcmp("neg", plabel) == 0){
		*pclass_id = -1;
		return 1;
	}
	if(strcmp("negative", plabel) == 0){
		*pclass_id = -1;
		return 1;
	}
	for(int x=0; x<m_NumClasses; x++){
		if(strcmp(m_Labels[x].pLabel, plabel) == 0){
			*pclass_id = x;
			return 1;
		}
	}
	MsgBox("error - GetClassId, no match: %s", plabel);
	return 0;
}

char *CClassIdManager::GetLabel(int class_id)
{
	// includes CATEGORY_CZ_DEBUG
	if(class_id == -1)
		return "neg";
	if(class_id>=0 && class_id<_MAX_CLASSES_)
		return m_Labels[class_id].pLabel;
	MsgBox("error - GetLabel, no match: %d", class_id);
	return nullptr;
}

int CClassIdManager::IdToConvertedId(int class_id)
{
	return m_Labels[class_id].ConvertedId;
}

int CClassIdManager::GetPreferredId()
{
	return m_FirstGotoId;
}

int CClassIdManager::Id_inCategory(int class_id, int category)
{
	if(class_id>=0 && class_id<_MAX_CLASSES_)
		return (m_Labels[class_id].Category == category);
	return 0;
}

/*	int CClassIdManager::ClassIdIsValid(int class_id)
{
	if(class_id == -1)
		return 1;
	if(class_id>=0 && class_id<m_NumClasses)
		return 1;
	return 0;
}*/

// ################################################################################################################################################
// ################################################################################################################################################
// make directory list

int PathIsDir( char *pPath )
{
	int IsDir, Attr;
	Attr = GetFileAttributes( pPath );
	IsDir = (Attr & FILE_ATTRIBUTE_DIRECTORY) && (Attr != -1);
	return IsDir;
}

void StripTrailingSpaces( char *pStr )
{
	int Len = (int)strlen(pStr);
	char *pEndStr = pStr + Len;
	while (Len--)
		if (*--pEndStr <= ' ') *pEndStr = 0;
		else break;
}

void KillDoubleSlashes( char *pSrc )
{
	char *pDst = pSrc;
	while (*pSrc){
		*pDst = *pSrc++;
		if ((*pSrc == '\\') && (*pDst == '\\'))
			pSrc++;
		pDst++;
	}
	*pDst = 0;
	/*** compiler does not like this...
	while (*pDst = *pSrc++)
	{
		if ((*pSrc == '\\') && (*pDst == '\\')) *pSrc++;
		pDst++;
	}
	***/
}

#define DIR_LIST_MAGIC 0x80000000 // QuickLoad is usually true/false, but we can add special instructions (rather than adding an extra parameter)
#define DIR_LIST_PURGE 0x04000000 // delete empty folders
//#define DIR_LIST_MASK  0xffff0000 // can still use the bottom 16 bits for other modes

// [IN] MayHaveWildCard - add wildcard search to end of path
// [IN] pPath - source path
// [IN] pSubDir - sub-dir to path, if wanted (can be NULL)
// [IN] pFileName - specific file to search for, if wanted (can be NULL)
// [OUT] pDir - destination for the complete search path
// [IN] DirSize - pDir buffer max
// [OUT] pWildName - destination for the wildcard part of the search
// [IN] WildNameSize - pWildName buffer max
char* AssembleSearch( int MayHaveWildCard, const char *pPath, const char *pSubDir, const char *pFileName, char *pDir, int DirSize, char *pWildName, int WildNameSize )
{
	int Len;
	char LocalPath[_MAX_PATH], LocalDir[_MAX_PATH], LocalName[_MAX_PATH], LocalDrive[_MAX_PATH], LocalExt[_MAX_PATH];

	// Similar to PathToDirAndName() with some extra checks for directories
	_fullpath( LocalPath, pPath, num_entries(LocalPath) );
	StripTrailingSpaces( LocalPath ); // It can happen

	if (MayHaveWildCard){
		if (PathIsDir( LocalPath ))
		//	cs_tcscat
			strcat_s( LocalPath, num_entries(LocalPath), "\\*.*" ); // check that path is a file and not a directory
	}
	else{
		// This must be a directory
		strcat_s( LocalPath, num_entries(LocalPath), "\\*.*" );
		KillDoubleSlashes( LocalPath ); // happens with 'c:\\'
	}

	_splitpath_s( LocalPath, LocalDrive, num_entries(LocalDrive), LocalDir, num_entries(LocalDir), LocalName, num_entries(LocalName), LocalExt, num_entries(LocalExt) );
	// New dir
	_makepath_s( pDir, DirSize, LocalDrive, LocalDir, "", "" );
	// Wildcard
	_makepath_s( pWildName, WildNameSize, "", "", LocalName, LocalExt );
	// add sub dir (if wanted)
	if (pSubDir != NULL)
	{
		Len = (int)strlen( pDir );
		if (Len > 1)
		{
			if (pDir[Len-1] != '\\') strcat_s( pDir, _MAX_PATH, "\\" );
		}
		strcat_s( pDir, _MAX_PATH, pSubDir );
	}

	// Add search (if wanted)
	if (pFileName != NULL)
	{
		Len = (int)strlen( pDir );
		if (Len > 1)
		{
			if (pDir[Len-1] != '\\') strcat_s( pDir, _MAX_PATH, "\\" );
		}
		strcat_s( pDir, _MAX_PATH, pFileName );
	}

	KillDoubleSlashes( pDir ); // happens with 'c:\\'
	return( pDir );
}

// note: to make this more usable we may have to add a couple of void* items to the action function
//
int MakeDirList( int Mode, int Going, HWND hDlg, int Recurse, int FileBias, char *pDir, char *pWildName, int(*action_func)(int IsFile, int Mode, char *pPath, char *pName, void *pData), int(*interrupt_func)(HWND hDlg), void *pData)
{
	int Ok, NumEntriesThisDir;
	WIN32_FIND_DATA FileData;
	HANDLE handle;
	char TheSearchPath[_MAX_PATH], TheSearchName[_MAX_PATH], TheDir[_MAX_PATH];

	AssembleSearch( TRUE, pDir, NULL, pWildName, TheSearchPath, num_entries(TheSearchPath), TheSearchName, num_entries(TheSearchName) );
	handle = FindFirstFile( TheSearchPath, &FileData );
	if (handle != INVALID_HANDLE_VALUE){
		NumEntriesThisDir = 0;
		do{
			if ((FileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			{
				// '.' and '..' are special - skip those
				if ((_stricmp( FileData.cFileName, "." ) != 0) && (_stricmp( FileData.cFileName, ".." ) != 0))
				{
					// assemble new directory
					sprintf_s( TheDir, num_entries(TheDir), "%s\\%s", pDir, FileData.cFileName );
					// deal with the directory itself - not that returning FALSE here will skip the folder
					
					if ((action_func)( FALSE, Mode, &TheDir[FileBias], "", pData ))
					{
						// and dive into it when recursing
						if (Recurse)
						{
							/* its a directory */
							if (Going) Going = MakeDirList( Mode, Going, hDlg, Recurse, FileBias, TheDir, pWildName, action_func, interrupt_func, pData );
						}
					}
				}
			}
			else{
				Ok  = ((FileData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) == 0);
				Ok &= ((FileData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) == 0);
				if (Ok){
					/* Its a file */
					if (pData)
					{
						(action_func)( TRUE, Mode, &pDir[FileBias], FileData.cFileName, pData);
					}
					else {
						if (Mode & DIR_LIST_MAGIC) // special instructions passed into top 16 bits
						{
							// very special - return file record so we can get the filesize!
							(action_func)( TRUE, Mode, &pDir[FileBias], FileData.cFileName, &FileData );
						}
						else {
							//Jan: if we have not sent any special data formerly, we will opearte with date of the last write to file -  for listview
							(action_func)( TRUE, Mode, &pDir[FileBias], FileData.cFileName, &FileData.ftLastWriteTime);
						}
					}
				}
			}
			NumEntriesThisDir++;

			if ((interrupt_func != NULL) /*&& (hDlg != NULL)*/)
				Going &= interrupt_func( hDlg );

		} while (FindNextFile( handle, &FileData ) && Going);

		FindClose( handle );

		// if we just have '.' and '..' entries the folder is empty
		if ((NumEntriesThisDir == 2) && (Mode & DIR_LIST_PURGE))
		{
		//	FormatDebugStringEx( DEBUG_NORMAL, "ListDir (purge) - removing empty folder %s\n", pDir );
		//	_trmdir( pDir );
		}
	}
	return( Going );
}

// ################################################################################################################################################
// ################################################################################################################################################
