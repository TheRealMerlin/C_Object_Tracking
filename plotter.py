import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
from pyarrow.parquet import read_metadata
import argparse

def plotter(DATA_OUT_FILE, FPS):
    '''
        Plots x and y plots for each droplet.
        Expects parquet file in following format for n droplets and N frames:
            x_0         y_0         ...     x_n-1       y_n-1
        0   <float>     <float>     ...     <float>     <float>
        ...
        N-1 <float>     <float>     ...     <float>     <float>
    '''
    # Get number of columns and rows from data file metadata
    numCols = read_metadata(DATA_OUT_FILE).num_columns
    numRows = read_metadata(DATA_OUT_FILE).num_rows

    # Get number of droplets
    numDroplets = numCols // 2

    # Get x and y values from parquet file
    xVals = []
    yVals = []
    for i in range(numDroplets):
        xVals.append(pd.read_parquet(DATA_OUT_FILE, columns=["x_" + str(i)]).to_numpy())
        yVals.append(pd.read_parquet(DATA_OUT_FILE, columns=["y_" + str(i)]).to_numpy())

    # Get v_x and v_y
    v_x = [[0] for i in range(numDroplets)]
    v_y = [[0] for i in range(numDroplets)]
    for i in range(numDroplets):
        for j in range(1, numRows):
            v_x[i].append((xVals[i][j] - xVals[i][j - 1]) / FPS)
            v_y[i].append((yVals[i][j] - yVals[i][j - 1]) / FPS)

    # Get a_x and a_y
    a_x = [[0] for i in range(numDroplets)]
    a_y = [[0] for i in range(numDroplets)]
    for i in range(numDroplets):
        for j in range(1, numRows):
            a_x[i].append((v_x[i][j] - v_x[i][j - 1]) / FPS)
            a_y[i].append((v_y[i][j] - v_y[i][j - 1]) / FPS)

    # Create list of time corresponding to each frame
    times = [i * FPS for i in range(1, numRows + 1)]

    # Create a_x(t) and a_y(t) plots
    for i in range(numDroplets):
        plt.figure(i * 2)
        plt.grid()
        plt.title("Droplet " + str(i + 1))
        plt.xlabel(r"$t$ (s)")
        plt.ylabel(r"$a_x$ (mircon / s$^2$)")
        plt.plot(times, a_x[i])

        plt.figure(i * 2 + 1)
        plt.grid()
        plt.title("Droplet " + str(i + 1))
        plt.xlabel(r"t (s)")
        plt.ylabel(r"$a_y$ (mircon / s$^2$)")
        plt.plot(times, a_y[i])

    # Display all plots
    plt.show()
    

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("FILEPATH", help="path to parquet file")
    parser.add_argument("-f", "--fps", type=float, required=True, help="frames per second associated with file")

    args = parser.parse_args()
    plotter(args.FILEPATH, args.fps)