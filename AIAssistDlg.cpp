// AIAssistDlg.cpp : implementation file
//

#include "pch.h"
#include "Classify.h"
#include "afxdialogex.h"
#include "Useful.h"
#include "AIAssistDlg.h"
#include "ollama.hpp"

static int g_CurIndex = 0;
static CString g_LastQuery = "";

// CAIAssistDlg dialog

IMPLEMENT_DYNAMIC(CAIAssistDlg, CDialogEx)

CAIAssistDlg::CAIAssistDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_DIALOG_AI_ASSIST, pParent)
{
	m_StrQuery = g_LastQuery;
	m_pBitmapPath = nullptr;
}

CAIAssistDlg::~CAIAssistDlg()
{
}

void CAIAssistDlg::SetParams(char *pBitmapPath)
{
	m_pBitmapPath = pBitmapPath;
}

void CAIAssistDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);

//	DDX_Control(pDX, IDC_EDIT_QUERY, m_EditQuery);
	DDX_Text(pDX, IDC_EDIT_QUERY, m_StrQuery);
	DDX_Text(pDX, IDC_EDIT_RESPONSE, m_StrResponse);
	DDX_Control(pDX, IDC_COMBO_MODELS, m_ComboModel);
}


BEGIN_MESSAGE_MAP(CAIAssistDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BUTTON_QUERY, &CAIAssistDlg::OnBnClickedButtonQuery)
	ON_BN_CLICKED(IDC_BUTTON_AI_INFO, &CAIAssistDlg::OnBnClickedButtonAiInfo)
END_MESSAGE_MAP()


// CAIAssistDlg message handlers

/*****************************************************************************
 
 gemma3 - READ UP WHAT THIS MODEL CAN DO, THE PARAMETERS ETC.
 https://deepinfra.com/google/gemma-3-27b-it

 https://github.com/jmont-dev/ollama-hpp?tab=readme-ov-file#generation-using-images


reduce the number of output tokens could get rid of the unwanted stuff in the response, MIGHT MAKE IT QUICKER TOO.
need to check the other options too.
also i got to do license plate, probably gotta use Grounding DINO

==============================================
query ideas :
---------------------------
 
is this a vehicle, yes or no?
if its a vehicle what class, make and model?
is this a view of the front, rear, side, interior?
what is the license plate?


in csv format answer the following questions:
is this a motorbike, car, van, food truck, pickup truck, heavy truck, bus, double-decker bus?
what the make of vehicle?
is this a view of the front, rear, side, interior?
what is the license plate?

THE POINT of all these classes is to kepp Gemma3 happy, cos it wants to report these classes, i can then filter those i want.

pick the one of the following categories to best define the vehicle: motorbike, car, van, truck, bus.
write a csv row for each vehicle in the image as follows, in csv format give me, vehicle type, vehicle make, vehicle orientation, vehicle truncated, license plate.
==============================================
*/

static void SetModelOptions(ollama::options &options)
{
/*
change config  (maybe create a Modelfile with my config).
make sure model is as quick as pos, eg, that its not reloading each time.
*/
	// Access and set these options like any other json type.
//	options["seed"] = 1;
	options["temperature"] = 0;
//	options["num_predict"] = 18;

/*
 "options": {
    "num_keep": 5,
    "seed": 42,
    "num_predict": 100,
    "top_k": 20,
    "top_p": 0.9,
    "min_p": 0.0,
    "typical_p": 0.7,
    "repeat_last_n": 33,
    "temperature": 0.8,
    "repeat_penalty": 1.2,
    "presence_penalty": 1.5,
    "frequency_penalty": 1.0,
    "mirostat": 1,
    "mirostat_tau": 0.8,
    "mirostat_eta": 0.6,
    "penalize_newline": true,
    "stop": ["\n", "user:"],
    "numa": false,
    "num_ctx": 1024,
    "num_batch": 2,
    "num_gpu": 1,
    "main_gpu": 0,
    "low_vram": false,
    "vocab_only": false,
    "use_mmap": true,
    "use_mlock": false,
    "num_thread": 8
  }*/
}

void CAIAssistDlg::OnBnClickedButtonQuery()
{
	UpdateData(1); // from dlg

	g_LastQuery = m_StrQuery;

//	ollama::show_requests(true);
//	ollama::show_replies(true);

	// Exceptions can be dynamically enabled and disabled through this call.
	// If exceptions are true, ollama::exception will be thrown in the event of errors. If exceptions are false, functions will either return false or empty values.
	ollama::allow_exceptions(true);


//    char *user_msg = "in csv format give me, vehicle type, vehicle make, vehicle orientation, license plate.";
	char *user_msg = m_StrQuery.GetBuffer(m_StrQuery.GetLength());

	/*
	ollama::message message_with_image("user", user_msg, image);
	//   std::string AiResult = ollama::chat("llava", message_with_image);
	std::string AiResult = ollama::chat("gemma3", message_with_image);
	*/


/*	ollama::request request(ollama::message_type::generation);
	request["model"] = "gemma3";//"mistral";
	request["prompt"] = user_msg;
	request["temperature"] = 0;
	//request["system"] = "Talk like a pirate for the next reply.";
	std::string AiResult = ollama::generate(request);
	*/
	ollama::options options;

	SetModelOptions(options);

//	ollama::response response = ollama::generate("gemma3", user_msg, options, image);

	char SelectedModel[64];
	g_CurIndex = m_ComboModel.GetCurSel();
	m_ComboModel.GetWindowText(SelectedModel, num_entries(SelectedModel));

	DWORD dwTicks = GetTickCount();
	SetWindowText("...waiting for response");
	std::string AiResult;
	std::string AiException;
	try
	{
		ollama::image image = ollama::image::from_file(m_pBitmapPath);
		AiResult = ollama::generate(SelectedModel, user_msg, options, image);
	}
	catch(ollama::exception& e)
	{
		AiException = e.what();
		MsgBox2(AiException.c_str());
	}
	m_StrQuery.ReleaseBuffer();
	float ftm = (float)(GetTickCount() - dwTicks);
	CString TimeStr;
	TimeStr.Format("%.2f seconds ...ready", ftm/1000.0f);
	SetWindowText(TimeStr);
	
	static char nlstr[2056];
	const char *pcstr = AiResult.c_str();
	int len = (int)strlen(pcstr);

	int nlsl = 0;
	for(int i=0; i<len; i++){
		if(nlsl < 2050){
			if(pcstr[i] == '\n')
				nlstr[nlsl++] = '\r';
			nlstr[nlsl++] = pcstr[i];
		}
	}
	nlstr[nlsl] = 0;
	m_StrResponse = nlstr;
	UpdateData(0); // to dlg
}

BOOL CAIAssistDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();
	// TODO:  Add extra initialization here

	m_ComboModel.ResetContent();

/*
	// this is much faster than calling ollama for the list.
	m_ComboModel.AddString("qwen3:1.7b");
	m_ComboModel.AddString("qwen2.5vl:7b"); // try the smaller vl one too
	m_ComboModel.AddString("llama2-uncensored:latest");
	m_ComboModel.AddString("llama3.2-vision:latest");
	m_ComboModel.AddString("gemma3:12b");
	m_ComboModel.AddString("gemma3:1b");
	m_ComboModel.AddString("llava:latest");
	m_ComboModel.AddString("gemma3:latest");
	// ollama rm moondream
	// ollama rm granite3.2
*/

	CString InfoStr;
	std::vector<std::string> models = ollama::list_models();    
	InfoStr = "These models are locally available: \r\n";
	for(std::string model : models){
		InfoStr = model.c_str();
		m_ComboModel.AddString(InfoStr);

		//copy and paste models from output window
		//OutputDebugString("\n");
		//OutputDebugString(InfoStr);
	}

	m_ComboModel.SetCurSel(g_CurIndex);

	return TRUE;  // return TRUE unless you set the focus to a control
	// EXCEPTION: OCX Property Pages should return FALSE
}

// "gemma3 - The current, most capable model that runs on a single GPU."

void CAIAssistDlg::OnBnClickedButtonAiInfo()
{
	CString InfoStr;
	std::vector<std::string> models = ollama::list_models();    
	InfoStr = "These models are locally available: \r\n";
	for(std::string model : models){
		InfoStr += model.c_str();
		InfoStr += "\r\n";
	}
	std::vector<std::string> runningmodels = ollama::list_running_models();
	InfoStr += "\r\nThese models are running: \r\n";
	for(std::string runningmodel : runningmodels){
		InfoStr += runningmodel.c_str();
		InfoStr += "\r\n model info: ";

		ollama::json model_info = ollama::show_model_info(runningmodel);
		std::string mi = model_info["details"]["family"];
		InfoStr += mi.c_str();
		InfoStr += "\r\n";
	}
/*
	// Request model info from the Ollama server.
    ollama::json model_info = ollama::show_model_info("gemma3");
    std::cout << "Model family is " << model_info["details"]["family"] << std::endl;
	*/

	GetDlgItem(IDC_EDIT_AI_INFO)->SetWindowText(InfoStr);
}
