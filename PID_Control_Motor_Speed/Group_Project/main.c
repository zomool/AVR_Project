/*
 * Group Project.c
 *
 * Created: 5/4/2024 11:05:04 PM
 * Author : Nagy
 */

#define F_CPU 1000000UL  // Define CPU frequency as 1 MHz
#include <avr/io.h>
#include <util/delay.h>
#define setbit(port,bit) (port) |= (1<<(bit))  // Macro to set a bit
#define clearbit(port,bit) (port) &= ~(1<<(bit))  // Macro to clear a bit
#include <avr/interrupt.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define Kp    1
#define Ki	  0.1
#define Kd	  0.05


int pwm = 0;  // PWM value
volatile uint16_t pulse_count = 0;  // Pulse count for encoder
volatile uint16_t timer0_overflow_count = 0;  // Timer0 overflow count
volatile double integral      = 0;
volatile double prev_error    = 0;
char str[50];  // String buffer for LCD

// Function to send command to LCD
void lcd_cmnd(unsigned char cmnd){
    PORTB = cmnd;  // Put command on PORTB
    PORTD = 0b01000000;  // Set RS to 0 (command mode) and E to 1 (enable)
    _delay_us(1);  // Short delay
    clearbit(PORTD,6);  // Clear E (disable)
    _delay_us(100);  // Short delay
}

// Function to initialize LCD
void lcd_init(){
    DDRB = 0xff;  // Set PORTB as output
    DDRD |= (1<<4) | (1<<5) | (1<<6);  // Set PD4, PD5, PD6 as output
    lcd_cmnd(0x38);  // Initialize LCD in 8-bit mode
    lcd_cmnd(0x0E);  // Display ON, cursor ON
    lcd_cmnd(0x01);  // Clear display
    _delay_ms(2);  // Delay for command execution
    lcd_cmnd(0x06);  // Increment cursor
}

// Function to send data to LCD
void lcd_data(unsigned char data){
    PORTB = data;  // Put data on PORTB
    PORTD = 0b01010000;  // Set RS to 1 (data mode) and E to 1 (enable)
    _delay_us(1);  // Short delay
    clearbit(PORTD,6);  // Clear E (disable)
    _delay_us(100);  // Short delay
}

// Function to print string on LCD
void print(char* str){
    int i = 0;
    while(str[i] != 0 ){
        lcd_data(str[i]);  // Send each character to LCD
        i++;
    }
}

// Function to initialize motor control
void motor_init(){
    TCCR2 = (1<<WGM01) | (1<<CS01) | (1<<COM01) | (1<<WGM00) | (1<<CS00);  // Configure Timer2 for PWM
    setbit(DDRD,7);  // Set PD7 as output (OC2 pin)
}

int main(void)
{
    lcd_init();  // Initialize LCD
    motor_init();  // Initialize motor control
    print("Allah Akbar");  // Display initial message on LCD

    uint16_t pulses_per_second = 0;  // Pulses per second
    uint16_t motor_speed = 0;  // Motor speed in RPM (Revolutions Per Minute)
    const uint16_t pulses_per_revolution = 20;  // Encoder pulses per revolution
    const uint16_t overflow_limit = 488;  // Overflow limit for 1 second (256 * 8 / 1,000,000 = 2.048 ms | 1000 / 2.048 ms = 488)

    // Configure PD2 and PD3 as input for interrupts
    clearbit(DDRD, PD2);
    clearbit(DDRD, PD3);
    setbit(PORTD, PD2);  // Enable pull-up resistor on PD2
    setbit(PORTD, PD3);  // Enable pull-up resistor on PD3

    // Configure PD0 and PD1 as motor control pins
    setbit(DDRD, PD0);
    setbit(DDRD, PD1);

    // Configure PA0 as input for encoder
    clearbit(DDRA, PA0);
    setbit(PORTA, PA0);  // Enable pull-up resistor on PA0

    MCUCR = 0x0A;  // Configure INT0 and INT1 for falling edge triggering
    GICR = (1<<INT0) | (1<<INT1);  // Enable INT0 and INT1 interrupts
    setbit(TCCR0, CS01);  // Set Timer0 prescaler to 8
    setbit(TIMSK, TOIE0);  // Enable Timer0 overflow interrupt
    sei();  // Enable global interrupts

    int encoder_state = 0;
    int last_encoder_state = 0;

    while(1){
        encoder_state = PINA & (1<<PA0);  // Read encoder state
        if ((encoder_state != 0) && (last_encoder_state == 0)) {  // Detect rising edge
            pulse_count++;  // Increment pulse count
        }
        last_encoder_state = encoder_state;  // Update last encoder state

        if (timer0_overflow_count >= overflow_limit) {  // Check if overflow limit is reached (1 second)
            pulses_per_second = pulse_count;  // Update pulses per second
            pulse_count = 0;  // Reset pulse count
            timer0_overflow_count = 0;  // Reset overflow count
            motor_speed = (pulses_per_second * 60) / pulses_per_revolution;  // Calculate motor speed in RPM

            lcd_cmnd(0x01);  // Clear LCD before printing new value
            print(" rpm: ");  // Print "rpm: " on LCD
            sprintf(str, "%u", motor_speed);  // Convert motor speed to string
			
			double error      = pwm - motor_speed;
			integral         += error;
			double derivative = error - prev_error;
			double output     = Kp*error + Ki*integral + Kd*derivative;
			pwm = output;
			prev_error = error;
            print(str);  // Print motor speed on LCD
        }

        if (pwm > 0) {  // If PWM is positive
            setbit(PORTD, PD1);  // Set PD1 (forward direction)
            clearbit(PORTD, PD0);  // Clear PD0
            OCR2 = pwm;  // Set PWM value
        }
        else if (pwm < 0) {  // If PWM is negative
            setbit(PORTD, PD0);  // Set PD0 (reverse direction)
            clearbit(PORTD, PD1);  // Clear PD1
            OCR2 = abs(pwm);  // Set PWM value to absolute of pwm
        }
        else {  // If PWM is zero
            clearbit(PORTD, PD1);  // Clear PD1
            clearbit(PORTD, PD0);  // Clear PD0
            OCR2 = 0;  // Set PWM value to zero
        }
    }
    return 0;  // Return from main
}

// Interrupt Service Routine for INT0
ISR (INT0_vect){
        pwm = pwm + 25;  // Increase PWM by 25
        if (pwm > 255)  // Limit PWM to maximum value of 255
        pwm = 255;
}

// Interrupt Service Routine for INT1
ISR (INT1_vect){
        pwm = pwm - 25;  // Decrease PWM by 25
        if (pwm < -255)  // Limit PWM to minimum value of -255
        pwm = -255;
}

// Interrupt Service Routine for Timer0 overflow
ISR(TIMER0_OVF_vect) {
        timer0_overflow_count++;  // Increment overflow count
}
