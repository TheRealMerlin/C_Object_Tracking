import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
from pyarrow.parquet import read_metadata
import argparse

np.warnings.filterwarnings('ignore', category=np.VisibleDeprecationWarning)

def plotter(DATA_OUT_FILE):
    '''
        Plots x and y plots for each droplet.
        Expects parquet file in following format for n droplets and N frames:
            x_0         y_0         ...     x_n-1       y_n-1       DIAMETERS   DENSITY     FPS
        0   <double>    <double>    ...     <double>    <double>    <double>    <double>    <double>
        1   <double>    <double>    ...     <double>    <double>    <double>    NULL        NULL
        ...
        n-1 <double>    <double>    ...     <double>    <double>    <double>    NULL        NULL
        n   <double>    <double>    ...     <double>    <double>    NULL        NULL        NULL
        ...
        N-1 <double>    <double>    ...     <double>    <double>    NULL        NULL        NULL
    '''

    # Get number of columns and rows from data file metadata
    numCols = read_metadata(DATA_OUT_FILE).num_columns
    numRows = read_metadata(DATA_OUT_FILE).num_rows

    # Get DIAMETERS, DENSITY, and FPS from parquet
    numDroplets = (numCols - 3) // 2
    DIAMETERS = pd.read_parquet(DATA_OUT_FILE, columns=["DIAMETERS"]).to_numpy()[:][:numDroplets]
    DIAMETERS = np.reshape(DIAMETERS, numDroplets)
    DENSITY = pd.read_parquet(DATA_OUT_FILE, columns=["DENSITY"]).to_numpy()[0][0]
    FPS = pd.read_parquet(DATA_OUT_FILE, columns=["FPS"]).to_numpy()[0][0]

    # Create list of time corresponding to each frame
    times = [i / FPS for i in range(numRows)]

    # Calculate mass of each droplet in kg
    masses = np.zeros(numDroplets)
    for i in range(numDroplets):
        masses[i] = DENSITY * (2/3) * np.pi * np.power(DIAMETERS[i] * 1e-6, 3)

    # Get x and y values from parquet file
    xVals = []
    yVals = []
    for i in range(numDroplets):
        xVals.append(pd.read_parquet(DATA_OUT_FILE, columns=["x_" + str(i)]).to_numpy())
        yVals.append(pd.read_parquet(DATA_OUT_FILE, columns=["y_" + str(i)]).to_numpy())

    xVals = np.reshape(xVals, (numDroplets, numRows))
    yVals = np.reshape(yVals, (numDroplets, numRows))

    # Get position displacement of droplet
    x_dis = []
    y_dis = []
    for i in range(numDroplets):
        x_dis.append(np.array(xVals[i]) - np.average(xVals[i]))
        y_dis.append(np.array(yVals[i]) - np.average(yVals[i]))

    # Get v_x and v_y
    v_x = [[0] for i in range(numDroplets)]
    v_y = [[0] for i in range(numDroplets)]
    for i in range(numDroplets):
        for j in range(1, numRows):
            v_x[i].append((xVals[i][j] - xVals[i][j - 1]) * FPS)
            v_y[i].append((yVals[i][j] - yVals[i][j - 1]) * FPS)

    # Get velocity displacement of droplet
    v2_dis = []
    for i in range(numDroplets):
        v = np.sqrt(np.power(v_x[i], 2) + np.power(v_y[i], 2))
        vx_avg = np.average(v_x[i])
        vy_avg = np.average(v_y[i])
        v_avg = np.sqrt(np.power(vx_avg, 2) + np.power(vy_avg, 2))
        v2_dis.append(np.power(v - v_avg, 2))

    # Get a_x and a_y (microns / s^2)
    a_x = [[0] for i in range(numDroplets)]
    a_y = [[0] for i in range(numDroplets)]
    for i in range(numDroplets):
        for j in range(1, numRows):
            a_x[i].append((v_x[i][j] - v_x[i][j - 1]) * FPS)
            a_y[i].append((v_y[i][j] - v_y[i][j - 1]) * FPS)

    # Convert a_x and a_y to m/s^2
    a_x = np.array(a_x) * 1e-6
    a_y = np.array(a_y) * 1e-6

    # Create a_x(t) and a_y(t) plots
    for i in range(numDroplets):
        plt.figure()
        plt.grid()
        plt.title("Droplet " + str(i + 1))
        plt.xlabel(r"$t$ (s)")
        plt.ylabel(r"$F_x$ (N)")
        plt.plot(times, masses[i] * a_x[i])

        plt.figure()
        plt.grid()
        plt.title("Droplet " + str(i + 1))
        plt.xlabel(r"t (s)")
        plt.ylabel(r"$F_y$ (N)")
        plt.plot(times, masses[i] * a_x[i])

        # Creates histograms
        plt.figure()
        plt.hist(x_dis[i])
        plt.title("dx " + str(i + 1))
        plt.xlabel("Microns")
        plt.ylabel("Number of points")

        plt.figure()
        plt.hist(y_dis[i])
        plt.title("dy " + str(i + 1))
        plt.xlabel("Microns")
        plt.ylabel("Number of points")

        plt.figure()
        plt.hist(v2_dis[i])
        plt.title("d(v)^2 " + str(i + 1))
        plt.xlabel("(Microns per second)^2")
        plt.ylabel("Number of points")

    # Display all plots
    plt.show()
    

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("FILEPATH", help="path to parquet file")

    args = parser.parse_args()
    plotter(args.FILEPATH)