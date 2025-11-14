
#pragma once

#ifdef _DEBUG
	#define SHOW_MEMORY_MSGS
#else
	// ALWAYS DEFINED
	#define SHOW_MEMORY_MSGS
#endif

//#define OUTPUT_DEBUG_STRING	// console debug strings

#define num_entries(name) (sizeof(name)/sizeof((name)[0]))
#define CSV_FILE_FILTERS	"csv files (*.csv)|*.csv|"

struct myRECT
{
	// for general use, not with the windows API.
	int left, right, top, bottom;
};

// file related..
extern int  myGetFileSize(FILE *f);
extern BOOL FolderExists(char *pPath);
extern BOOL FileExists(char *pPath);
extern int  FileHasExtension(char *pFileName, char *pExtension);
extern int  FileHasCsvExtension(char *pFileName);
extern int  FileHasBitmapExtension(char *pFileName);
extern BOOL ChangeFileExtension(char *pPath, char *pExt);
extern void FullPathToFolderAndFilename(char *pFullPath, char *pFolder, int FolderCount, char *pFilename, int FilenameCount);
extern void FolderAndFilenameToPath(char *pPath, int PathCount, char *pFolder, char *pFilename);
extern char *PointerToFilename(char *pPath);
extern char *PointerToExtension(char *pPath);
extern int  CreateNestedFolders(char *folderPath, char *pFirstFolderThatExists);
extern int  BrowseForFolder(HWND hDlg, char *pPath, char *szTitle);
extern int  MakeDirList(int Mode, int Going, HWND hDlg, int Recurse, int FileBias, char *pDir, char *pWildName, int(*action_func)(int IsFile, int Mode, char *pPath, char *pName, void *pData), int(*interrupt_func)(HWND hDlg), void *pData);

// text and message box..
extern int  LoadEditboxFont(CWnd *pEditbox);
extern void ProgressUpdate_SetWindowText(CWnd *pCWnd, char *pMsg, int idx, int num);
extern void MsgBox(const char *pFmt, ...);
extern int  MsgBox_YN(const char *pFmt, ...);
extern char *OutputDbgStr(const char *pFmt, ...);
extern void MsgBox2_buf(char *pText);
extern void MsgBox2(const char *pFmt, ...);
extern int  g_blocking_WM, g_didMessageBox;

// other..
extern double GetDistanceBetweenPoints(double x1, double y1, double x2, double y2);
extern float MakePercentage(int Val, int Total);
extern int GetRandomInRange(int Min, int Max);
extern int GenerateTimestamp();
extern char *TimestampToString(int ts);


// #########################################################################################################################
class CGrowingMemoryPool
{
	// usage : *GetBuffer(int NumChars) - Get a pointer to a char buffer of with Num entries.
private:
	char **m_ppBuf;	// buffer list
	int    m_UsedElements, m_MaxElements;
	int    m_NumBuffers, m_MaxBuffers, m_BufIndex;
	char  *__GetBuffer(int Num);
	int    Grow();
	void   FreeMem();
public:
	CGrowingMemoryPool();
	~CGrowingMemoryPool();
	int   Initialise(int NumElementsPerBlock);
	void  Reset();
	char *GetBuffer(int NumChars);
	void  GetUsageStatistics(int &NumBuffers, int &MaxElements);
};

extern CGrowingMemoryPool g_MemPool;

// #########################################################################################################################
enum CLASS_CATEGORY
{
	CATEGORY_DETECTIONS=1,
	CATEGORY_REMOVE,
	CATEGORY_CZ_DEBUG,
};

#define _MAX_CLASSES_	20	// todo (not important) - make it not hard coded, and everywhere else this macro is used buffer must be malloc'ed (once).

struct CLASS_LABEL
{
	int   Present;
	int   ConvertedId;
	int   Category;
	char *pLabel;
};

class CClassIdManager
{
public://private:
	CLASS_LABEL m_Labels[_MAX_CLASSES_];
	int m_FirstGotoId;

public:
	int m_NumClasses, m_NumDetectionClasses, m_NumClasses_IncRemoveAndBlurr;
	char *m_Id_MouseLBtnDown, *m_Id_MouseLBtnDlbClk, *m_Id_MouseRBtnDown, *m_Id_MouseMBtnDown;
	CClassIdManager();

	void AddDetectionClasses();
	void AddClass(int Category, char *pLabel);
	int GetClassId(char *plabel, int *pclass_id);
	char *GetLabel(int class_id);
	int IdToConvertedId(int class_id);
	int GetPreferredId();
	int Id_inCategory(int class_id, int category);
/*	int ClassIdIsValid(int class_id) */
};

extern CClassIdManager g_CIdM;

// #########################################################################################################################
enum MODELCODE
{
	MODEL_UNDEFINED = 0,
	MODEL_BYHAND,		// or call this 'human'.
	MODEL_PRELABELLED,
	MODEL_LPR,
	MODEL_GRD_DINO,
	MODEL_YOLOV4,
	MODEL_YOLOV7,
	MODEL_NUM_ENTRIES,
	// it would be better to have a model "string" in the csv file instead of these codes, gonna stick with this for now (until i have time to do it), I can convert csv files later.
};
static const char *g_pModelCode[MODEL_NUM_ENTRIES] = { "MODEL_UNDEFINED", "MODEL_BYHAND", "MODEL_PRELABELLED", "MODEL_LPR", "MODEL_GRD_DINO", "MODEL_YOLOV4", "MODEL_YOLOV7" };


enum PICTURECODE
{
	PIC_UNDEFINED = 0,
	PIC_COCO,
	PIC_ANPR,
	PIC_DASHCAM,
	PIC_YOUTUBE,
	PIC_NUM_ENTRIES,
};
static const char *g_pPictureCode[PIC_NUM_ENTRIES] = { "PIC_UNDEFINED", "PIC_COCO", "PIC_ANPR", "PIC_DASHCAM", "PIC_YOUTUBE" };


enum COLOURCODE
{
	COLOUR_UNDEFINED = 0,
	COLOUR_YES,
	COLOUR_NO,
	COLOUR_NUM_ENTRIES,
};
static const char *g_pColourCode[COLOUR_NUM_ENTRIES] = { "COLOUR_UNDEFINED", "COLOUR_YES", "COLOUR_NO" };
