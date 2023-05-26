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
double PX_DISTANCE = 0.0, DISTANCE = 0.0, FPS = 0.0, DENSITY = 1000.0;
std::vector<double> DIAMETERS;
bool TIMEIT = false, SHOW = false;

// Displays help for program
void display_help(char** argv) {
	std::cerr << "usage: " << argv[0] << " FILEPATH" << " [-h]" << " -n DROPLETS" << " -px PIXELS" << " -pd DISTANCE" << " -d DIAMETERS" << " [-rho DENSITY]" << " [-t]" << " [-s]" << std::endl;
	std::cerr << std::endl;
	std::cerr << "positional arguments:" << std::endl;
	std::cerr << " FILEPATH\t\tpath to video file (e.g. .mov, .mp4, etc.)" << std::endl;
	std::cerr << std::endl;
	std::cerr << "required arguments:" << std::endl;
	std::cerr << " -n DROPLETS, --droplets DROPLETS\n\t\t\tnumber of droplets to track" << std::endl;
	std::cerr << " -px PIXELS, --pixels PIXELS\n\t\t\tdistance between plates in pixels" << std::endl;
	std::cerr << " -pd DISTANCE, --distance DISTANCE\n\t\t\tdistance between plates in microns" << std::endl;
	std::cerr << " -d DIAMETERS, --diameters DIAMETERS\n\t\t\tdiameter of each droplet in pixels (e.g. '20.5 10')" << std::endl;
	std::cerr << "options:" << std::endl;
	std::cerr << " -rho DENSITY, --density DENSITY\n\t\t\tdensity of droplets in kg/m^3 (Default: 1000)" << std::endl;
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
		} else if(strcmp(argv[i], "-pd") == 0 || strcmp(argv[i], "--distance") == 0) {
			if(i + 1 < argc) {
				DISTANCE = std::strtod(argv[++i], NULL);
			} else {
				std::cerr << "-pd DISTANCE option requires one argument" << std::endl;
				return 1;
			}
		} else if(strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--diameters") == 0) {
			if(i + 1 < argc) {
				std::string sDiameters = argv[++i];
				std::stringstream ss(sDiameters);
				std::string sDiameter;
				while(ss >> sDiameter) {
					DIAMETERS.push_back(std::stod(sDiameter));
				}
			} else {
				std::cerr << "-d DIAMETERS option requires atleast one argument" << std::endl;
				return 1;
			}
		} else if(strcmp(argv[i], "-rho") == 0 || strcmp(argv[i], "--density") == 0) {
			if(i + 1 < argc) {
				DENSITY = std::strtod(argv[++i], NULL);
			} else {
				std::cerr << "-rho DENSITY option requires one arguement" << std::endl;
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
		std::cerr << "requires -pd DISTANCE" << std::endl;
		return 1;
	}
	if(DIAMETERS.size() == 0) {
		std::cerr << "requires -d DIAMETERS" << std::endl;
		return 1;
	}
	return 0;
}

// Store data to parquet
arrow::Status store_data(std::string &filepath , std::vector<std::vector<double>> &x, std::vector<std::vector<double>> &y) {
	// Create fields (column names) and array_vector (data)
	arrow::FieldVector fields;
	arrow::ArrayVector array_vector;

	// Store x,y data into arrow::Table for output
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

	// Store droplet diameters
	fields.push_back(arrow::field("DIAMETERS", arrow::float64()));
	arrow::DoubleBuilder dbuilder_diam;
	std::shared_ptr<arrow::Array> diameter_arr;
	ARROW_RETURN_NOT_OK(dbuilder_diam.AppendValues(DIAMETERS));
	ARROW_RETURN_NOT_OK(dbuilder_diam.AppendNulls(NUM_FRAMES - DIAMETERS.size()));
	ARROW_RETURN_NOT_OK(dbuilder_diam.Finish(&diameter_arr));
	array_vector.push_back(diameter_arr);

	// Store droplet density
	fields.push_back(arrow::field("DENSITY", arrow::float64()));
	arrow::DoubleBuilder dbuilder_den;
	std::shared_ptr<arrow::Array> density_arr;
	ARROW_RETURN_NOT_OK(dbuilder_den.Append(DENSITY));
	ARROW_RETURN_NOT_OK(dbuilder_den.AppendNulls(NUM_FRAMES - 1));
	ARROW_RETURN_NOT_OK(dbuilder_den.Finish(&density_arr));
	array_vector.push_back(density_arr);

	// Store FPS
	fields.push_back(arrow::field("FPS", arrow::float64()));
	arrow::DoubleBuilder dbuilder_fps;
	std::shared_ptr<arrow::Array> fps_arr;
	ARROW_RETURN_NOT_OK(dbuilder_fps.Append(FPS));
	ARROW_RETURN_NOT_OK(dbuilder_fps.AppendNulls(NUM_FRAMES - 1));
	ARROW_RETURN_NOT_OK(dbuilder_fps.Finish(&fps_arr));
	array_vector.push_back(fps_arr);

	// Create schema and data table
	std::shared_ptr<arrow::Schema> schema = arrow::schema(fields);
	std::shared_ptr<arrow::Table> table = arrow::Table::Make(schema, array_vector);

	// Write table to output file with the schema
	std::shared_ptr<arrow::io::FileOutputStream> outfile;
	ARROW_ASSIGN_OR_RAISE(outfile, arrow::io::FileOutputStream::Open(filepath));
	ARROW_RETURN_NOT_OK(parquet::arrow::WriteTable(*table, arrow::default_memory_pool(), outfile, 3));
	return outfile->Close();
}

// Get most significant bit
int getMSB(int val) {
	if(val == 0) {
		return 0;
	}
	int msb = 0;
	val = val / 2;
	while(val != 0) {
		val = val / 2;
		msb++;
	}
	return (1 << msb);
}

// Program entry
int main(int argc, char** argv) {
	// Parse arguements and ends program if error
	if(parse_args(argc, argv) == 1) {
		return 1;
	}

	// Calculates ratio (microns : px)
	double ratio = DISTANCE / PX_DISTANCE;

	// Updates diameters from pixels to microns
	for(int i = 0; i < DIAMETERS.size(); i++) {
		DIAMETERS[i] = DIAMETERS[i] * ratio;
	}

	// Opens video and reads into frame
	cv::VideoCapture video(PATH);
	cv::Mat frame;
	bool ok = video.read(frame);
	if(!video.isOpened()) {
		std::cerr << "Could not open video" << std::endl;
		return -1;
	}

	// Gets number of frames in video and FPS
	NUM_FRAMES = (int)video.get(cv::CAP_PROP_FRAME_COUNT);
	FPS = video.get(cv::CAP_PROP_FPS);

	// Initializes a vector of trackers and bboxes
	std::vector<cv::Ptr<cv::Tracker>> trackers;
	std::vector<cv::Rect> bboxes;
	for(int i = 0; i < NUM_DROPLETS; i++) {
		// Create tracker and bbox
		trackers.push_back(cv::TrackerCSRT::create());
		if(i == 0) {
			bboxes.push_back(cv::selectROI(frame, false));
		} else {
			bboxes.push_back(cv::selectROI(frame, false, false, false));
		}
		
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

	// Display progress bar (updates in roughly 5% intervals)
	int pCount = 0;
	int pUpdateFrame = getMSB(NUM_FRAMES / 20);
	float pRatio = (float)(pUpdateFrame * 100.0 / NUM_FRAMES);
	std::string pBar = '[' + std::string(ceil((float)NUM_FRAMES / pUpdateFrame), '.') + ']';
	std::cout << "Tracking...\n";
	std::cout << pBar << " " << std::setprecision(4) << pCount * pRatio << "%\t\r";
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

		// Update progress bar
		if(!(j & (pUpdateFrame - 1))) {
			pBar[++pCount] = '=';
			std::cout << pBar << " " << std::setprecision(4) << pCount * pRatio << "%\t\r";
		}
	}

	// Display tracking complete
	pBar[++pCount] = '=';
	std::cout << pBar << " 100%\t\n";
	std::cout << "Tracking complete!\n";

	// Store data in paruqet file
	std::cout << "Storing data...\n";
	std::string out_file_name = std::filesystem::path(PATH).stem().string() + "_out.parquet";
	arrow::Status st = store_data(out_file_name, x, y);
	if(!st.ok()) {
		std::cerr << st << std::endl;
	} else {
		std::cout << "Data Stored!\n";
	}

	// Display total runtime of algorithm
	std::chrono::system_clock::time_point end_time;
	if(TIMEIT) {
		end_time = std::chrono::system_clock::now();
		std::chrono::duration<double> elapsed_seconds = end_time - start_time;
		std::cout << "Elapsed time: " << elapsed_seconds.count() << " s" << std::endl;
	}

	// Garbage collection
	video.release();
	cv::destroyAllWindows();

	return 0;
}