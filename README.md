# Server-Client Traffic Monitoring System

A system that handles traffic activity. 
System actors: a single server and multiple clients, acting as traffic participants.

System functionalities:

* sends data regarding maximum driving speed on a given street portion to all the connected clients 
* sends data regarding weather, sport events, peco prices only to the subscribed traffic participants
* a traffic participant announces an accident on a given street. The server is able to communicate the incident to all the connected clients
* every minute, the clients will send data regarding their driving speed at a given moment in time. The server will update this particular information for all the clients.

## Built With

* C/C++
* SQLite Database

## Authors

* **Gutu Tiberiu** 

