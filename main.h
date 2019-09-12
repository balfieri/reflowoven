// Toaster Solder Reflow Oven
//
// This example code is in the Public Domain.
//
// Unless required by applicable law or agreed to in writing, this
// software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied.
//-------------------------------------------------------------------

#include <iostream>
#include <cstdio>
#include <string>

#include "delay.h"              
#include "math.h"
#include "max6675.h"            // temperature sensor
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
    { 240, 249 },  // peak temperature
    { 241,   0 },  // turn off immediately and open the door
//  { 270, 217 },  // can't glide down fast enough, so no point
//  { 300, 150 },
    { 0x7fffffff, 0 }, // stay off for 68 years :-)
};

const Point chipquik_lead[] = 
{
    {  30, 100 },
    { 120, 150 },
    { 150, 183 },
    { 210, 235 },  // peak temperature
    { 211,   0 },  
//  { 240, 183 }, 
//  { 270, 100 },
    { 0x7fffffff, 0 }, // stay off for 68 years :-)
};

const Point kester_lead_free[] = 
{
    {  90, 150 },
    { 180, 205 },
    { 210, 245 },  // peak temperature
    { 211,   0 },
//  { 240, 205 },  
//  { 270, 150 },
    { 0x7fffffff, 0 }, // stay off for 68 years :-)
};

const Point kester_lead[] = 
{
    {  90, 150 },
    { 180, 180 },
    { 210, 220 },  // peak temperature
    { 211,   0 },
//  { 240, 180 },  
//  { 270, 100 },
    { 0x7fffffff, 0 }, // stay off for 68 years :-)
};

const Profile profiles[] = 
{
    { "ChipQuik Lead-Free Sn96.5/Ag3.0/Cu0.5",
      chipquik_lead_free,
      sizeof( chipquik_lead_free ) / sizeof( chipquik_lead_free[0] ) },

    { "ChipQuik Lead Sn63/Pb37",
      chipquik_lead,
      sizeof( chipquik_lead ) / sizeof( chipquik_lead[0] ) },

    { "Kester Lead-Free Sn96.5/Ag3.0/Cu0.5",
      kester_lead_free,
      sizeof( kester_lead_free ) / sizeof( kester_lead_free[0] ) },

    { "Kester Lead Sn63/Pb37",
      kester_lead,
      sizeof( kester_lead ) / sizeof( kester_lead[0] ) },
};

const int profile_cnt = sizeof( profiles ) / sizeof( profiles[0] );

int main()
{
    // OVEN RELAY   
    int relay_n = 1;   // off
    gpio_num_t relay_pin = GPIO_NUM_25;
    gpio_set_direction( relay_pin, GPIO_MODE_OUTPUT );
    gpio_set_level( relay_pin, relay_n );

    // BUZZER
    gpio_num_t buzzer_pin = GPIO_NUM_4;
    gpio_set_direction( buzzer_pin, GPIO_MODE_OUTPUT );
    gpio_set_level( buzzer_pin, 1 );  // off

    // TEMPERATURE SENSOR
    MAX6675 temp( GPIO_NUM_5, GPIO_NUM_18, GPIO_NUM_19 );
    #define F( c ) (32.0 + 9.0/5.0*c)

    // Choose Profile
    const Profile& profile = profiles[1];  // use leaded solder for now

    float C_start = 25.0;
    float C       = temp.readC();
    float C_max   = C;
    int   t = 0;       // in seconds
    int   t_start = 0; 
    int   p = -1;      // point index
    bool  t_repeated = false;   // repeat last time to wait for temperature to increase
    int   t_buzzer_secs_left = 0;
    for( ;; ) 
    {
        // tick one second
        Delay::sec( 1 );
        t++;

        // time to turn off the buzzer?
        if ( t_buzzer_secs_left != 0 ) {
            t_buzzer_secs_left--;
            if ( t_buzzer_secs_left == 0 ) {
                gpio_set_level( buzzer_pin, 0 );  // off
            }
        }

        // see if we need to move to the next point in the profile
        if ( p == -1 || t >= profile.points[p].time_sec ) {
            t_start = t - 1;
            p++;
            if ( p == profile.point_cnt ) break;
            if ( p > 0 ) C_start = profile.points[p-1].C_target;

            // buzz 1 sec for p == 0, 2 sec for p == 1, etc.
            // buzz for 10 sec for last p to indicate that it's time to open the door
            t_buzzer_secs_left = (p == (profile.point_cnt-1)) ? 10 : (p + 1);
            gpio_set_level( buzzer_pin, 1 );  // on
        }

        // read current temperature
        C = temp.readC();
        if ( C > C_max ) C_max = C;

        // Figure out the new slope to use to hit the target temp.
        // Then figure out the target at this point in time.
        float slope   = ( profile.points[p].C_target - C_start ) / float( profile.points[p].time_sec - t_start );
        float C_t     = float( t - t_start )*slope + C_start;

        std::cout << "\n";

        std::cout << "Solder:   " << profile.name << "\n";

        std::cout << "Time:     " << t << " secs" << (t_repeated ? " (repeated)" : "") << "\n";
        std::cout << "Current:  ";
        if ( __isnand(C) ) {
            std::cout << "no thermocouple attached!\n";
            break;
        } else {
            std::printf( "%0.2fC  (%0.2fF)\n", C, F(C) );
        }
        std::printf( "Target:   %0.2fC  (%0.2fF)\n", C_t, F(C_t) );

        relay_n = C >= C_t;
        std::cout << "Relay:    " << (relay_n ? "OFF" : "ON") << "\n";
        std::cout << "Door:     " << ((C_t == 0) ? "OPEN NOW" : "KEEP CLOSED") << "\n";
        gpio_set_level( relay_pin, relay_n );

        // repeat time to let temperature catch up?
        t_repeated = C < C_t;
        if ( t_repeated ) t--;
    }

    std::cout << "Max Seen: " << C_max << "C  (" << F(C_max) << "F)\n";
    std::cout << "DONE\n";
    gpio_set_level( relay_pin, 1 ); // OFF
    for( ;; ) 
    {
        Delay::sec( 3600 );
    }
    return 0;
}
