from paho.mqtt import client as mqtt

def on_connect(client, userdata, flags, rc):
    client.subscribe([("environmental", 0), ("aiq", 0)])

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