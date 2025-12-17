// SPDX-License-Identifier: MIT

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <hidapi/hidapi.h>
//#include <unistd.h>

#include "m8mouse.h"
//#define DEBUG_ONLY



enum{
    RUN_ACTION_LIST,
    RUN_ACTION_GET,
    RUN_ACTION_SET,
    RUN_ACTION_USAGE,
    RUN_ACTION_UNKNOWN
};

int cli_debug_level = LOG_FATAL;

int cli_requested_dpi = -1;
int cli_requested_led = -1;
int cli_requested_speed = -1;
int cli_requested_poll_rate = -1;
int cli_requested_dpires_level = -1;
int cli_requested_dpires_value = -1;
int cli_dump_mem = 0;
char *cli_save_profile = NULL;
char *cli_load_profile = NULL;
int cli_requested_brightness = -1;
int cli_raw_addr = -1;
int cli_raw_value = -1;

static FILE *global_logfile = NULL;

void cleanup_logging() {
    if(global_logfile) {
        log_remove_fp(global_logfile);
        fclose(global_logfile);
        global_logfile = NULL;
    }
}



void print_device_state(){
    //log_trace("print_device_state: Printing device state");

    mode *dpi_mode = device_get_active_mode(M8_DEVICE_MODE_DPI);
    if(dpi_mode)
        printf("  %-15s: %s\n", "DPI Mode", dpi_mode->label);
    else
        printf("  DPI Mode is unknown\n");

    printf("  %-15s: ", "DPI Resolution");
    for(int i=0; i<M8_DPI_RES_COUNT; i++){
        mode *dpires_mode = device_get_mode_value(M8_DEVICE_MODE_DPI_RES, i);
        if(dpires_mode)
            printf("DPI %i [%s]", i+1, dpires_mode->label);
        else
            printf("  N/A");
        if(i < (M8_DPI_RES_COUNT - 1))
            printf(", ");
        //print a new line halfway through
        if(i == M8_DPI_RES_COUNT / 2 - 1){
            printf("\n%19s", " ");
        }
    }
    printf("\n");

    mode *led_mode = device_get_active_mode(M8_DEVICE_MODE_LED);
    //int ledindex = m8device.led_mode;
    if(led_mode)
        printf("  %-15s: %s\n", "LED Mode", led_mode->label);
    else
        printf("  LED Mode is unknown\n");

    mode *speed_mode = device_get_active_mode(M8_DEVICE_MODE_SPEED);
    //int speedindex = m8device.led_speed;
    if(speed_mode)
        printf("  %-15s: %s\n", "LED Speed", speed_mode->label);
    else
        printf("  LED Speed is unknown\n");

    mode *poll_mode = device_get_active_mode(M8_DEVICE_MODE_POLL_RATE);
    if(poll_mode)
        printf("  %-15s: %s\n", "Poll Rate", poll_mode->label);
    else
        printf("  Poll Rate: unknown (may not be supported)\n");

    mode *bright_mode = device_get_active_mode(M8_DEVICE_MODE_BRIGHTNESS);
    if(bright_mode)
        printf("  %-15s: %s\n", "LED Brightness", bright_mode->label);
    else
        printf("  LED Brightness: unknown\n");
}

void print_single_mode(char *label, mode* curr){
    
    char buffer[512];
    char *pointer = buffer;
    int line = 0, letter = 0, width = 64;

    pointer = buffer;
    pointer += sprintf(pointer, "  %-16s [", label);
    for(int i = 1; curr->label; i++, curr++){
        //print a new line if it gets too long
        letter = pointer - buffer;
        if((letter / width) > line){
            pointer += sprintf(pointer, "\n%20s", " ");
            line++;
        }

        pointer += sprintf(pointer, "%s, ", curr->label);
    }
    sprintf(pointer - 2, "]\n");
    printf("%s", buffer);
}

void print_modes(){
    
    
    printf("Known modes\n");
    
    mode *curr;

    curr = device_get_all_modes(M8_DEVICE_MODE_DPI);
    print_single_mode("DPI modes", curr);

    curr = device_get_all_modes(M8_DEVICE_MODE_DPI_RES);
    print_single_mode("DPI resolution", curr);

    curr = device_get_all_modes(M8_DEVICE_MODE_LED);
    print_single_mode("LED modes", curr);
    
    curr = device_get_all_modes(M8_DEVICE_MODE_SPEED);
    print_single_mode("LED speeds", curr);
    
    curr = device_get_all_modes(M8_DEVICE_MODE_POLL_RATE);
    print_single_mode("Poll rates", curr);
    
    curr = device_get_all_modes(M8_DEVICE_MODE_BRIGHTNESS);
    print_single_mode("Brightness", curr);
    
}


void print_usage(){
    printf("Usage: \n"
    "    m8mouse \n"
    "    m8mouse -l \n"
    "    m8mouse [-dpi D | -led L | -speed S | -poll P | -dpires L:R | -bright B]\n"
    "    m8mouse -save <file>    # save current config to file\n"
    "    m8mouse -load <file>    # load config from file and apply\n"
    "    \n"
    "    Options: \n"
    "       -l        list known modes and values\n"
    "       -dpi      set DPI to this index (from known modes) \n"
    "       -dpires   set DPI resolution for level L to resolution R (e.g. -dpires 1:8)\n"
    "       -led      set LED mode to this index (from known modes) \n"
    "       -speed    set LED speed to this index (from known modes) \n"
    "       -bright   set LED brightness (1=Full, 2=Half)\n"
    "       -poll     set polling rate (1=1000Hz, 2=500Hz, 3=250Hz, 4=125Hz)\n"
    "       -save     save current device config to a profile file\n"
    "       -load     load a profile file and apply to device\n"
    "       -raw      set raw memory byte at ADDR:VALUE (hex, e.g. -raw 30:05)\n"
    "       -dump     dump device memory (for debugging)\n"
    "       -g        print debug messages\n"
    "       -h        help message (this one)\n"
    "\n");
}

    
int process_args(int argc, char *argv[]){
    int arg_index = 1;
    int run_action = RUN_ACTION_GET;
    
    while(arg_index < argc){
        char *option   = argv[arg_index];
        char *argument = "";

        if(arg_index + 1 < argc){
            argument = argv[arg_index + 1];
        }

        if(!strcmp(option, "-h")){
            return RUN_ACTION_USAGE;
        }else if(!strcmp(option, "-g")){
            cli_debug_level = LOG_WARN;
        }else if(!strcmp(option, "-g1")){
            cli_debug_level = LOG_INFO;
        }else if(!strcmp(option, "-g2")){
            cli_debug_level = LOG_TRACE;
        }else if(!strcmp(option, "-l")){
            return RUN_ACTION_LIST;
        }else if(!strcmp(option, "-dump")){
            cli_dump_mem = 1;
        }else if(!strcmp(option, "-dpi")){
            if(strlen(argument) > 0)
                cli_requested_dpi = atoi(argument) - 1;
            run_action = RUN_ACTION_SET;
            arg_index++;
        }else if(!strcmp(option, "-dpires")){
            if(strlen(argument) > 0){
                char *colon = strchr(argument, ':');
                if(colon){
                    *colon = '\0';
                    cli_requested_dpires_level = atoi(argument) - 1;
                    cli_requested_dpires_value = atoi(colon + 1) - 1;
                    *colon = ':';
                }else{
                    printf("Error: -dpires requires L:R format (e.g. -dpires 1:8)\n");
                    return RUN_ACTION_UNKNOWN;
                }
            }
            run_action = RUN_ACTION_SET;
            arg_index++;
        }else if(!strcmp(option, "-led")){
            if(strlen(argument) > 0)
                cli_requested_led = atoi(argument) - 1;
            run_action = RUN_ACTION_SET;
            arg_index++;
        }else if(!strcmp(option, "-speed")){
            if(strlen(argument) > 0)
                cli_requested_speed = atoi(argument) - 1;
            run_action = RUN_ACTION_SET;
            arg_index++;
        }else if(!strcmp(option, "-poll")){
            if(strlen(argument) > 0)
                cli_requested_poll_rate = atoi(argument) - 1;
            run_action = RUN_ACTION_SET;
            arg_index++;
        }else if(!strcmp(option, "-bright")){
            if(strlen(argument) > 0)
                cli_requested_brightness = atoi(argument) - 1;
            run_action = RUN_ACTION_SET;
            arg_index++;
        }else if(!strcmp(option, "-save")){
            if(arg_index + 1 < argc && strlen(argument) > 0) {
                cli_save_profile = argument;
                arg_index++;
            } else {
                printf("Error: -save requires a filename argument\n");
                return RUN_ACTION_UNKNOWN;
            }
        }else if(!strcmp(option, "-load")){
            if(arg_index + 1 < argc && strlen(argument) > 0) {
                cli_load_profile = argument;
                run_action = RUN_ACTION_SET;
                arg_index++;
            } else {
                printf("Error: -load requires a filename argument\n");
                return RUN_ACTION_UNKNOWN;
            }
        }else if(!strcmp(option, "-raw")){
            if(strlen(argument) > 0){
                char *colon = strchr(argument, ':');
                if(colon){
                    *colon = '\0';
                    char *endptr_addr = NULL;
                    char *endptr_value = NULL;
                    cli_raw_addr = (int)strtol(argument, &endptr_addr, 16);
                    cli_raw_value = (int)strtol(colon + 1, &endptr_value, 16);
                    *colon = ':';
                    if (argument[0] == '\0' || *endptr_addr != '\0' || (colon + 1)[0] == '\0' || *endptr_value != '\0') {
                        printf("Error: -raw requires valid hex ADDR:VALUE (e.g. -raw 30:05)\n");
                        return RUN_ACTION_UNKNOWN;
                    }
                }else{
                    printf("Error: -raw requires ADDR:VALUE format in hex (e.g. -raw 30:05)\n");
                    return RUN_ACTION_UNKNOWN;
                }
            }
            run_action = RUN_ACTION_SET;
            arg_index++;
        }else{
            return RUN_ACTION_UNKNOWN;
        }
        arg_index++;
    }
    
    if(run_action == RUN_ACTION_SET && 
        (cli_requested_dpi == -1 && cli_requested_led == -1 && cli_requested_speed == -1 &&
         cli_requested_poll_rate == -1 && cli_requested_dpires_level == -1 && 
         cli_requested_brightness == -1 && cli_raw_addr == -1 && cli_load_profile == NULL))
        run_action = RUN_ACTION_UNKNOWN;
    
    return run_action;
}

int main(int argc, char *argv[]){
    
    int run_action = process_args(argc, argv);
    
    if(run_action == RUN_ACTION_LIST){
        print_modes();
        return 0;
    }else if(run_action == RUN_ACTION_USAGE || run_action == RUN_ACTION_UNKNOWN){
        print_usage();
        return 1;
    }

    //initialise logs
    log_set_level(cli_debug_level);
    if(cli_debug_level == LOG_TRACE){
        log_set_level(LOG_ERROR);
        time_t timenow = time(NULL);
        struct tm *tm_now_info = localtime(&timenow);
        char timestamp_buff[64];
        strftime(timestamp_buff, sizeof(timestamp_buff), "%Y%m%d-%H%M%S", tm_now_info);
        char filename_buff[64];
        sprintf(filename_buff, "m8debug-%s.log", timestamp_buff);
        global_logfile = fopen(filename_buff, "a");
        if(global_logfile){
            fprintf(global_logfile, "\n=================================\n");
            log_add_fp(global_logfile, LOG_TRACE);
            atexit(cleanup_logging);
        } else {
            log_warn("Failed to open debug log file %s", filename_buff);
        }
        //log_set_quiet(1);
        log_set_level(LOG_INFO);
    }
    
        
    if(device_init()){
        printf("Error initialising device. May not be connected or no user permission\n");
        printf("      - check that device %04x:%04x is connected to usb (lsusb)\n", USB_M8_VID, USB_M8_PID);
        printf("      - run with sudo or add uaccess to udev rules (see README.md)\n");
        cleanup_logging();
        return 1;
    }
    
    puts("Getting device modes");
    device_query();
    //print_device_mem();
    print_device_state();

    if(cli_dump_mem){
        puts("\nDevice memory dump:");
        device_dump_mem();
    }

    if(cli_save_profile){
        printf("Saving profile to %s\n", cli_save_profile);
        if(device_save_profile(cli_save_profile)){
            printf("Error: Failed to save profile\n");
        }else{
            printf("Profile saved successfully\n");
        }
    }

    if(run_action == RUN_ACTION_SET){
        int set_result = 0;
        
        if(cli_load_profile){
            printf("Loading profile from %s\n", cli_load_profile);
            if(device_load_profile(cli_load_profile)){
                printf("Error: Failed to load profile\n");
                set_result = 1;
            }else{
                printf("Profile loaded, applying to device...\n");
            }
        }
        
        set_result |= device_set_modes(cli_requested_dpi, cli_requested_led, cli_requested_speed);

        if(cli_requested_dpires_level >= 0 && cli_requested_dpires_value >= 0){
            set_result |= device_set_dpires(cli_requested_dpires_level, cli_requested_dpires_value);
        }
        
        if(cli_requested_poll_rate >= 0){
            set_result |= device_set_poll_rate(cli_requested_poll_rate);
        }
        
        if(cli_requested_brightness >= 0){
            set_result |= device_set_brightness(cli_requested_brightness);
        }
        
        if(cli_raw_addr >= 0 && cli_raw_value >= 0){
            printf("Setting raw memory: address 0x%02x = 0x%02x\n", cli_raw_addr, cli_raw_value);
            set_result |= device_set_raw(cli_raw_addr, (uint8_t)cli_raw_value);
        }
        
        if(!set_result){
            puts("Updating device modes");
            device_update();
            //verify by querying again
            puts("Refreshing device modes");
            device_query();
            print_device_state();
        }
    }
    
    device_shutdown();
    
    cleanup_logging();
    return 0;
}
