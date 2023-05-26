# Compile
## Linux (Fedora)
``` console
dnf install opencv-devel libarrow-devel
cmake .
make
```

# How to use the program
## x64/Release/C++_Object_Tracking.exe
The program is intended to run in command line with the following commands:
```console
usage: C++_Object_Tracking.exe FILEPATH [-h] -n DROPLETS -px PIXELS -pd DISTANCE -d DIAMETERS [-rho DENSITY] [-t] [-s]

positional arguments:
 FILEPATH               path to video file (e.g. .mov, .mp4, etc.)

required arguments:
 -n DROPLETS, --droplets DROPLETS
                        number of droplets to track
 -px PIXELS, --pixels PIXELS
                        distance between plates in pixels
 -pd DISTANCE, --distance DISTANCE
                        distance between plates in microns
 -d DIAMETERS, --diameters DIAMETERS
                        diameter of each droplet in pixels (e.g. '20.5 10')
options:
 -rho DENSITY, --density DENSITY
                        density of droplets in kg/m^3 (Default: 1000)
 -h, --help             show this help message and exit
 -t, --timeit           prints run-time of tracking algorithm
 -s, --show             displays video with trackers
```
The distance between the plates and pixel diameters needs to be determined with some other software using a frame from the video (e.g. GIMP).

The program will output "FILENAME_out.parquet" in the directory of the .exe in the following format:

|  | x<sub>0</sub> | y<sub>0</sub> | ... | x<sub>n-1</sub> | y<sub>n-1</sub> | DIAMETERS | DENSITY | FPS |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | \<double> | \<double> | ... | \<double> | \<double> | \<double> | \<double> | \<double> |
| 1 | \<double> | \<double> | ... | \<double> | \<double> | \<double> | NULL | NULL |
| ... |  |  | ... |  |  |  |  |  |
| n - 1 | \<double> | \<double> | ... | \<double> | \<double> | \<double> | NULL | NULL |
| n | \<double> | \<double> | ... | \<double> | \<double> | NULL | NULL | NULL |
| ... |  |  | ... |  |  |  |  |  |
| N-1 | \<double> | \<double> | ... | \<double> | \<double> | NULL | NULL | NULL |

where:
- n is the number of droplets
- N is the number of frames
- DIAMETERS contains the diameters of each droplet in microns corresponding to its index
- DENSITY contains density of the droplets in kg/m^3 (defaults to density of water 1000 kg/m^3)
- FPS contains the frames per second of the video

## plotter.py
This python script will convert the positional data outputed from the executable into graphs of F<sub>x,y</sub> vs t for each droplet.

It's usage from command line is as follows:
```console
usage: py plotter.py [-h] [-f FPS] FILEPATH

positional arguments:
  FILEPATH           path to parquet file

options:
  -h, --help         show this help message and exit
  -f FPS, --fps FPS  frames per second associated with file
```
