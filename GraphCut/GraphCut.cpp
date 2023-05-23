#include "GraphCut.h"


GraphCut::GraphCut(string inputpath, string outputpath) {
	inputDir = inputpath;
	outputDir = outputpath;
}

void GraphCut::startStitching() {

	//create result folder
	cv::utils::fs::createDirectory(outputDir);

	string panoramaDir = cv::utils::fs::join(outputDir, "panorama");
	cv::utils::fs::createDirectory(panoramaDir);
	
	if (Utils::isDebug) {
		debugDir = cv::utils::fs::join(outputDir, "debug");
		cv::utils::fs::createDirectory(debugDir);
	}

	cout << inputDir << endl;
	//list all frame name
	for (const auto& entry : fs::directory_iterator(inputDir)) {
		string frame_name = entry.path().filename().string();
		string imagesDir = entry.path().string();

		if (Utils::isDebug) {
			string debugFrameDir = cv::utils::fs::join(debugDir, frame_name);
			cv::utils::fs::createDirectory(debugFrameDir);
			Utils::debugPath = debugFrameDir;
		}
	
		
		//Start stitching image
		Mat result = imagesController.stitchingImages(imagesDir);
		//write panorama result
		string resultPath = cv::utils::fs::join(panoramaDir, frame_name);
		imwrite(resultPath + ".png", result);

		
		imagesController.currentFrameindex++;
	}
}
