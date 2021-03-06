#include "Transfer.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#pragma warning(disable: 4244)

img_trans::img_trans(std::string fname)
{
	SetImg(fname);
}
img_trans::img_trans(cv::Mat image)
{
	SetImg(image);
}
img_trans::img_trans()
{}
void img_trans::SetImg(std::string fname)
{
	file_name = fname;
	img = cv::imread(file_name);
	if(!img.channels())
		return;
	SetWeight();
	GetImageParams(BGRtoLab(img), mean, stdd);
}
void img_trans::SetImg(cv::Mat image)
{
	img = image.clone();
	SetWeight();
	GetImageParams(BGRtoLab(img), mean, stdd);
}
void img_trans::SetWeight(double weight)
{
	if(!img.channels())
		return;
	if(channel_w.size() < IT_CHANNELS)
		channel_w.resize(IT_CHANNELS);
	channel_w[0] = weight;
	channel_w[1] = weight;
	channel_w[2] = weight;
}
cv::Mat CTW(img_trans& source, std::vector<img_trans> layers)
{
	/*
		Sum of all layers and input wights
		per same channel prefer to be 100.
		It's not limit, it can be 100 in input
		and sum 100 in all layers, but result
		percentage then will be 50% for input 
		and 50% for all layers (respectivly
		to their weights). Negative == 0
	*/

	std::vector<double> divider;
	divider.resize(IT_CHANNELS);
	for(unsigned i = 0; i < IT_CHANNELS; i++)
	{
		divider[i] = 0;
		if(source.channel_w[i] < 0)
			continue;
		divider[i] += source.channel_w[i];
	}
	for(auto& layer :  layers)
	{
		for(unsigned i = 0; i < IT_CHANNELS; i++)
		{
			if(layer.channel_w[i] < 0)
				continue;
			divider[i] += layer.channel_w[i];
		}
	}
	std::vector<cv::Mat> res_channels, lab_channels;
	cv::split(source.img, res_channels); // source.img already in lab
	cv::split(source.img, lab_channels);
	// init
	for(unsigned i = 0; i < IT_CHANNELS; i++)
		res_channels[i] *= source.channel_w[i]/divider[i];
	// add
	for(auto& layer :  layers)
	{	for(unsigned i = 0; i < IT_CHANNELS; i++)
		{
			double layer_weight = layer.channel_w[i]/divider[i];
			double std_koef = layer.stdd[i]/source.stdd[i];
			res_channels[i] += ((lab_channels[i] - source.mean[i]) * std_koef	 + layer.mean[i]) * layer_weight;
		}
	}
	cv::Mat res;
	cv::merge(res_channels, res);
	return LabtoBGR(res);
}
void GetImageParams(cv::Mat& img, std::vector<double>& mean, std::vector<double>& stdd)
{
	cv::Mat _mean, _stdd;
	cv::meanStdDev(img, _mean, _stdd);
	mean.resize(IT_CHANNELS);
	stdd.resize(IT_CHANNELS);
	for(unsigned i = 0; i < IT_CHANNELS; i++)
	{
		mean[i] = _mean.at<double>(i);
		stdd[i] = _stdd.at<double>(i);
	}
}

cv::Mat RGB_to_LMS = (cv::Mat_<float>(3,3) <<	0.3811f, 0.5783f, 0.0402f,
												0.1967f, 0.7244f, 0.0782f,
												0.0241f, 0.1288f, 0.8444f);


cv::Mat LMS_to_RGB = (cv::Mat_<float>(3,3) <<	4.4679f, -3.5873f, 0.1193f,
												-1.2186f, 2.3809f, -0.1624f,
												0.0497f, -0.2439f, 1.2045f);
float _x = 1/sqrt(3), _y = 1/sqrt(6), _z = 1/sqrt(2);
cv::Mat LMS_to_lab = (cv::Mat_<float>(3,3) <<	_x, _x, _x,
												_y, _y, -2*_y,
												_z, -_z, 0);
cv::Mat lab_to_LMS = (cv::Mat_<float>(3,3) <<	_x, _y, _z,
												_x, _y, -_z,
												_x, -2*_y, 0);

cv::Mat BGRtoLab(cv::Mat input)
{
	cv::Mat img_RGB;
	cv::cvtColor(input, img_RGB, CV_BGR2RGB);
	cv::Mat min_mat = cv::Mat::Mat(img_RGB.size(), CV_8UC3, cv::Scalar(1, 1, 1));
	cv::max(img_RGB, min_mat, img_RGB);
	img_RGB.convertTo(img_RGB, CV_32FC1, 1/255.f);
	cv::Mat img_lms;
	cv::transform(img_RGB, img_lms, RGB_to_LMS);
	cv::log(img_lms,img_lms);
	img_lms /= log(10);
	cv::Mat img_lab;
	cv::transform(img_lms, img_lab, LMS_to_lab);
	return img_lab;
}
cv::Mat LabtoBGR(cv::Mat input)
{
	cv::Mat img_lms;
	cv::transform(input, img_lms, lab_to_LMS);
	cv::exp(img_lms,img_lms);
	cv::pow(img_lms,log(10),img_lms);
	cv::Mat img_RGB;
	cv::transform(img_lms, img_RGB, LMS_to_RGB);
	img_RGB.convertTo(img_RGB, CV_8UC1, 255.f);
	cv::Mat img_BGR;
	cv::cvtColor(img_RGB, img_BGR, CV_RGB2BGR);
	return img_BGR;
}