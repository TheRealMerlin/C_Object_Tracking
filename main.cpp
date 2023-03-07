#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <chrono>
#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/writer.h>
#include <parquet/exception.h>
#include <opencv2/opencv.hpp>
#include <opencv2/tracking.hpp>
#include <opencv2/core.hpp>

/*
	Current run-time against test.mov (30 s, 6.66 fps, 201 frames, 2 droplets, ~146.8 px between plates, 200 microns speration)
	is (9.32228 s w/ --show : 7.1656 s w/o --show)
*/

// Global variables
std::string PATH = "";
int NUM_DROPLETS = 0, NUM_FRAMES = 0;
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

// Store data to parquet
arrow::Status store_data(std::string &filepath , std::vector<std::vector<double>> &x, std::vector<std::vector<double>> &y) {
	// Create fields (column names) and array_vector (data)
	arrow::FieldVector fields;
	arrow::ArrayVector array_vector;

	// Store data into arrow::Table for output
	for(int i = 0; i < NUM_DROPLETS; i++) {
		// Create fields for schema (how to store the data)
		fields.push_back(arrow::field("x_" + std::to_string(i), arrow::float64()));
		fields.push_back(arrow::field("y_" + std::to_string(i), arrow::float64()));

		// Store x[i] and y[i] into arrow::Array
		arrow::DoubleBuilder dbuilder_x;
		arrow::DoubleBuilder dbuilder_y;
		std::shared_ptr<arrow::Array> x_array;
		std::shared_ptr<arrow::Array> y_array;
		ARROW_RETURN_NOT_OK(dbuilder_x.AppendValues(x[i]));
		ARROW_RETURN_NOT_OK(dbuilder_y.AppendValues(y[i]));
		ARROW_RETURN_NOT_OK(dbuilder_x.Finish(&x_array));
		ARROW_RETURN_NOT_OK(dbuilder_y.Finish(&y_array));

		// Append x and y arrays to array_vector
		array_vector.push_back(x_array);
		array_vector.push_back(y_array);
	}
	std::shared_ptr<arrow::Schema> schema = arrow::schema(fields);
	std::shared_ptr<arrow::Table> table = arrow::Table::Make(schema, array_vector);

	// Write table to output file
	std::shared_ptr<arrow::io::FileOutputStream> outfile;
	ARROW_ASSIGN_OR_RAISE(outfile, arrow::io::FileOutputStream::Open(filepath));
	ARROW_RETURN_NOT_OK(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), outfile, 3));
	return outfile->Close();
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
	NUM_FRAMES = (int)video.get(cv::CAP_PROP_FRAME_COUNT);

	// Initializes a vector of trackers and bboxes
	std::vector<cv::Ptr<cv::Tracker>> trackers;
	std::vector<cv::Rect> bboxes;
	for(int i = 0; i < NUM_DROPLETS; i++) {
		// Create tracker and bbox
		trackers.push_back(cv::TrackerCSRT::create());
		bboxes.push_back(cv::selectROI(frame, false));

		// Initialize tracker
		trackers[i]->init(frame, bboxes[i]);
	}

	// Close select ROI window
	cv::destroyAllWindows();

	// Initialize 2-D array of x and y values
	std::vector<std::vector<double>> x(NUM_DROPLETS, std::vector<double>(NUM_FRAMES, 0.0));
	std::vector<std::vector<double>> y(NUM_DROPLETS, std::vector<double>(NUM_FRAMES, 0.0));

	// Time the algorithm
	std::chrono::system_clock::time_point start_time;
	if(TIMEIT) {
		start_time = std::chrono::system_clock::now();
	}

	// Store first frame values
	for(int i = 0; i < NUM_DROPLETS; i++) {
		x[i][0] = (bboxes[i].x + bboxes[i].width / 2) * ratio;
		y[i][0] = (bboxes[i].y + bboxes[i].height / 2) * ratio;
	}

	// Tracking loop
	for(int j = 1; j < NUM_FRAMES; j++) {
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

	// Store data in paruqet file
	std::string out_file_name = std::filesystem::path(PATH).stem().string() + "_out.parquet";
	arrow::Status st = store_data(out_file_name, x, y);
	if(!st.ok()) {
		std::cerr << st << std::endl;
	}

	// Display total runtime of algorithm
	std::chrono::system_clock::time_point end_time;
	if(TIMEIT) {
		end_time = std::chrono::system_clock::now();
		std::chrono::duration<double> elapsed_seconds = end_time - start_time;
		std::cerr << "elapsed time: " << elapsed_seconds.count() << " s" << std::endl;
	}

	// Garbage collection
	video.release();
	cv::destroyAllWindows();

	return 0;
}