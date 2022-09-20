from time import time
from typing import Iterator
from pkg_resources import get_build_platform
from flask import Flask, request, render_template,Response,stream_with_context
import telegram_send
import json
import pandas as pd
import json
import plotly
import plotly.express as px
import sqlite3 as sql
import numpy as np
from datetime import datetime
import time
import random
app = Flask(__name__)





lastfallday = "0"
lastfallmon = "0"
lastfallyear = "0"
lastfallhr = "0"
lastfallmin = "0"
hr = "0"
lasttremordate = "0"

def add_data(date, acc_x, acc_y, acc_z):  
  try:
    # Connecting to database
    con = sql.connect('tremors.db')
    # Getting cursor
    c =  con.cursor() 
    # Adding data
    c.execute("INSERT INTO Tremors (date, acc_x, acc_y, acc_z) VALUES (%s, %s, %s, %s)" %(date, acc_x, acc_y, acc_z))
    # Applying changes
    con.commit() 
  except:
    print("An error has occured")



@app.route('/post', methods = ["POST"])
def post():

    global lastfallday
    global lastfallmon
    global lastfallyear
    global lastfallhr
    global lastfallmin
    global hr
    global lasttremordate

    y = json.loads(request.data)
    #print(y)
    
    hr = float(y["HR"])
    fallday = y["fallDay"]
    fallmon = y["fallMon"]
    fallyear = y["fallYear"]
    fallhr = y["fallHr"]
    fallmin = y["fallMin"]
    tremordate = f'{y["tremorDay"]}-{y["tremorMon"]}-{y["tremorYear"]} {y["tremorHr"]}:{y["tremorMin"]}'
    
    if int(fallmin) < 10:
        fallmin = '0' + str(fallmin)

    if (hr < 40 or hr > 120) and (hr != 0):
        now = datetime.now()
        hralert = f"Alert! Heart rate is {hr}"
        telegram_send.send(messages=[hralert])
        hrcon = sql.connect(('heartrate.db'))
        hrcur = hrcon.cursor()
        hrcur.execute('insert or ignore into heartrate(date,hr) values (?,?)',(now.strftime("%d/%m/%Y %H:%M:%S"),hr))
        hrcon.commit()
        hrcon.close()
    
    if fallday != lastfallday or lastfallmon != fallmon or lastfallyear != fallyear or lastfallhr != fallhr or lastfallmin != fallmin:
        fallalert = f"Alert! Patient fell at {fallhr}:{fallmin}"
        lastfallday  = fallday
        lastfallmon = fallmon
        lastfallyear = fallyear
        lastfallhr = fallhr
        lastfallmin = fallmin
        hrcon = sql.connect(('heartrate.db'))
        hrcur = hrcon.cursor()
        falldate = f'{lastfallday}-{lastfallmon}-{lastfallyear} {lastfallhr}:{lastfallmin}'
        hrcur.execute('insert or ignore into falls(date) values (?)',(falldate,))
        hrcon.commit()
        hrcon.close()
        telegram_send.send(messages=[fallalert])


    if lasttremordate != tremordate:
        lasttremordate = tremordate
        acc_x = y["acc_x"]
        acc_y = y["acc_y"]
        acc_z = y["acc_z"]

        print("this is ",','.join([str(ax) for ax in acc_x]))
        tmrcon = sql.connect(('heartrate.db'))
        tmrcur = tmrcon.cursor()
        tmrcur.execute('insert or ignore into tremors(date,acc_x,acc_y,acc_z) values (?,?,?,?)',(tremordate,','.join([str(ax) for ax in acc_x]),','.join([str(ay) for ay in acc_y]),','.join([str(az) for az in acc_z])))
        tmrcon.commit()
        tmrcon.close()
        #for i in range(0,128):
            #add_data(lasttremordate, y["acc_x"][i], y["acc_y"][i], y["acc_z"][i])
                        


    return ''

@app.route('/', methods=['GET', 'POST'])
def index():

    hrcon = sql.connect(('heartrate.db'))



    
    hrcur = hrcon.cursor()
    
    hrcur.execute('SELECT * FROM heartrate ORDER BY id DESC LIMIT 1;')
    heartrate_data = hrcur.fetchall()
    print("data",heartrate_data) 

    hrcur.execute('SELECT * FROM falls ORDER BY id DESC LIMIT 1;')
    lastfall_data = hrcur.fetchall()
    print("fell on:",lastfall_data)

    hrcur.execute('SELECT * FROM tremors ORDER BY id DESC LIMIT 1;')
    tremordata = hrcur.fetchall()
    print("tremors:",tremordata)

    lastfallday =  lastfall_data[0][1].split('-')[0]
    lastfallmon =  lastfall_data[0][1].split('-')[1]
    lastfallyear = lastfall_data[0][1].split('-')[2].split(' ')[0]
    lastfallhr =   lastfall_data[0][1].split('-')[2].split(' ')[1].split(':')[0]
    lastfallmin =  lastfall_data[0][1].split('-')[2].split(' ')[1].split(':')[1]

    
    hrcon.close()
    #print(hr)
    if len(heartrate_data) >= 1:
        heartrate_arg = heartrate_data[0][2]
    else:
        heartrate_arg = "Nil"
    return render_template('wearabledashboard.html',heartrate=hr,Day=lastfallday,Month=lastfallmon,Year=lastfallyear,Hour=lastfallhr,Min=lastfallmin)   

@app.route('/chart-data')
def chart_data():
    tmrcon = sql.connect(('heartrate.db'))
    tmrcur = tmrcon.cursor()
    tmrcur.execute('SELECT * FROM tremors ORDER BY id DESC LIMIT 1;')
    tremordata = tmrcur.fetchall()
    tmrcon.commit()
    tmrcon.close()
    print("tremors :",tremordata)
    acc_x = [int(x) for x in tremordata[0][2].split(',')]
    acc_y = [int(x) for x in tremordata[0][3].split(',')]
    acc_z = [int(x) for x in tremordata[0][4].split(',')]      

    def generate_data() -> Iterator[str]:
        i = 0
        while True:               
            json_data = json.dumps(
                {                    "time": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                    "ax": acc_x[i],"ay": acc_y[i],"az":acc_z[i]})
            print("json",json_data)
            print("length",len(acc_x))
            yield f"data:{json_data}\n\n"
            i += 1
            time.sleep(1)
    response = Response(stream_with_context(generate_data()), mimetype="text/event-stream")
    response.headers["Cache-Control"] = "no-cache"
    response.headers["X-Accel-Buffering"] = "no"
    return response

@app.route('/callback', methods=['POST', 'GET'])
def cb():
    return gm(request.args.get('data'))
   
@app.route('/tremorplot')
def tremorplot():
    return render_template('tremors.html', date=lasttremordate, graphJSON=gm())


@app.route('/history')
def history():
    return render_template('history.html')

@app.route('/heartrate')
def heartrate():

    hrcon = sql.connect(('heartrate.db'))
    hrcur = hrcon.cursor()
    hrcur.execute('SELECT * FROM heartrate order by id desc')
    heartrate_data = hrcur.fetchall()
    hrcon.close()

    return render_template('heartrate.html',data=heartrate_data)    

@app.route('/falls')
def falls():
    fallcon = sql.connect('heartrate.db')
    fallcur = fallcon.cursor()
    fallcur.execute('SELECT * FROM falls order by id desc')
    fall_data = fallcur.fetchall()
    fallcon.close()
    
    return render_template('falls.html',data=fall_data)  

def gm():
    global acc_x
    global acc_y
    global acc_z
    #acc_x_val = 
    namelist = ['TremorX','TremorY','TremorZ']
    t   =   np.linspace(0,10,128)
    df  =   pd.DataFrame([acc_x,acc_y,acc_z],index=namelist,columns=t)
    df.T
    fig =   px.line(df.T)
    fig.show()
    graphJSON = json.dumps(fig, cls=plotly.utils.PlotlyJSONEncoder)
    print(fig.data[1])
    #fig.data[0]['staticPlot']=True
    
    return graphJSON    


app.run(debug=True, host='0.0.0.0', port= 5000)


