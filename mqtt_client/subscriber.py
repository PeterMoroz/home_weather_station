from paho.mqtt import client as mqtt

def on_connect(client, userdata, flags, rc):
    client.subscribe([("dht/temperature", 0), ("dht/humidity", 0), ("ccs811/eco2", 0), ("ccs811/tvoc", 0)])

def on_message(client, userdata, msg):
    print(f"{msg.topic}: {msg.payload.decode()}")

def run():
    client = mqtt.Client()
    client.connect('broker.emqx.io', 1883)
    client.on_connect = on_connect
    client.on_message = on_message
    client.loop_forever()

if __name__ == '__main__':
    run()