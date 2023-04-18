# How to use the program
## x64/Release/C++_Object_Tracking.exe
The program is intended to run in command line with the following commands:
```console
usage: C++_Object_Tracking.exe FILEPATH [-h] -n DROPLETS -px PIXELS -d DISTANCE [-t] [-s]

positional arguments:
 FILEPATH               path to video file (e.g. .mov, .mp4, etc.)

required arguments:
 -n DROPLETS, --droplets DROPLETS
                        number of droplets to track
 -px PIXELS, --pixels PIXELS
                        distance between plates in pixels
 -d DISTANCE, --distance DISTANCE
                        distance between plates in microns
options:
 -h, --help             show this help message and exit
 -t, --timeit           prints run-time of tracking algorithm
 -s, --show             displays video with trackers
```
The distance between the plates needs to be determined with some other software using a frame from the video (e.g. GIMP).

The program will output "FILENAME_out.parquet" in the directory of the .exe in the following format:

|  | x<sub>0</sub> | y<sub>0</sub> | ... | x<sub>n-1</sub> | y<sub>n-1</sub> |
| --- | --- | --- | --- | --- | --- |
| 0 | \<float> | \<float> | ... | \<float> | \<float> |
| ... |  |  | ... | |  |
| N-1 | \<float> | \<float> | ... | \<float> | \<float> |

where n is the number of droplets and N is the number of frames.

## plotter.py
This python script will convert the positional data outputed from the executable into graphs of a<sub>x,y</sub> vs t for each droplet. It also requires the FPS of the video from which the data was generated from. It's usage from command line is as follows:
```console
usage: plotter.py [-h] -f FPS FILEPATH

positional arguments:
  FILEPATH           path to parquet file

options:
  -h, --help         show this help message and exit
  -f FPS, --fps FPS  frames per second associated with file
```