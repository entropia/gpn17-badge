from flask import Flask, redirect
from time import sleep
import json

app = Flask(__name__, static_url_path='', static_folder='data/system/web')


@app.route('/')
def index():
    return redirect('/index.html')


@app.route('/api/scan')
def scan():
    # Simulate scan
    sleep(5)
    return json.dumps([{'id': i, 'ssid': 'Net' + str(i), 'rssi': '-20', 'encType': 4 if i%2 == 0 else 7} for i in range(0, 10)])


if __name__ == '__main__':
    app.run(host='localhost', port=5000, threaded=False, debug=True)
