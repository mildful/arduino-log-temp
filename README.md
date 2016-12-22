# arduino-log-temp
Just playing and discovering the CC3200. The CC3200 get current temperature and send data to a MQTT broker
(mosquitto). A little Go script subscribe to the broker and store data in a time-series db (influxDB).
Grafana is used to display datas.

- [CC3200](https://github.com/mildful/arduino-log-temp)
- [Go server](https://github.com/mildful/go-iot-server)
