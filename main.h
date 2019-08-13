// Internet of Things App
//
// This example code is in the Public Domain.
//
// Unless required by applicable law or agreed to in writing, this
// software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied.
//-------------------------------------------------------------------

#include <iostream>
#include <string>

#include "delay.h"              
#include "nvs_flash.h"
#include "wifi.h"
#include "math.h"
#include "oled.h"
#include "max6675.h"            // temperature sensor
#include "mpu9255.h"            // motion and temperature sensor
#include "driver/adc.h"         // analog inputs

// Profiles for Various Solder Pastes
//
struct Point 
{
    int   time_sec;             // when?
    float C_target;             // target temperature in C
};

struct Profile 
{
    const char *        name;
    const Point *       points;
    int                 point_cnt;     
};

const Point chipquik_lead_free[] = 
{
    {  90, 150 },
    { 180, 175 },
    { 210, 217 },
    { 240, 249 },
    { 270, 217 },
    { 300, 150 },
};

const Profile profiles[] = 
{
    { "ChipQuik Lead-Free Sn96.5/Ag3.0/Cu0.5 (T5)",
      chipquik_lead_free,
      sizeof( chipquik_lead_free ) / sizeof( chipquik_lead_free[0] ) },
};

const int profile_cnt = sizeof( profiles ) / sizeof( profiles[0] );

int main()
{
    // OVEN RELAY   
    int relay_n = 1; 
    gpio_num_t relay_pin = GPIO_NUM_25;
    gpio_set_direction( relay_pin, GPIO_MODE_OUTPUT );
    gpio_set_level( relay_pin, relay_n );

    // I2C OLED
    //             RST          SCL          SDA       Resolution    I2C Addr
    OLED oled( GPIO_NUM_NC, GPIO_NUM_22, GPIO_NUM_21, SSD1306_128x64, 0x3c );
    if ( oled.init() ) {
        oled.clear();
        oled.select_font( 1 );
        oled.draw_string( 0, 0, "Hi, OLED!", WHITE, BLACK );
        oled.refresh( true );
    } 

    // Sensors        CS          SCLK        MISI          MISO
    MAX6675 temp( GPIO_NUM_5, GPIO_NUM_18,              GPIO_NUM_19 );
    #define F( c ) (32.0 + 9.0/5.0*c)

    // Choose Profile
    const Profile& profile = profiles[0];

    float C_start = 25.0;
    float C       = temp.readC();
    float C_max   = C;
    int   t = 0;       // in seconds
    int   t_start = 0; 
    int   p = -1;      // point index
    bool  t_repeated = false;  // repeat last time to wait for temperature to increase
    for( ;; ) 
    {
        // tick one second
        Delay::sec( 1 );
        t++;

        // see if we need to move to the next point 
        if ( p == -1 || t >= profile.points[p].time_sec ) {
            t_start = t - 1;
            p++;
            if ( p == profile.point_cnt ) break;
            if ( p > 0 ) C_start = profile.points[p-1].C_target;
        }

        // read current temperature
        C = temp.readC();
        if ( C > C_max ) C_max = C;

        // Figure out the new slope to use to hit the target temp.
        // Then figure out the target at this point in time.
        float slope   = ( profile.points[p].C_target - C ) / float( profile.points[p].time_sec - t_start );
        float C_t     = float( t - t_start )*slope + C_start;

        std::cout << "\n";

        std::cout << profile.name << "\n";

        std::cout << "Time:     " << t << " secs" << (t_repeated ? " (repeated)" : "") << "\n";
        std::cout << "Current:  ";
        if ( __isnand(C) ) {
            std::cout << "no thermocouple attached!\n";
            break;
        } else {
            std::cout << C << "C  (" << F(C) << "F)\n";
        }
        std::cout << "Target:   " << C_t   << "C  (" << F(C_t)   << "F)\n";

        relay_n = C >= C_t;
        std::cout << "Relay:    " << (relay_n ? "OFF" : "ON") << "\n";
        gpio_set_level( relay_pin, relay_n );

        // repeat time to let temperature catch up?
        t_repeated = C < C_t;
        if ( t_repeated ) t--;
    }

    std::cout << "Max Seen: " << C_max << "C  (" << F(C_max) << "F)\n";
    std::cout << "DONE\n";
    gpio_set_level( relay_pin, 1 ); // OFF
    for( ;; ) {}
    return 0;
}
