
#include "pch.h"
#include "InterfaceAI.h"

#ifdef jjjjjjjjjjjjjjjjjjjj
#include "Useful.h"

//#include "AIAssistDlg.h"
//#include "ollama.hpp"


void CInterfaceAI::testfunc(char *pBitmapPath)
{
    /*
//	ollama::chat("llaVa", "what is in the picture?");

	ollama::show_requests(true);
    ollama::show_replies(true);

    // Exceptions can be dynamically enabled and disabled through this call.
    // If exceptions are true, ollama::exception will be thrown in the event of errors. If exceptions are false, functions will either return false or empty values.
    ollama::allow_exceptions(true);



    ollama::image image = ollama::image::from_file(pBitmapPath);

    char *user_msg = "in csv format give me, vehicle type, vehicle make, vehicle orientation, license plate.";

    ollama::message message_with_image("user", user_msg, image);
    //   std::string AiResult = ollama::chat("llava", message_with_image);
    std::string AiResult = ollama::chat("gemma3", message_with_image);

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

    CString output;
    output = "me:\r\n";
    output += user_msg;
    output += "\r\n\r\nAI assistant:\r\n";
    output += nlstr;

    MsgBox2_buf(output.GetBuffer(output.GetLength()));
    output.ReleaseBuffer();
    */
}

/*
Next 
want to change config  (maybe create a Modelfile with my config).
eg, temperature to zero.
make sure model is as quick as pos, eg, that its not reloading each time.

gemma3 - The current, most capable model that runs on a single GPU.
-------------------------------------------------------------------
try different models, gemma seems best so far, and different ways of asking the question.


ollama::options options;

// Access and set these options like any other json type.
options["seed"] = 1;
options["temperature"] = 0;
options["num_predict"] = 18;

*/

#endif

