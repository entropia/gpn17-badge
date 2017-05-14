from flask import Flask, redirect, request
from time import sleep
import json

app = Flask(__name__, static_url_path='', static_folder='data_raw/system/web')


@app.route('/')
def index():
    return redirect('/index.html')


@app.route('/api/scan')
def scan():
    # Simulate scan
    sleep(5)
    return json.dumps(
        [{'id': i, 'ssid': 'Net' + str(i), 'rssi': '-20', 'encType': 4 if i % 2 == 0 else 7} for i in range(0, 10)])


@app.route('/api/conf/wifi', methods=['POST'])
def conf_wifi():
    # Simulate connect
    sleep(5)
    pw = request.form['pw']
    id = int(request.form['net'])
    if pw == "correct" or id % 2 != 0:
        return "true"
    return "false"


@app.route('/api/conf/nick', methods=['POST'])
def conf_nick():
    # Simulate save
    sleep(1)
    return "true"


if __name__ == '__main__':
    app.run(host='localhost', port=5000, threaded=False, debug=True)
