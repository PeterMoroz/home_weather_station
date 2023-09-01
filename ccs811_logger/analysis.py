import sys
import os
import pandas as pd
import matplotlib.pyplot as plt


if len(sys.argv) != 2:
    print(f"usage: {sys.argv[0]} <path_to_csv_file>")
    quit()


file_path = sys.argv[1]

data = pd.read_csv(file_path, index_col='datetime', parse_dates=True)

plt.figure(1)
data['eTVOC'].plot(label='eTVOC', color='orange')
plt.legend()
plt.grid(which='major', color='#300000', linestyle='-')
plt.minorticks_on()
plt.grid(which='minor', color='#900000', linestyle=':')
plt.get_current_fig_manager().set_window_title(f"{file_path} - eTVOC")

plt.figure(2)
data['eCO2'].plot(label='eCO2', color='violet')
plt.legend()
plt.grid(which='major', color='#300000', linestyle='-')
plt.minorticks_on()
plt.grid(which='minor', color='#900000', linestyle=':')

plt.get_current_fig_manager().set_window_title(f"{file_path} - eCO2")
plt.show()
