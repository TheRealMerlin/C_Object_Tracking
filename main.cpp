#include <iostream>
#include <fstream>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/core.hpp>

// Global variables
std::string PATH = "";
int NUM_DROPLETS = 0;
double PX_DISTANCE = 0.0, DISTANCE = 0.0;
bool TIMEIT = false, SHOW = false;

// Displays help for program
void display_help(char** argv) {
	std::cerr << "usage: " << argv[0] << " FILEPATH" << " [-h]" << " -n DROPLETS" << " -px PIXELS" << " -d DISTANCE" << " [-t]" << " [-s]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "positional arguments:" << std::endl;
	std::cerr << " FILEPATH\t\tpath to video file (e.g. .mov, .mp4, etc.)" << std::endl;
	std::cerr << std::endl;
	std::cerr << "required arguments:" << std::endl;
	std::cerr << " -n DROPLETS, --droplets DROPLETS\n\t\t\tnumber of droplets to track" << std::endl;
	std::cerr << " -px PIXELS, --pixels PIXELS\n\t\t\tdistance between plates in pixels" << std::endl;
	std::cerr << " -d DISTANCE, --distance DISTANCE\n\t\t\tdistance between plates in microns" << std::endl;
	std::cerr << "options:" << std::endl;
	std::cerr << " -h, --help\t\tshow this help message and exit" << std::endl;
	std::cerr << " -t, --timeit\t\tprints run-time of tracking algorithm" << std::endl;
	std::cerr << " -s, --show\t\tdisplays video with trackers" << std::endl;
}

// Gets necessary arguments from command line
int parse_args(int argc, char** argv) {
	for(int i = 1; i < argc; i++) {
		if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			display_help(argv);
			return 1;
		} else if(strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--droplets") == 0) {
			if(i + 1 < argc) {
				NUM_DROPLETS = std::strtol(argv[++i], NULL, 10);
			} else {
				std::cerr << "-n DROPLETS option requires one argument" << std::endl;
				return 1;
			}
		} else if(strcmp(argv[i], "-px") == 0 || strcmp(argv[i], "--pixels") == 0) {
			if(i + 1 < argc) {
				PX_DISTANCE = std::strtod(argv[++i], NULL);
			} else {
				std::cerr << "-px PIXELS option requires one argument" << std::endl;
				return 1;
			}
		} else if(strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--distance") == 0) {
			if(i + 1 < argc) {
				DISTANCE = std::strtod(argv[++i], NULL);
			} else {
				std::cerr << "-d DISTANCE option requires one argument" << std::endl;
				return 1;
			}
		} else if(PATH.compare("") == 0) {
			std::ifstream test(argv[i]);
			if(!test) {
				std::cerr << argv[i] << " does not exist or is not a file" << std::endl;
				return -1;
			} else {
				PATH = argv[i];
			}
		} else {
			std::cerr << "unkown argument: " << argv[i] << std::endl;
			display_help(argv);
			return 1;
		}
	}
	if(PATH.compare("") == 0) {
		std::cerr << "requires FILEPATH" << std::endl;
		return 1;
	}
	if(NUM_DROPLETS == 0) {
		std::cerr << "requires -n DROPLETS" << std::endl;
		return 1;
	}
	if(PX_DISTANCE == 0.0) {
		std::cerr << "requires -px PIXELS" << std::endl;
		return 1;
	}
	if(DISTANCE == 0.0) {
		std::cerr << "requires -d DISTANCE" << std::endl;
		return 1;
	}
	return 0;
}

// Program entry
int main(int argc, char** argv) {
	// Parse arguements and ends program if error
	if(parse_args(argc, argv) == 1) {
		return 1;
	}


	cv::Ptr<cv::Tracker> tracker = cv::TrackerCSRT::create();

	cv::VideoCapture video(PATH);

	cv::Mat frame;
	bool ok = video.read(frame);
	if(!video.isOpened()) {
		std::cerr << "Could not open video" << std::endl;
		return -1;
	}

	int num_frames = (int)video.get(cv::CAP_PROP_FRAME_COUNT);

	cv::Rect bbox = cv::selectROI(frame, false);
	cv::imshow("Tracking", frame);
	tracker->init(frame, bbox);

	for(int j = 0; j < num_frames; j++) {
		bool ok = tracker->update(frame, bbox);
		if(ok) {
			cv::rectangle(frame, bbox, cv::Scalar(255, 0, 0), 2, 1);
		}else {
			std::cerr << "Tracking Failure Detected!" << std::endl;
		}

		cv::imshow("Tracking", frame);

		int k = cv::waitKey(1);
		if(k == 27) { break; }
	}

	return 0;
}