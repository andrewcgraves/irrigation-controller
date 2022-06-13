# Irrigation Controller
An irrigation controller system designed to run on an an **Arduino UNO WIFI Rev 2**

It's a pretty simple system and i've tried keeping it organised but heres a quick breakdown I will update as things are added

---

## Core Functionality
- Ability to control the states with ~~**Auto (PIN 8)**~~ and **Manual (PIN 7)** [**buttons**](https://www.sparkfun.com/products/9190)
- An LED light [**WS2812B**](https://www.sparkfun.com/products/13282) on pin 4 used to indicate state of the system at any point
- Ability to control over the local network using [**PubSubClient**](https://www.arduino.cc/reference/en/libraries/pubsubclient/) (A library enabling the use of [**MQTT**](https://mqtt.org/), a lightweight messaging protocol used for small devices). Below are the following topics that this program is listening for
  - `sprinkler/on/{zones}` where `{zones}` is replaced with `1,2,3,...` etc. Turns on watering in the specified zones for the specified time. The time is specified in minutes as a payload in the MQTT message.
  - `sprinkler/off` - shuts off all watering and auto schedules.
  - ~~`sprinkler/auto` - In Development~~
  - ~~`sprinkler/set_schedule` - In Development~~

*`auto` and `set_schedule` will come later in development. I currently use Home Assistant to manage automation. I cover this more in detail in the [**Additional Information Section**](#additional-information)*

---

## Setup
- Create a file in the same directory called "arduino_secrets.h" and populate it with the following information:

    ```c
    #define SECRET_SSID "{YOUR NETWORK NAME HERE}"
    #define SECRET_PASS "{YOUR NETWORK PASSWORD HERE}"
    #define SECRET_BROKER_IP "{YOUR BROKER IP HERE}"
    #define SECRET_BROKER_PORT "{YOUR BROKER PORT HERE}"
    ```
- Modify the `irrigation-controller.ino` file with the following information
  - number of zones 
  - default time of watering
  - changing pin numbers for different configurations

-----

## Additional Information
I use [Home Assistant](https://www.home-assistant.io/) with the [MQTT Plugin](https://www.home-assistant.io/integrations/mqtt/) to trigger this sprinkler system. This allows me to create a "smarter" system with more data to work with. For example, I can create an [Automation](https://www.home-assistant.io/docs/automation/) that checks the weather before deciding to trigger the watering or not.

Below is a sample automation you can use if you wanted to also trigger this system with Home Assistant.

```
- id: {ID}
  alias: Sprinkler Trigger
  description: an automation that is responsible for managing the sprinkler timers.
  trigger:
  - platform: time
    at: 06:00:00
  condition:
  - condition: time
    weekday:
    - sun
  action:
  - service: mqtt.publish
    data:
      topic: {TOPIC_NAME}
      payload: {DURATION}
  mode: single
```

*This is a current project so I will be updating it along the way. There may be things I missed in this README but i'll respond to any questions thrown my way :)*
