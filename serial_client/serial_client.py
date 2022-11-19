import serial
import datetime

port = serial.Serial(port = "COM3", baudrate=115200, bytesize=8, timeout=2, stopbits=serial.STOPBITS_ONE)

line = ""
file_out = open('output.txt', 'w')
file_csv = open('output.csv', 'w')

total_line_count = 0
csv_line_count = 0

file_csv.write('datetime,co2,tvoc\n')

def parse_line(line):
    co2 = ''
    tvoc = ''
    
    if line[0:6] != 'CCS811':
        return None
        
    pos0 = line.find('CO2:')
    pos1 = line.find('ppm')
    if pos0 == -1 or pos1 == -1 or pos1 <= pos0:
        return None
    
    pos0 += 5
    pos1 -= 1
    co2 = line[pos0:pos1]
    
    pos0 = line.find('TVOC:')
    pos1 = line.find('ppb')
    if pos0 == -1 or pos1 == -1 or pos1 <= pos0:
        return None
        
    pos0 += 6
    pos1 -= 1
    tvoc = line[pos0:pos1]
    
    return (co2, tvoc)

while True:
    if port.in_waiting > 0:
        line = port.readline()
        # print(line)        
        # print(line.decode('ascii', errors='ignore'))
        s = line.decode('ascii', errors='ignore')
        file_out.write(s)
        total_line_count += 1
        if total_line_count % 10 == 0:
            file_out.flush()
            
        vals = parse_line(s)
        if vals == None:
            print('Could not parse line\t' + s + '\n')
            continue
        
        co2, tvoc = vals
        curr_datetime = datetime.datetime.now()
        # file_csv.write('{},{},{}\n'.format(curr_datetime, co2, tvoc))
        file_csv.write('{},{},{}\n'.format(curr_datetime.strftime('%Y-%m-%dT%H:%M:%S'), co2, tvoc))
        csv_line_count += 1
        if csv_line_count % 100 == 0:
            file_csv.flush()