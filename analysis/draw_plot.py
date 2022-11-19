from pandas import read_csv
from matplotlib import pyplot
from datetime import datetime

series = read_csv('../serial_client/output.csv', header=0, index_col=0, parse_dates=['datetime'], squeeze=True)
# series['tvoc'] = series['tvoc'].astype(float)
# print(series.dtypes)

# series.plot()

# pyplot.figure(1)
# pyplot.ylim(0, 4000)
# series['co2'].plot(label='co2')
# pyplot.legend()

# pyplot.figure(2)
# pyplot.ylim(0, 4000)
# series['tvoc'].plot(label='tvoc')
# pyplot.legend()

pyplot.ylim(0, 4000)
series['tvoc'].plot(label='tvoc')
pyplot.legend()

pyplot.show()