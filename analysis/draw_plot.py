from pandas import read_csv
from matplotlib import pyplot
from datetime import datetime
from matplotlib.widgets import Slider

series = read_csv('../serial_client/output.csv', header=0, index_col=0, parse_dates=['datetime'])
# series = read_csv('../serial_client/output.csv', header=0, parse_dates=['datetime'])
# print(series.head())

series['tvoc'] = series['tvoc'].astype(float)
print(series.dtypes)

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

N = 3600

x = series['datetime']
y = series['tvoc']
# fig = pyplot.figure()
# ax = fig.subplots()
# p = ax.plot(x, y)

# pyplot.show()

fig, ax = pyplot.subplots(figsize=(10, 6))

def bar(pos):
    pos = int(pos)
    ax.clear()
    if pos + N > len(x):
        n = len(x) - pos
    else:
        n = N
    
    xvalues = x[pos:pos + n]
    yvalues = y[pos:pos + n]
    ax.plot(xvalues, yvalues)
    
    
barpos = pyplot.axes([0.18, 0.05, 0.55, 0.03])
slider = Slider(barpos, 'timeline', 0, len(x) - N)
slider.on_changed(bar)

bar(0)
pyplot.show()