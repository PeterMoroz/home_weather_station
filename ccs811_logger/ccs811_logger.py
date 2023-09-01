from flask import Flask, request, render_template
from datetime import datetime

import argparse
import csv

import os.path


out = None
app = Flask(__name__)

@app.route('/data')
def data():
    global fpath
    with open(fpath) as file:
        reader = csv.reader(file)
        return render_template('csv_table.html', csv=reader)

@app.route('/ccs811')
def ccs811():
    etvoc = request.args.get('etvoc')
    eco2 = request.args.get('eco2')
    
    if etvoc != None and eco2 != None:
        print(f"eTVOC: {etvoc}, eCO2: {eco2}")
        global out
        if out is not None:
            now = datetime.now()
            dts = now.strftime('%d/%m/%Y %H:%M:%S')
            out.write(f'{now}, {etvoc}, {eco2}\n')
    else:
        print("wrong request - misssed data")

    return ''
    
if __name__ == '__main__':

    ap = argparse.ArgumentParser()
    ap.add_argument("-p", "--port", type=int, default=5000, help="port, the server listen to (1024 to 65535)")
    ap.add_argument("-d", "--directory", type=str, help="the output directory", required=True)
    args = vars(ap.parse_args())
    
    port=args["port"]
    basedir = args["directory"]
    
    print(f'directory to write log = {basedir}\n')

    global fpath

    now = datetime.now()
    dts = now.strftime('%d-%m-%Y_%H-%M-%S')
    fname = f'css811_{dts}.csv'
    fpath = os.path.join(basedir, fname)
    out = open(fpath, 'w', buffering=1)
    if out is None:
        print(f'Could not open file {fpath}!\n')
        os.exit(0)
    out.write('datetime,eTVOC,eCO2\n')
    app.run(host='0.0.0.0', debug=True, port=port)
    out.close()
