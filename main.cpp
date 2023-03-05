#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/writer.h>
#include <parquet/exception.h>
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
		} else if(strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--timeit") == 0) {
			TIMEIT = true;
		} else if(strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--show") == 0) {
			SHOW = true;
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

	// Calculates ratio (microns : px)
	double ratio = DISTANCE / PX_DISTANCE;

	// Opens video and reads into frame
	cv::VideoCapture video(PATH);
	cv::Mat frame;
	bool ok = video.read(frame);
	if(!video.isOpened()) {
		std::cerr << "Could not open video" << std::endl;
		return -1;
	}

	// Gets number of frames in video
	int num_frames = (int)video.get(cv::CAP_PROP_FRAME_COUNT);

	// Creates a vector of trackers and bboxes
	std::vector<cv::Ptr<cv::Tracker>> trackers;
	std::vector<cv::Rect> bboxes;
	for(int i = 0; i < NUM_DROPLETS; i++) {
		trackers.push_back(cv::TrackerCSRT::create());
		bboxes.push_back(cv::selectROI(frame, false));
	}

	// Close select ROI window
	cv::destroyAllWindows();

	// Initialize trackers w/ respective bboxes
	for(int i = 0; i < NUM_DROPLETS; i++) {
		trackers[i]->init(frame, bboxes[i]);
	}

	// Initialize 2-D array of x and y values
	std::vector<std::vector<double>> x(NUM_DROPLETS, std::vector<double>(num_frames, 0.0));
	std::vector<std::vector<double>> y(NUM_DROPLETS, std::vector<double>(num_frames, 0.0));

	// Store first frame values
	for(int i = 0; i < NUM_DROPLETS; i++) {
		x[i][0] = (bboxes[i].x + bboxes[i].width / 2) * ratio;
		y[i][0] = (bboxes[i].y + bboxes[i].height / 2) * ratio;
	}

	// Tracking loop
	for(int j = 1; j < num_frames; j++) {
		// Read next frame
		video.read(frame);
		
		// Update each tracker
		for(int i = 0; i < NUM_DROPLETS; i++) {
			ok = trackers[i]->update(frame, bboxes[i]);
			if(ok) {
				// Tracking success
				x[i][j] = (bboxes[i].x + bboxes[i].width / 2) * ratio;
				y[i][j] = (bboxes[i].y + bboxes[i].height / 2) * ratio;

				// Draw rectangle on frame if displaying trackers
				if(SHOW) {
					cv::rectangle(frame, bboxes[i], cv::Scalar(255, 0, 0), 2, 1);
				}
			} else {
				// Tracking failure
				std::cerr << "Tracking Failure Detected!\tDroplet: " << i + 1 << "\tFrame: " << j << std::endl;
			}
		}
		
		// Update tracking display window
		if(SHOW) {
			cv::imshow("Tracking", frame);
			int k = cv::waitKey(1);
			if(k == 27) { break; }
		}
	}

	// Create schema for output file
	arrow::FieldVector fields;
	for(int i = 0; i < NUM_DROPLETS; i++) {
		fields.push_back(arrow::field("x_" + std::to_string(i), arrow::float64()));
		fields.push_back(arrow::field("y_" + std::to_string(i), arrow::float64()));
	}
	std::shared_ptr<arrow::Schema> schema = arrow::schema(fields);

	// Store data into arrow::Table for output
	arrow::ArrayVector array_vector;
	for(int i = 0; i < NUM_DROPLETS; i++) {
		arrow::DoubleBuilder dbuilder;
		dbuilder.AppendValues(x[i]);
		std::shared_ptr<arrow::DoubleArray> data_array;
		dbuilder.Finish(&data_array);
		array_vector.push_back(data_array);
	}
	std::cerr << array_vector.at(0) << std::endl;
	std::shared_ptr<arrow::Table> table = arrow::Table::Make(schema, array_vector);

	// Write table to output file
	std::shared_ptr<arrow::io::FileOutputStream> outfile;
	PARQUET_THROW_NOT_OK(arrow::io::FileOutputStream::Open("test.parquet", &outfile));
	PARQUET_THROW_NOT_OK(parquet::arrow::WriteTable(*table.get(), arrow::default_memory_pool(), outfile, 3));

	// Garbage collection
	outfile->Close();
	video.release();
	cv::destroyAllWindows();

	return 0;
}