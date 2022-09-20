import sqlite3  
  
con = sqlite3.connect("heartrate.db")  
print("Database opened successfully")  
  
con.execute("create table heartrate (id INTEGER PRIMARY KEY AUTOINCREMENT, date  TEXT UNIQUE NOT NULL, hr REAL NOT NULL)")  
con.execute("create table falls (id INTEGER PRIMARY KEY AUTOINCREMENT, date  TEXT UNIQUE NOT NULL)")
con.execute("create table tremors (id INTEGER PRIMARY KEY AUTOINCREMENT, date TEXT UNIQUE NOT NULL, acc_y text not null,acc_z text not null,acc_x text not null) ")
print("Table created successfully")  
  
con.close()  