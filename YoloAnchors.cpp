
// code in this file was adapted from Stephane Charette's Darkmark - src-darknet/yolo_anchors.cpp
// 
// https://codeberg.org/CCodeRun/DarkMark
// https://www.ccoderun.ca/darkmark/
// DarkMark (C) 2019-2024 Stephane Charette <stephanecharette@gmail.com>


#include "pch.h"
#include "YoloAnchors.h"
#include "Useful.h"

//typedef size_t mySIZE_T;
typedef int mySIZE_T;

//#ifndef yoloanchors

#include <algorithm>
#include <fstream>
#include <limits>
#include <random>
#include <string>
#include <vector>
#include <map>
//#include "yolo_anchors.hpp"
//#include "Tools.hpp"


typedef std::vector<int>	VInt;
typedef std::vector<float>	VFloat;


struct box_label
{
	int class_idx;
	float x;
	float y;
	float w;
	float h;
};

typedef std::vector<box_label> Boxes;


struct matrix
{
	mySIZE_T rows;
	mySIZE_T cols;
	VFloat vals;

	matrix(const mySIZE_T r, const mySIZE_T c)
	{
		rows = r;
		cols = c;
		vals = VFloat(std::max<mySIZE_T>(1lu, r * c), 0.0f);
		return;
	}

	matrix() : matrix(0, 0)
	{
		return;
	}

	float & row_and_col(const mySIZE_T r, const mySIZE_T c)
	{
		if (rows == 0 || cols == 0)
		{
		//	throw std::invalid_argument("cannot get cell r=" + std::to_string(r) + " c=" + std::to_string(c) + " when rows=" + std::to_string(rows) + " and cols=" + std::to_string(cols));
			MsgBox("cannot get cell r=%s %s%s  %s%s %s%s", std::to_string(r).c_str(), " c=", std::to_string(c).c_str(), " when rows=", std::to_string(rows).c_str(), " and cols=", std::to_string(cols).c_str());
		}
		if (r >= rows || c >= cols)
		{
		//	throw std::invalid_argument("invalid cell r=" + std::to_string(r) + " c=" + std::to_string(c) + " when rows=" + std::to_string(rows) + " and cols=" + std::to_string(cols));
			MsgBox("invalid cell r=%s %s%s  %s%s %s%s", std::to_string(r).c_str(), " c=", std::to_string(c).c_str(), " when rows=", std::to_string(rows).c_str(), " and cols=", std::to_string(cols).c_str());
		}

		return vals.at(r * cols + c);
	}

	float * row(const mySIZE_T r)
	{
		return & row_and_col(r, 0);
	}
};


struct model
{
	VInt assignments;
	matrix centers;
};


float dist(float *x, float *y, int n)
{
	n = n; // stop unused warning

	float mw = (x[0] < y[0]) ? x[0] : y[0];
	float mh = (x[1] < y[1]) ? x[1] : y[1];
	float inter = mw * mh;
	float sum = x[0] * x[1] + y[0] * y[1];
	float un = sum - inter;
	float iou = inter / un;

	return 1.0f - iou;
}


VInt random_samples(const mySIZE_T maximum_number_of_samples)
{
	VInt v;
	v.reserve(maximum_number_of_samples);
	while (v.size() < maximum_number_of_samples)
	{
		v.push_back((int)v.size());
	}

//	#if 1
	//
	// If using a stand-alone version of yolo anchors, you'll have to use this
	// (or something similar) as your random number generator.
	//
	std::random_device rd;
	std::mt19937 rng(rd());
/*	#else
	auto & rng = dm::get_random_engine();
	#endif
*/
	std::shuffle(v.begin(), v.end(), rng);

	return v;
}


void random_centers(matrix & data, matrix & centers)
{
	const VInt s = random_samples(data.rows);
	for (mySIZE_T row = 0; row < centers.rows; row ++)
	{
		for (mySIZE_T col = 0; col < data.cols; col ++)
		{
			centers.row_and_col(row, col) = data.row_and_col(s[row], col);
		}
	}
	return;
}


int closest_center(float *datum, matrix & centers)
{
	int best = 0;
	float best_dist = dist(datum, centers.row(best), centers.cols);
	for (mySIZE_T j = 0; j < centers.rows; ++j)
	{
		float new_dist = dist(datum, centers.row(j), centers.cols);
		if (new_dist < best_dist)
		{
			best_dist = new_dist;
			best = (int)j;
		}
	}

	return best;
}


int kmeans_expectation(matrix & data, VInt & assignments, matrix & centers)
{
	int converged = 1;
	for (mySIZE_T i = 0; i < data.rows; ++i)
	{
		int closest = closest_center(data.row(i), centers);
		if (closest != assignments[i])
		{
			converged = 0;
		}
		assignments[i] = closest;
	}

	return converged;
}


void kmeans_maximization(matrix & data, VInt & assignments, matrix & centers)
{
	matrix old_centers(centers.rows, centers.cols);

	VInt counts(centers.rows, 0);

	for (mySIZE_T i = 0; i < centers.rows; ++i)
	{
		for (mySIZE_T j = 0; j < centers.cols; ++j)
		{
			old_centers.row_and_col(i, j) = centers.row_and_col(i, j);
			centers.row_and_col(i, j) = 0;
		}
	}

	for (mySIZE_T i = 0; i < data.rows; ++i)
	{
		++counts[assignments[i]];
		for (mySIZE_T j = 0; j < data.cols; ++j)
		{
			centers.row_and_col(assignments[i], j) += data.row_and_col(i, j);
		}
	}

	for (mySIZE_T i = 0; i < centers.rows; ++i)
	{
		if (counts[i])
		{
			for (mySIZE_T j = 0; j < centers.cols; ++j)
			{
				centers.row_and_col(i, j) /= counts[i];
			}
		}
	}

	for (mySIZE_T i = 0; i < centers.rows; ++i)
	{
		for (mySIZE_T j = 0; j < centers.cols; ++j)
		{
			if(centers.row_and_col(i, j) == 0)
			{
				centers.row_and_col(i, j) = old_centers.row_and_col(i, j);
			}
		}
	}

	return;
}


model do_kmeans(matrix & data, const mySIZE_T number_of_clusters)
{
	matrix centers(number_of_clusters, data.cols);

	VInt assignments(data.rows, 0);

	random_centers(data, centers);

	for (int i = 0; i < 1000 && !kmeans_expectation(data, assignments, centers); ++i)
//	for (int i = 0; i < 1000 and not kmeans_expectation(data, assignments, centers); ++i)
	{
		kmeans_maximization(data, assignments, centers);
	}

	model m;
	m.assignments = assignments;
	m.centers = centers;

	return m;
}


Boxes read_boxes(const std::string & filename)
{
	Boxes boxes;
	boxes.reserve(20);

	std::ifstream ifs(filename);
	while (ifs.good())
	{
		box_label box;
		ifs >> box.class_idx >> box.x >> box.y >> box.w >> box.h;
		if (ifs.eof())
		{
			// ran into EOF, meaning we didn't read all 5 entries we need, don't use this entry!
			break;
		}
		boxes.push_back(box);
	}

	return boxes;
}


int calc_anchors(FILENAMEGROUP_FILTERED *pFiltrd, 
//	const std::string & train_images_filename, 
	const mySIZE_T number_of_clusters, const mySIZE_T width, const mySIZE_T height, const mySIZE_T number_of_classes, std::string & new_anchors, std::string & new_counters_per_class, float & new_avg_iou)
{
	new_anchors				= "";
	new_counters_per_class	= "";
	new_avg_iou				= 0.0f;

	if( (width	< 32) ||
		(width	% 32) ||
		(height	< 32) ||
		(height	% 32) ||
		(number_of_clusters <= 1) )
	{
		//throw std::invalid_argument
		MsgBox("width and height must both be multiples of 32, and number_of_clusters must be greater than 1");
		return 0;
	}

	VFloat rel_width_height_array;
	VInt counter_per_class(number_of_classes, 0);
	mySIZE_T number_of_boxes = 0;
//	std::string path;
//	std::ifstream ifs(train_images_filename);

	
	// ADD ALL YOUR ANNOTATIONS IN LOOP BELOW
	// ######################################
	for(int i=0; i<pFiltrd->Num; i++){

		FILENAMEGROUP *pFnG = pFiltrd->ppFNG[i];

/*	while (std::getline(ifs, path)){
		if (path.empty())
			continue;
		// find the .txt label file that goes with this image file
		std::string labelpath = path;
		mySIZE_T pos = (mySIZE_T)labelpath.rfind(".");
		if (pos != std::string::npos)
			labelpath.erase(pos);
		labelpath += ".txt";
		*/

		box_label tempBx;
		for(int b=0; b<pFnG->NumClsfdLines; b++){
			if(pFnG->pClsfdLines[b].ClassId >= 0){
				tempBx.class_idx = pFnG->pClsfdLines[b].ClassId;
				BBOX Bx = pFnG->pClsfdLines[b].box;
				tempBx.x = (Bx.x1 + Bx.x2) / 2.0f;
				tempBx.y = (Bx.y1 + Bx.y2) / 2.0f;
				tempBx.w = Bx.x2 - Bx.x1;
				tempBx.h = Bx.y2 - Bx.y1;

				number_of_boxes ++;
				counter_per_class[tempBx.class_idx] ++;
				rel_width_height_array.push_back(tempBx.w * static_cast<float>(width));
				rel_width_height_array.push_back(tempBx.h * static_cast<float>(height));
			}
		}

	/*	for (const auto & box : read_boxes(labelpath))
		{
			number_of_boxes ++;
			counter_per_class[box.class_idx] ++;
			rel_width_height_array.push_back(box.w * static_cast<float>(width));
			rel_width_height_array.push_back(box.h * static_cast<float>(height));
		}*/
	}
	if(number_of_boxes <= number_of_clusters){
		MsgBox2("error - number_of_boxes < number_of_clusters\r\n\r\n not enough boxes to calculate anchors");
		return 0;
	}
	matrix boxes_data(number_of_boxes, 2);
	for (mySIZE_T i = 0; i < number_of_boxes; ++i)
	{
		boxes_data.row_and_col(i, 0) = rel_width_height_array[i * 2 + 0];
		boxes_data.row_and_col(i, 1) = rel_width_height_array[i * 2 + 1];
	}
	// K-means
	model anchors_data = do_kmeans(boxes_data, number_of_clusters);

	// Store all the sizes in a multimap.  The key is the total area, and the value is the width+height stored as strings.
	// This way the multimap will automatically sort all the values for us from smallest to largest, and all that we need
	// to do is create the final string from all the values.
	std::multimap<float, std::string> mm;
	for (mySIZE_T row = 0; row < number_of_clusters; row ++)
	{
		const float w				= anchors_data.centers.row_and_col(row, 0);
		const float h				= anchors_data.centers.row_and_col(row, 1);
		const float area			= w * h;
		const mySIZE_T round_width	= (mySIZE_T)std::round(w);
		const mySIZE_T round_height	= (mySIZE_T)std::round(h);
		const std::string text	= std::to_string(round_width) + ", " + std::to_string(round_height);
		mm.insert(std::make_pair(area, text));
	}
	for (auto [key, val] : mm)
	{
		(void)key; // silence "unused variable" warning on older compilers (Ubuntu 18.04 and g++ 7.5.0)
		if (!new_anchors.empty())
			new_anchors += ", ";
		new_anchors += val;
	}

	// now we figure out the values for counters_per_class
	float avg_iou = 0.0f;
	for (mySIZE_T i = 0; i < number_of_boxes; ++i)
	{
		float box_w		= rel_width_height_array[i * 2 + 0];
		float box_h		= rel_width_height_array[i * 2 + 1];
		float min_dist	= 1000000.0f;//std::numeric_limits<float>::max();
		float best_iou	= 0.0f;
		for (mySIZE_T j = 0; j < number_of_clusters; ++j)
		{
			const float anchor_w		= anchors_data.centers.row_and_col(j, 0);
			const float anchor_h		= anchors_data.centers.row_and_col(j, 1);
			const float min_w			= (box_w < anchor_w) ? box_w : anchor_w;
			const float min_h			= (box_h < anchor_h) ? box_h : anchor_h;
			const float box_intersect	= min_w * min_h;
			const float box_union		= box_w * box_h + anchor_w * anchor_h - box_intersect;
			const float iou				= box_intersect / box_union;
			const float distance		= 1 - iou;
			if (distance < min_dist)
			{
				min_dist = distance;
				best_iou = iou;
			}
		}

		if (best_iou > 0.0f && best_iou < 1.0f)
			avg_iou += best_iou;
	}

	for (mySIZE_T i = 0; i < number_of_classes; i++)
	{
		if (i > 0)
			new_counters_per_class += ", ";
		new_counters_per_class += std::to_string(counter_per_class[i]);
	}

	new_avg_iou = 100.0f * avg_iou / static_cast<float>(number_of_boxes);
	return 1;
}

int CalculateYoloAnchors(CWnd *pCWnd, FILENAMEGROUP_FILTERED *pFiltrd, float TargetWd, float TargetHt)
{
	std::string new_anchors;
	std::string counters_per_class;
	std::string max_anchors;
	float max_iou = 0.0f;

	int Ok = 1, Num = 100;
	for(int i=0; i<Num && Ok; i++){
		float avg_iou = 0.0f;
		Ok = calc_anchors(pFiltrd, 9, (mySIZE_T)TargetWd, (mySIZE_T)TargetHt, g_CIdM.m_NumClasses, new_anchors, counters_per_class, avg_iou);
		if(Ok){
			if(avg_iou > max_iou){
				// highest iou is the best anchors
				max_iou = avg_iou;
				max_anchors = new_anchors;
			}
			ProgressUpdate_SetWindowText(pCWnd, "calculate anchors", i+1, Num);
		}
	}
	if(Ok)
		MsgBox2("yolo anchors\r\n\r\n counters_per_class\r\n%s\r\n\r\n best anchors (%d attempts)\r\n%s\r\n\r\n iou\r\n%f", counters_per_class.c_str(), Num, max_anchors.c_str(), max_iou);
	return 0;	
}

/*
	float best_avg_iou = 0.0f;

	for (size_t attempt = 0; attempt < max_attempts and std::time(nullptr) < end_time; attempt ++)
	{
		progress_window.setProgress(double(attempt) / double(max_attempts));

		std::string counters_per_class;
		std::string anchors;

		float avg_iou = 0.0f;

		calc_anchors(info.train_filename, anchor_clusters, info.image_width, info.image_height, number_of_classes, anchors, counters_per_class, avg_iou);
		if (avg_iou > best_avg_iou)
		{
			dm::Log("attempt #" + std::to_string(attempt) + ": avg IoU ........ " + std::to_string(avg_iou));
			dm::Log("attempt #" + std::to_string(attempt) + ": new anchors .... " + anchors);
			dm::Log("attempt #" + std::to_string(attempt) + ": new counters ... " + counters_per_class);

			best_avg_iou = avg_iou;
			m["anchors"] = anchors;

			if (class_imbalance)
			{
				m["counters_per_class"] = counters_per_class;
			}
		}

*/