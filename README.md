# Irrigation Controller
An irrigation controller system designed to run on an an **Arduino UNO WIFI Rev 2**

It's a pretty simple system and i've tried keeping it organised but heres a quick breakdown I will update as things are added

## Core Functionality
- Ability to control the states with **Auto (PIN 8)** and **Manual (PIN 7)** [**buttons**](https://www.sparkfun.com/products/9190)
- An LED light [**WS2812B**](https://www.sparkfun.com/products/13282) on pin 4 used to indicate state of the system at any point
- Ability to controll over the internet by calling the ip the arduino is connected to with one of the following endings 
  - /on/{zone}/{watering time in min}
    - triggers the system on for the desired time in the desired zone
  - /off
    - turns the system off and turns auto off
  - /auto
    - Sets the system to turn on at 9PM local time

## Setup
- Create a file in the same directory called "arduino_secrets.h" and populate it with the following information:

    ```c
    #define SECRET_SSID "{YOUR NETWORK NAME HERE}";
    #define SECRET_PASS "{YOUR NETWORK PASSWORD HERE}";
    ```
- Modify the `irrigation-controller.ino` file with the following information
  - number of zones 
  - default time of watering
  - changing pin numbers for different configurations


-----
*This is a current project so I will be updating it along the way. There may be things I missed in this README but i'll respond to any questions thrown my way :)*
