#include "ImagesController.h"

void ImagesController::readImages(string imgdir) {
	int index = 0;

	for (const auto& entry : fs::directory_iterator(imgdir)) {
		if (entry.path().extension() == ".png") {

			Mat img = imread(entry.path().string(), CV_LOAD_IMAGE_UNCHANGED);
			ImgData imgData;
			imgData.img = img;

			int w = imgData.img.size().width;
			int h = imgData.img.size().height;

			resize(imgData.img, imgData.imgScaled, Size(w / scalefactor, h / scalefactor), 0, 0, INTER_LINEAR);

			//find gradient of image
			Mat sourceGray, sinkGray;
			cv::cvtColor(imgData.imgScaled, sourceGray, COLOR_RGB2GRAY, 0);
			Laplacian(sourceGray, imgData.gradient, CV_8U, 1, 1, 0, BORDER_DEFAULT);

			if (Utils::isDebug) {
				string srcgradientpath = Utils::debugPath + "/sourceGradient" + to_string(index) + ".png";
				cv::imwrite(srcgradientpath, imgData.gradient);

			}

			index++;
			images.push_back(imgData);
		}
	}
}

//find overlap Region
void ImagesController::findOverlapRegion() {
	for (int i = 1; i < images.size(); i++) {
		Overlap roi(images[i - 1].img, images[i].img, i - 1, i);
		overlapROI.push_back(roi);
	}
}

//calculate energy function from overlap region
int ImagesController::edgeEnergy(Point2d s, Point2d t) {

	int w = images[imgindex].imgScaled.size().width;
	int h = images[imgindex].imgScaled.size().height;


	Vec4b& colorA_s = images[imgindex].imgScaled.at<Vec4b>(s.x, s.y);
	Vec4b& colorB_s = images[imgindex + 1].imgScaled.at<Vec4b>(s.x, s.y);

	Vec4b& colorA_t = images[imgindex].imgScaled.at<Vec4b>(t.x, t.y);
	Vec4b& colorB_t = images[imgindex + 1].imgScaled.at<Vec4b>(t.x, t.y);


	int gradientA_s = images[imgindex].imgScaled.at<Vec4b>(s.x, s.y)[3] ? images[imgindex].gradient.at<uchar>(s.x, s.y) : 0;
	int gradientB_s = images[imgindex + 1].imgScaled.at<Vec4b>(s.x, s.y)[3] ? images[imgindex + 1].gradient.at<uchar>(s.x, s.y) : 0;

	int gradientA_t = images[imgindex].imgScaled.at<Vec4b>(t.x, t.y)[3] ? images[imgindex].gradient.at<uchar>(t.x, t.y) : 0;
	int gradientB_t = images[imgindex + 1].imgScaled.at<Vec4b>(t.x, t.y)[3] ? images[imgindex + 1].gradient.at<uchar>(t.x, t.y) : 0;

	int cr = abs(colorA_s[0] - colorB_s[0]) + abs(colorA_t[0] - colorB_t[0]);
	int cg = abs(colorA_s[1] - colorB_s[1]) + abs(colorA_t[1] - colorB_t[1]);
	int cb = abs(colorA_s[2] - colorB_s[2]) + abs(colorA_t[2] - colorB_t[2]);

	int energy;
	int gradientcost = abs(gradientA_s - gradientB_s) + abs(gradientA_t - gradientB_t);
	int rgbcost = (cr + cg + cb) / 3;
	energy = 0.5 * rgbcost + 0.5 * gradientcost;

	
	if (currentFrameindex > 0) {
		int idx = (s.x) * w + (s.y);
		int idx2 = (t.x) * w + (t.y);
	}

	return energy;
}

void ImagesController::buildGraph() {

	int w = sourceData.img.size().width;
	int h = sourceData.img.size().height;

	int rescale_w = sourceData.img.size().width / scalefactor;
	int rescale_h = sourceData.img.size().height / scalefactor;

	Mat overlapImg = overlapROI[imgindex].overlapImg;
	Mat overlapBoundary = overlapROI[imgindex].overlapBoundary;

	// calculate node count
	int nodes_count = 0;
	for (int i = 0; i < rescale_h; i++) {
		for (int j = 0; j < rescale_w; j++) {
			int index = i * rescale_w + j;
			if (overlapImg.at <uchar>(i * scalefactor, j * scalefactor)) {

				pixelIndex2nodeIndex[index] = nodes_count++;
			}
		}
	}
	//calculate edge count
	int edge_count = 0;
	for (int i = 0; i < rescale_h; i++) {
		for (int j = 0; j < rescale_w; j++) {
			int index = i * rescale_w + j;
			if (pixelIndex2nodeIndex.find(index) != pixelIndex2nodeIndex.end()) {
				int leftIndex = i * rescale_w + (j - 1);
				if (pixelIndex2nodeIndex.find(leftIndex) != pixelIndex2nodeIndex.end()) {
					edge_count++;
				}
				int upIndex = (i - 1) * rescale_w + j;
				if (pixelIndex2nodeIndex.find(upIndex) != pixelIndex2nodeIndex.end()) {
					edge_count++;
				}
			}
		}
	}
	//fill out graph
	G = new Graph_III(nodes_count, edge_count);

	if (currentFrameindex == 0) {
		overlapROI[imgindex].extractDataTerm();
	}


	Mat dataterm = overlapROI[imgindex].dataTermMap;
	string datatermpath = Utils::debugPath + "/dataTerm" + to_string(Utils::sourceImgindex) + "_" + to_string(Utils::sinkImgindex) + ".jpg";
	if (Utils::isDebug) {
		cv::imwrite(datatermpath, dataterm);
	}

	std::map<std::pair<int, int>, int> currentEdgeEnergy;


	for (int i = 0; i < rescale_h; i++) {
		for (int j = 0; j < rescale_w; j++) {
			int index = i * rescale_w + j;
			if (overlapImg.at <uchar>(i * scalefactor, j * scalefactor)) {
				G->add_node();
				Point2d p_node(i, j);
				int nodeIndex = pixelIndex2nodeIndex.find(index)->second;

				if (dataterm.at<Vec3b>(i * scalefactor, j * scalefactor) == Vec3b(255, 0, 0)) {
					G->add_tweights(nodeIndex, INT_MAX, 0);
				}
				else if (dataterm.at<Vec3b>(i * scalefactor, j * scalefactor) == Vec3b(0, 255, 0)) {
					G->add_tweights(nodeIndex, 0, INT_MAX);
				}


				// left edge weight
				int leftIndex = i * rescale_w + (j - 1);
				Point2d p_left(i, (j - 1));
				auto leftNodeMap = pixelIndex2nodeIndex.find(leftIndex);

				if (leftNodeMap != pixelIndex2nodeIndex.end()) {
					int e = edgeEnergy(p_node, p_left);
					G->add_edge(nodeIndex, leftNodeMap->second, e, e);
					currentEdgeEnergy[std::make_pair(index, leftIndex)] = e;
				}

				// up edge weight
				int upIndex = (i - 1) * rescale_w + j;
				Point2d p_up((i - 1), j);
				auto upNodeMap = pixelIndex2nodeIndex.find(upIndex);
				if (upNodeMap != pixelIndex2nodeIndex.end()) {
					int e = edgeEnergy(p_node, p_up);
					G->add_edge(nodeIndex, upNodeMap->second, e, e);
					currentEdgeEnergy[std::make_pair(index, upIndex)] = e;
				}


			}
		}
	}

	if (currentFrameindex == 0) {
		previousEnergy.push_back(currentEdgeEnergy);
	}
	else {
		previousEnergy[imgindex] = currentEdgeEnergy;
	}
	currentEdgeEnergy.clear();

}

Mat ImagesController::textureMapping() {
	int w = sourceData.img.size().width;
	int h = sourceData.img.size().height;

	int rescale_w = sourceData.img.size().width / scalefactor;
	int rescale_h = sourceData.img.size().height / scalefactor;

	label = Mat(h, w, CV_8UC3, Scalar(0, 0, 0));
	Mat image(h, w, CV_8UC4, Scalar(0, 0, 0, 0));


	Mat mask_src(h, w, CV_8UC1);
	Mat mask_sink(h, w, CV_8UC1);

	Mat src_bgr;
	Mat sink_bgr;

	cv::cvtColor(sourceData.img, src_bgr, CV_BGRA2BGR);
	cv::cvtColor(sinkData.img, sink_bgr, CV_BGRA2BGR);

	//apply multiband blending to stitch images
	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			int index = (y / scalefactor) * (w / scalefactor) + (x / scalefactor);
			auto nodeMap = pixelIndex2nodeIndex.find(index);

			bool check = true;

			int node_seg = G->what_segment(nodeMap->second);
			int cur_seg = -1;

			//draw label	
			if (sourceData.img.at < Vec4b >(y, x)[3] != 0) {
				if (overlapROI[imgindex].overlapImg.at<uchar>(y, x) == 0)
					mask_src.at< uchar >(y, x) = 255;
				else if (x < overlapROI[imgindex].overlapCenter) 
					mask_src.at< uchar >(y, x) = 255;				
			}
			if (sinkData.img.at < Vec4b >(y, x)[3] != 0) {
				if (overlapROI[imgindex].overlapImg.at<uchar>(y, x) == 0)
					mask_sink.at< uchar >(y, x) = 255;
				else if (x >= overlapROI[imgindex].overlapCenter)
					mask_sink.at< uchar >(y, x) = 255;
			}

			if (nodeMap != pixelIndex2nodeIndex.end()) {
				if (node_seg == Graph_III::SOURCE) {
					mask_src.at< uchar >(y, x) = 255;
					mask_sink.at< uchar >(y, x) = 0;
					label.at < Vec3b >(y, x) = Vec3b(255, 0, 0);
				}
				else if (node_seg == Graph_III::SINK) {
					mask_sink.at< uchar >(y, x) = 255;
					mask_src.at< uchar >(y, x) = 0;
					label.at < Vec3b >(y, x) = Vec3b(0, 255, 0);
				}
			}
		}
	}

	if (Utils::isDebug) {
		string labelpath = Utils::debugPath + "/label" + to_string(Utils::sourceImgindex) + "_" + to_string(Utils::sinkImgindex) + ".png";
		cv::imwrite(labelpath, label);
	}

	detail::MultiBandBlender blender(false, 3);

	blender.prepare(Rect(0, 0, w, h));
	blender.feed(src_bgr, mask_src, Point2f(0, 0));
	blender.feed(sink_bgr, mask_sink, Point2f(0, 0));
	Mat result, result_mask;
	Mat deg, deg2;

	src_bgr.copyTo(deg, mask_src);

	sink_bgr.copyTo(deg2, mask_sink);
	blender.blend(result, result_mask);

	Mat result_notmask;
	cv::bitwise_not(result_mask, result_notmask);

	sourceData.img.copyTo(image, mask_src);
	sinkData.img.copyTo(image, mask_sink);

	result.convertTo(result, CV_8UC3);
	cv::cvtColor(result, result, CV_BGR2BGRA, 4);
		
	result.copyTo(image, overlapROI[imgindex].overlapImg);
	int overlapCenter = overlapROI[imgindex].overlapCenter;

		
	for (int i = 0; i < h; i++) {
		for (int j = 0; j < w; j++) {
			if (result_mask.at<uchar>(i, j) == 255) {
				int gray1 = 0.299 * sourceData.img.at<Vec4b>(i, j)[2] + 0.587 * sourceData.img.at<Vec4b>(i, j)[1] + 0.114 * sourceData.img.at<Vec4b>(i, j)[0];
				int gray2 = 0.299 * sinkData.img.at<Vec4b>(i, j)[2] + 0.587 * sinkData.img.at<Vec4b>(i, j)[1] + 0.114 * sinkData.img.at<Vec4b>(i, j)[0];
				if (gray2 - gray1 > 0&&gray1<10) {
					image.at<Vec4b>(i, j) = sinkData.img.at<Vec4b>(i, j);
				}
				else if (gray1 - gray2 > 0 && gray2 <10 ) {
					image.at<Vec4b>(i, j) = sourceData.img.at<Vec4b>(i, j);
				}		
			}
				
		}
	}

	if (Utils::isDebug) {
		//output debug images 
		//draw seam on result
		if (imgindex == 0) {
			image.copyTo(seamResult);
		}
		else {
			sinkData.img.copyTo(seamResult, mask_sink);
		}
		for (int y = 0; y < h; y++) {
			for (int x = 0; x < w; x++) {
				Vec3b color = label.at<Vec3b>(y, x);


				for (int wy = 0; wy < 4; wy++) {
					for (int wx = 0; wx < 4; wx++) {

						if (color[0] != 0 && color[1] != 0 && color[2] != 0 && color[3] != 0) {
							continue;
						}
						else if (x + wx < w && y + wy < h) {

							Vec3b othercolor = label.at<Vec3b>(y + wy, x + wx);
							if (color == Vec3b(255, 0, 0) && othercolor == Vec3b(0, 255, 0)) {
								seamResult.at<Vec4b>(y, x) = Vec4b(0,0,255,255);
								break;
							}
							else if (color == Vec3b(0, 255, 0) && othercolor == Vec3b(255, 0, 0)) {
								seamResult.at<Vec4b>(y, x) = Vec4b(0, 0, 255, 255);
								break;
							}
						}
					}
				}
			}
		}

		string seam_path = Utils::debugPath + "/seamresult.png";
		imwrite(seam_path, seamResult);

		string src_mask = Utils::debugPath + "/src_mask" + to_string(Utils::sourceImgindex) + "_" + to_string(Utils::sinkImgindex) + ".png";
		cv::imwrite(src_mask, mask_src);

		string sink_mask = Utils::debugPath + "/sink_mask" + to_string(Utils::sourceImgindex) + "_" + to_string(Utils::sinkImgindex) + ".png";
		cv::imwrite(sink_mask, mask_sink);

		string rgbp = Utils::debugPath + "/rgb" + to_string(Utils::sourceImgindex) + "_" + to_string(Utils::sinkImgindex) + ".png";
		cv::imwrite(rgbp, deg);

		string rgb2p = Utils::debugPath + "/rgb2" + to_string(Utils::sourceImgindex) + "_" + to_string(Utils::sinkImgindex) + ".png";
		cv::imwrite(rgb2p, deg2);

		string res = Utils::debugPath + "/res" + to_string(Utils::sourceImgindex) + "_" + to_string(Utils::sinkImgindex) + ".png";
		cv::imwrite(res, image);

		string labelpath = Utils::debugPath + "/label" + to_string(Utils::sourceImgindex) + "_" + to_string(Utils::sinkImgindex) + ".png";
		cv::imwrite(labelpath, label);

		labelpath = Utils::debugPath + "/" + to_string(Utils::sourceImgindex) + "_" + to_string(Utils::sinkImgindex) + "label.png";
		cv::imwrite(labelpath, label);

	}

	return image;
	
}

Mat ImagesController::stitchImage(Mat source, Mat sink) {

	//initial source sink img;
	int w = source.size().width;
	int h = source.size().height;

	sourceData.img = source;
	sinkData.img = sink;


	if (w != sink.size().width || h != sink.size().height) {
		std::printf("Input Images Resolution Not Matched");
		return {};
	}

	overlapROI[imgindex];

	cout << "build graph" << endl;
	buildGraph();

	// solve min-cut
	int flow = G->maxflow();
	std::printf("Flow = %d\n", flow);

	// output Result label,image,(image with cut seam)
	Mat nodeType(h, w, CV_8UC4, Scalar(0, 0, 0));
	Mat result = textureMapping();
	//overlapROI[imgindex].updateROI(label);
	pixelIndex2nodeIndex.clear();
	G->reset();
	delete G;
	return result;
}


Mat ImagesController::stitchingImages(string imagesDir) {

	//load all images path in current frame
	readImages(imagesDir);
	cout << "\nTotal images: " << images.size() << endl;

	//extract overlapRegion

	if (currentFrameindex == 0) {
		cout << "find overlap" << endl;
		findOverlapRegion();
	}
	else {
		cout << "no need find overlap" << endl;
	}

	auto start = high_resolution_clock::now();
	Mat source = images[0].img;
	Utils::sourceImgindex = 0;

	for (int i = 1; i < images.size(); i++) {
		imgindex = i - 1;
		int imgindex = i - 1;
		Utils::sinkImgindex = i;
		cout << "index :" << i << endl;

		Mat sink = images[i].img;
		stitchResult = stitchImage(source, sink);
		source = stitchResult;
		Utils::sourceImgindex = i;
		previousImg.assign(images.begin(), images.end());

	}
	auto stop = high_resolution_clock::now();
	auto duration = duration_cast<milliseconds>(stop - start);
	auto secs = std::chrono::duration_cast<std::chrono::duration<float>>(duration);
	cout << "Total Execution Time: " << secs.count() << "s" << endl << endl;
	images.clear();

	return stitchResult;
}