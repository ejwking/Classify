# Classify - YOLO Annotation Tool for Windows

C++ Windows GUI for marking bounding boxes of objects in images and classifying images as 'accept' or 'refuse'.<br>
Use this program to open, edit and save .csv files containing your annotations.<br>
Export the YOLO path list and YOLO format annotation files.

## Usage options

### Semi-automatic method - Use a pre-trained object detection model to write an annotations .csv file

The .csv must contain one annotation per line in the following format..<br>
filename_count*,picture_code*,colour_code*,width,height,filename,box_x1,box_y1,box_x2,box_y2,class_id,confidence*,model_code*<br>
Fields marked with * are optional.<br>
filename must be in "quotes" and is relative to the 'base picture folder' (specified in the UI).<br>
For example I have used MSCOCO pre-trained YOLOv7 models to annotate vehicles in my images, and here are the first 3 lines of my .csv file:<br>
1,0,1,640,480,"coco2017\Veh\000000000001.jpg",0.008536,0.245823,0.717354,0.625195,car,88,2<br>
1,0,1,640,480,"coco2017\Veh\000000000001.jpg",0.419244,0.002830,0.998441,0.463768,truck,99,2<br>
2,0,1,480,640,"coco2017\Veh\000000000064.jpg",0.109182,0.554797,0.367805,0.647733,truck,77,2<br>


### Manual method - You don't have a .csv file with model generated annotations and wish to define all annotations by hand

In the UI select the option to generate a .csv file of blank entries for a specified picture folder.


## Other features

* Options for filtering and sorting the pictures/annotations
* Dataset statistics
* Calculate YOLO anchors
* Export all pictures at YOLO target dimensions to an image cache folder with the accompanying YOLO annotation files
* Using Ollama to run LLMs locally, query an LLM on the current picture (see screenshot). With more development this could be used to further automate the classification process


## Installation and Setup

Open classify.sln in Visual Studio, compile and run. That's it, simple.<br>
I have not yet added a UI for adding classes, so do this manually in the code before you compile. Add your desired classes to Useful.cpp function AddDetectionClasses(). Currently it includes classes for vehicle detection.


## Specialised feature

Annotations 'remove' and 'blurr' are used to crop out unwanted sections of the picture, and blur sections of the picture.
CSV files with these annotations must be processed by the code in FixAndCropPictures.cpp. The amended pictures are exported to a separate folder.
This feature was written for my very specific purpose, and is unlikely to be useful to you unless you adapt the code for your own purpose.

<br>

![mainA](images/mainA.PNG)

<br>

![mainR](images/mainR.PNG)

Controls 

![help](images/help.PNG)

Input options, load a single .csv or a folder of .csv files.

![input](images/input.PNG)

Options for filtering and sorting pictures and annotations.

![options](images/options.PNG)

Simple but comprehensive dataset statistics.

![statistics](images/statistics.PNG)

Yolo export options.

![yolo_export](images/yolo_export.PNG)

Ollama LLM window.

![ollama](images/ollama.PNG)
